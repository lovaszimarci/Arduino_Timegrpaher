#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>
#include "LCD_I2C.h"
#include "HW_INIT.h"
#include "UART.h"


//======================================
//GLOBALIS VALTOZOK A TIMER INTERRUPTHOZ
//======================================
volatile uint16_t SpikeTimerValue;
volatile bool SpikeFlag = false;


//===================================
// KORPUFFER ADATAINAK INICIALIZALASA
//===================================
uint16_t puffer[8] = {0,0,0,0,0,0,0,0};
uint8_t puffer_index = 0; // ez a valtozo koveti az aktualis helyet az ertekekeknek
uint32_t running_sum = 0; // ez az osszeg teszi lehetove hogy ne keljen mozgatni adatokat csak kivonni es hozzaadni
bool PufferIsFull = false; // a kezdeti fazis jelzesehez hasznalt valtozo, true --> meg toltodik a puffer


//=============================================
//STATISZTIKAI ADATOK VALTOZOINAK INICIALIZALAS
//=============================================

#define AVERAGE_COUNT 8 // 12 utesenkent kuldi ki a program az lcd-re az infot
uint8_t tick_count = 0;

uint32_t Average_deltaT; // a meresbol szamolt atlag delta t ertek timer lepesben
uint32_t Average_deltaT_us; // a meresbol szamolt atlag delta t ertek us-ben

uint16_t ReferenceBph; // a user altal terminalban megadott referencia bph ertek
float Reference_deltaT_us; // a referencia bph ertekbol kapunk egy referencia delta t erteket, ennek kesobb ad erteket a program

float rate_sum =0.0; // az elteresek osszege
float beat_error_sum = 0.0; // a beat errorok osszege



//=====================================
//SYNC FAZIS VALTOZOINAK INICIALIZALASA
//=====================================
bool SYNC_Starting = true; // true --> meg nem volt elozo hangtuske amihez viszonyitana, false --> mar volt elozo hangtuske
uint16_t SYNC_previousT; // az elozo hangtuske timer erteke
uint16_t SYNC_localT;// lokalis valtozo a timer ertekenek tarolasara
uint16_t SYNC_deltaT; // bejovo ket tuske kozotti ido (delta t)



//================================
// ALLAPOTGEP ALLAPOTAINAK LEIRASA
//================================
typedef enum{
    SYNC = 1, // egy atmeneti allapot a rosszul erzekelt tuskek kiszurasara, es a delta ido ertek kiszamitasara
    BLANK_PERIOD, // itt ha mar a SYNC allpot validalta a tusket egy suket idoszakot valositunk meg hogy a viszhangok ne erzekelodjenek
    PROCESSING, // itt dolgozza fel az ertekeket a rendszer statisztikava es kuldi ki az lcd re az ertekeket
} State;

State GlobalState = SYNC; // allapot tarolo


//=============================
// TIMER INTERRUPT MEGVALOSITAS
//=============================

ISR(TIMER1_CAPT_vect){

    SpikeTimerValue = ICR1; //timer érték mentése
    SpikeFlag = true; // új tüske jelzése, tüske utáni jelérzékelés tiltásának állapota
    TIMSK1 &= ~(1 << ICIE1);// timer megszakitas tiltasa

}




