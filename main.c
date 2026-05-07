#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#define avarage_size 8


volatile uint16_t SpikeTimerValue;
volatile bool SpikeFlag = false;


//puffer adatok
uint16_t puffer[8];
uint8_t puffer_index;
uint32_t running_avarage;


// SYNC fazis flages es valtozok
bool SYNC_Starting = true;
uint16_t SYNC_previousT;
//uint16_t SYNC_presentT;
// lokalis valtozo az ido atmeneti tarolasara
uint16_t SYNC_localT;
uint16_t SYNC_deltaT;




typedef enum{
    SYNC = 1,
    /*
     feladat: megtalalni a legelso tiszta tusket a zajban
     ha jon egy tuske meg kell nezni hogy mennyi ido telt el az elozo es az uj ota
     ha tobb mint 80ms akkor valoszinuleg egy uj valid tuske nem pedig egy viszhang vagy egy random zaj
     ha validnak talalta az algoritmus akkor a state BLANK_PERIOD LESZ
     */
    BLANK_PERIOD,// mute ACSR |= (1 << ACD); unmute ACSR &= ~(1 << ACD);
    WAITING_FOR_NEW_SPIKE,
    PROCESSSING,
} State;

State GlobalState = SYNC;



ISR(TIMER1_CAPT_vect){
    //timer érték mentése
    SpikeTimerValue = ICR1;
    // új tüske jelzése, tüske utáni jelérzékelés tiltásának állapota
    SpikeFlag = true;
    // timer megszakitas tiltasa
    TIMSK1 &= ~(1 << ICIE1);
}

void SetupTimer(){
/*

 TCCR1A (timer/counter control register a)
 7      6       5       4       3       2       1       0
 COM1A1 COM1A0  COM1B1  COM1B0  -       -     WGM11     WGM10
 0      0       0       0       0       0       0       0 --> setup mask



 TCCR1B (timer/counter control register b)
 7      6       5       4       3       2       1       0
 ICNC1 ICES1    -     WGM13   WGM12    CS12    CS11    CS10
 0      0       0       0       0       1       0       0

 előosztás: 1 0 0

 */
 TCCR1B |= (1<<CS12);
 /*
 TIMSK1 (timer/counter interrupt mask register)


 7      6       5       4       3       2       1       0
 -      -     ICIE1     -       -     OCIE1B  OCIE1A  TOIE1
 0      0       1       0       0       0       0       0
 (icie1 input capture interrupt enable, )
 */
 TIMSK1 |= (1<<ICIE1);
 /*
 ICR1 --> csak olvasni, itt van az input capture értéke


--------------------------------------------------------------
16bit számláló max érték = 65536
Fclk = 16Mhz = 16 000 000 Hz
prescale = 265
--> 16000000/265 = 62500Hz --> egy másodperc alatt 62500-at lép a timer
1 timer lépés = 0,000016s = 0.016 milisec = 16 mikrosec

Tmax = 65536 x 0.000016s = 1.048576s

Valós idő kiszámítása timer értékből

Idő(milisec) = timerérték x 0.016
 TCCR1C (timer/counter control register c)

 // Tegyük fel, hogy az algoritmusod kiszámolta a két tüske közötti különbséget:
 uint16_t timer_lepesek = 10416; // (Ez csak egy példa érték)

 // 1. Lépés: Átváltás mikroszekundumba (Biztonságos, 32-bites egész számmal)
 // A 32 bit (uint32_t) azért kell, mert 65535 * 16 már nem férne el 16 biten!
 uint32_t eltelt_ido_us = (uint32_t)timer_lepesek * 16;

 // 2. Lépés: Átváltás milliszekundumba (Itt már jöhet a tört szám, azaz a float)
 float eltelt_ido_ms = (float)eltelt_ido_us / 1000.0;

 // Ezt az értéket már ki is küldheted a Serial Monitorra!
 // Ki fogja írni: "166.65 ms"
 --------------------------------------------------------------
 */
}

void SetupComp(){

/*
ADCSRB --> default 0

ACSR (analog comparator control and status register)
7       6       5       4       3       2       1       0
ACD   ACBG     ACO     ACI     ACIE    ACIC   ACIS1   ACIS0
0       0       N/A     0       1       1       1       0
ACD --> default 0 viszont a süketítéshez ezt kell majd használni talán

acis1/0 =10 --> lefutó él

*/

ACSR |= (1<<ACIE);
ACSR |= (1<<ACIC);
ACSR |= (1<<ACIS1);

/*


DIDR1 (digital input disable register)

7       6       5       4       3       2       1       0
-       -       -       -       -       -     AIN1D   AIN0D
0       0       0       0       0       0       1       1

(letiltja a digitális bemenetet és csak analog jelet dolgozza fel a pin)
 */

 DIDR1 |= (1<<AIN1D);
 DIDR1 |= (1<<AIN0D);
}

void Setup(){

}

int main(){
    //hw setup
    SetupTimer();
    SetupComp();

    while(true){
        switch(GlobalState){
            // SYNC fázis
            case SYNC:
                //ha van uj tuske
                if(SpikeFlag){
                    cli();
                    // timer lokalis valtozoba pakolasa
                    SYNC_localT = SpikeTimerValue;
                    sei();

                    // uj tuske jelzeseneke hamisba allitasa
                    SpikeFlag = false;

                    if(SYNC_Starting){
                        //init fazisban van a rendszer, meg nem tud deltaT-t szamolni
                        SYNC_previousT = SYNC_localT;
                        SYNC_Starting  = false;
                        GlobalState = BLANK_PERIOD;
                    }
                    else{
                        // ket utes kozotti ido - meg nem biztos hogy valid utes
                        SYNC_deltaT = SYNC_localT - SYNC_previousT;

                        //deltaT validalas
                        // ora delta ido ms ben
                        // min: 80ms max: 320ms - ezekre mar ra lett szamitva hibahatar
                        // 1 timer lepes 0.016 ms
                        // 80ms = 5000 timer lepes
                        // 320ms = 20000 timer lepes


                        if(SYNC_deltaT > 5000 && SYNC_deltaT < 20000){
                            //todo korpuffer vagy feldolgozo fazisnak tovabbitani deltaT erteket
                            SYNC_previousT = SYNC_localT;
                            GlobalState = BLANK_PERIOD;
                        }
                        else{
                            GlobalState = BLANK_PERIOD;
                        }
                    }
                }
            case BLANK_PERIOD:

        }
    }

}

//      usart communication
//
//