int main(){

    //=========================================
    //HW, UART ES I2C LCD KIJELZO INICIALIZALAS
    //=========================================

    I2C_init(); // I2C kommunikaciohoz hasznalt folyamatok es regiszterek elokeszitese
    LCD_Init(); // LCD kijelzo inicializalasa
    UART_Init(); // UART kommunikaciohoz hasznalt folyamatok es regiszterek elokeszitese

    ReferenceBph = UART_AskForBph(); // referencia bph ertek bekerese a usertol uart kommunikacioval terminalon keresztul
    // Ha a Target_BPH = 21600, akkor 1 oraban (3600 mp -> 3.600.000.000 us) van ennyi ütés.
    Reference_deltaT_us = 3600000000.0 / ReferenceBph; // a referencia bph ertekbol meghatarozott idokoz ket utes kozott, ez az elerndo cel

    SetupTimer(); //Timer felprogramozasa a megfelelo mukodesre
    SetupComp(); // annalog komparator felprogramozasa a megfelelo mukodesre



//===============================================
//While loop kezdete, itt kezd el merni a program
//===============================================
    while(true){
        switch(GlobalState){

            //=============================
            //SYNC FAZIS MEGVALOSITASA
            //=============================

            case SYNC:

                if(SpikeFlag){ // ha a timer jelzett hogy van uj tuske akkor kezeljuk
                    cli(); // ameddig atmentjuk a regisztert, a megszakitasokat letiltjuk
                    SYNC_localT = SpikeTimerValue; // timer ertekenek lokalis valtozoba pakolasa
                    sei();

                    SpikeFlag = false; // uj tuske jelzeseneke hamisba allitasa

                    if(SYNC_Starting){ //meg nem volt elozo tuske ertek igy nem tud delta ido erteket szamolni
                        //================================
                        //KEZDETI FAZIS ELOZO TUSKE NELKUL
                        //================================
                        SYNC_previousT = SYNC_localT; // elozo tuske ertek beallitasa
                        SYNC_Starting  = false; // a kovetkezo tuskenel mar lesz elozo tuske, nincs kezdeti fazis
                        GlobalState = BLANK_PERIOD; //  kovetkezo fazis beallitasa
                    }
                    else{

                        SYNC_deltaT = SYNC_localT - SYNC_previousT;// ket utes kozotti ido - meg nem biztos hogy valid utes

                        //MEGJEGYZES:
                                    //deltaT validalas
                                    // min: 80ms max: 320ms (ezek az ertekeke mar a hibahatarral egyuttiek)
                                    // 1 timer lepes 0.016 ms
                                    // 80ms = 5000 timer lepes
                                    // 320ms = 20000 timer lepes
                                    // ha nagyon hamar jott utana uj tuske akkor valoszinuleg zajt erzekelt
                                    // ha nagyon keson jott utana akkor megint vagy zaj vagy levettek az orat

                        if(SYNC_deltaT > 5000 && SYNC_deltaT < 20000){ // idobeli hatarok ellenorzese delta t ertekre
                            //=========================
                            //VALID IDOKULONBSEG
                            //=========================
                            SYNC_previousT = SYNC_localT;
                            GlobalState = PROCESSING;
                        }
                        else{
                            //========================
                            //ERVENYTELEN IDOKULONBSEG
                            //========================
                            if(SYNC_deltaT > 20000){
                                // kihagyott egy utest a gep
                                // kotelezo frissiteni az elozo utest hogy friss helyrol induljon
                                SYNC_previousT = SYNC_localT;
                            }
                            else{
                                //Kisebb volt a detla t ertek mint 5000
                                // valoszinuleg zaj volt --> nem frissitjuk az elozo tuske erteket
                                GlobalState = BLANK_PERIOD;
                            }

                        }
                    }
                }
                break;
            //================================
            //BLANK PERIOS FAZIS MEGVALOSITASA
            //================================
            case BLANK_PERIOD:

            cli(); // timer ertek masolashoz letiltom a megszakitasokat
            uint16_t CurrentTime = TCNT1; // a jelenlegi ido timer erteke
            sei();

            uint16_t PassedTime = CurrentTime - SYNC_previousT; // eltelt ido szamitasa

            // 60ms =  3750 timer step
            if(PassedTime > 3750){

                SpikeFlag = false; // letorolhetjuk a hangtuske flagjet mert letelt az ido
                TIFR1 = (1<<ICF1); // timer input capture flagjenek torlese regiszterbol
                TIMSK1 |= (1<<ICIE1); // timer interupt ujraengedelyezese
                GlobalState = SYNC; // vissza mehet a program sync allapotba

            }
            break;

            //==============================
            //PROCESSING FAZIS MEGVALOSITASA
            //==============================

            case PROCESSING:


                if(PufferIsFull == false){ // puffer meg nincs tele, feltolto folyamat kezelese
                    puffer[puffer_index] = SYNC_deltaT;
                    running_sum += SYNC_deltaT;
                    puffer_index += 1;
                    if(puffer_index == 8){
                        puffer_index = 0;
                        PufferIsFull = true;
                    }
                }
                else{
                    // itt mar fel van toltve a korpuffer es csak frissiteni kell

                    running_sum -= puffer[puffer_index];
                    puffer[puffer_index] = SYNC_deltaT;
                    running_sum += SYNC_deltaT;
                    puffer_index += 1;
                    if(puffer_index == 8){
                        puffer_index = 0;
                    }

                    //===========================================
                    // NAPI ELTERES SZAMITAS
                    //===========================================

                    Average_deltaT = running_sum >> 3; // korpuffer altal kezelt runing sum osztasa 8-al, shifteleses megoldassal
                    Average_deltaT_us = Average_deltaT * 16;  // az atlag elteres idore valtasa timer lepesrol

                    //napi atlag elteres szamitasa:
                    // ((gyari referencia idokoz - sajat atlagolt idokoz)/ gyari referencia idokoz) * egy nap masodpercekben
                    float Current_rate = ((Reference_deltaT_us - Average_deltaT_us)/Reference_deltaT_us)*86400.0;

                    tick_count++; // noveljuk a kattanas szamolo erteket, 8 kattanasonkent iratunk ki

                    if(tick_count >= AVERAGE_COUNT){

                        //==========================================
                        // BEAT ERROR SZAMITAS
                        //==========================================
                        for(int i = 1; i < 8;i++ ){
                            uint32_t diff_ticks = (puffer[i] > puffer[i-1]) ?
                                            (puffer[i] - puffer[i-1]) :
                                            (puffer[i-1] - puffer[i]);

                            float diff_ms = diff_ticks * 0.016;
                            beat_error_sum += diff_ms;
                        }


                        float Average_rate = Current_rate; // az adott korpuffer atlag lesz a kiirando adat
                        float Average_beat_error = (beat_error_sum / 7.0); // azert csak hettel osztunk mert a 8 elemu pufferben csak 7 hiba ertek van,
                       // (matematikailag igy jon ki)




                       //================================================
                       //ADATOK TOVABBITASA LCD KIJELZORE
                       //================================================

                        char line1[17]; // LCD kijelzo 1. soranak puffere
                        char line2[17]; // LCD kijelzo 2.  soranak puffere

                        char rate_str[10]; // atmeneti puffer
                        dtostrf(Average_rate, 4, 1, rate_str); // mivel a sima sprintf uC-n nem tud float erteket stringe konvertalni,
                        //ezert egy direkt uC-re kifejlesztett fugvennyel helyettesitettem

                        char beat_error_str[10];
                        dtostrf(Average_beat_error, 3, 1, beat_error_str);

                        char plusminus = (Average_rate >= 0) ? '+' : '\0'; // napi elteres elojelenek meghatarozasa
                        sprintf(line1, "Rate:%c%s s/d", plusminus, rate_str);

                        sprintf(line2, "B.err: %s ms", beat_error_str);



                        //LCD kiiras
                        // (az lcd fugvenyek leirasi az adott header fajlban kommmentben szerepelnek)
                        LCD_SetCursor(0, 0);
                        LCD_PrintString(line1);
                        LCD_SetCursor(1, 0);
                        LCD_PrintString(line2);

                        //nullazas
                        tick_count = 0;
                        beat_error_sum = 0.0;

                    }

                }

                GlobalState = BLANK_PERIOD;
                break;

        }
    }
}
