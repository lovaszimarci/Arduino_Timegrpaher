#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

// LCD es I2C makrok
/*
 I2C csomag bitek lcd funkcio szerint:

 bit 7,6,5,4 adat bitek
 bit 3 hattervilagitas ==> 1 vilagit, 0 nem
 bit 2 enable ==> lcd-nek jelzes hogy olvassa be a biteket
 bit 1 read/wrie  ==> mivel csak irni akarunk mindig 0
 bit 0 register select ==> 0 akkor parancs, 1 akkor karakter
 */

 #define LCD_ADDR (0x27<<1)
 //PCF8574T kiosztas
 #define LCD_BACKLIGHT 0x08  // Bit 3 (0b00001000)
 #define LCD_ENABLE    0x04  // Bit 2 (0b00000100)
 #define LCD_RW        0x02  // Bit 1 (0b00000010)
 #define LCD_RS        0x01  // Bit 0 (0b00000001)

 #define HIGH_NIBLE_MASK 0xF0 // (0b11110000)


// globalis allapot jelzok megszakitashoz
volatile uint16_t SpikeTimerValue;
volatile bool SpikeFlag = false;


//puffer adatok
uint16_t puffer[8] = {0,0,0,0,0,0,0,0};
uint8_t puffer_index = 0;
uint32_t running_sum = 0;
bool PufferIsFull = false;


//statisztikai adatok
uint16_t Avarage_deltaT;
uint16_t Bph;




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
    BLANK_PERIOD,
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


// I2C alap fugvenyek
void I2C_init(){
    // clk sebesseg
    // clk speed ==> 100kHz
    // keplet: scl = CPU clock speed/ 16+2(TWBR) + (Prescaler value)
    // 100kHz ==> 16 000 000/ 16+2(72) + 0
    TWBR = 72;
    TWSR = 0x00; // twps1, twps0 ==> 0, a tobbi read only
    TWCR = (1 << TWEN); // enable bit
}

void I2C_start(){
    TWCR = (1<<TWINT); // interrupt flag letorlese
    TWCR = (1<<TWSTA); // start jel a slaveknek a buszon
    TWCR = (1<<TWEN); //  eneable bit

    //varakozas ameddig nem jelzet vissa a hw hogy kiadta a start jelet (a twint 1 be allitja a hw)
    while(!(TWCR &(1<<TWINT)));
}

void I2C_write(uint8_t data){
    TWDR = data; // adat atadas a data registernek

    // interrrupt bit es engedelyezo bit
    // (a start bit mar a start fugvenyben 1 be lett allitva)
    TWCR = (1<<TWINT) | (1<<TWEN);

    // varokazas hogy atvegye a slave a biteket es visszajelezzen az interrupt bittel
    while (!(TWCR & (1 << TWINT)));

}


void I2C_stop(){
    //stop jel
    TWCR = (1<<TWSTO) | (1<<TWINT) | (1<<TWEN);
}

//LCD uzenet pipeline: lcd write(alap irasi jelek es adat kuldes) --> lcd pulse enable (ketszer hivja meg a writot az enable bit miatt)
//--> lcd send byte(kette szedi 4 bytera, ketszer hivja meg a pulse enablet)

//LCD driverek I2C kapcsolattal
void LCD_Write_I2C(uint8_t data){
    I2C_start();
    // eloszor cimezuk
    I2C_write(LCD_ADDR);
    // cimzes utan adat kuldese
    LCD_Write_I2C(data);
    // stop jel kiadasa
    I2C_stop();
}

void LCD_PulseEnable(uint8_t data){

    //adata  kuldes enable bit nelkul
    LCD_Write_I2C(data | LCD_ENABLE);

    // adata kuldesse enable bit kapcsolasaval (0 ra kapcsolas mert alacsony aktiv)
    LCD_Write_I2C(data & ~LCD_ENABLE);
}

void LCD_SendByte(uint8_t data, uint8_t mode){
    // felso 4 bit kiszurese --> also 4 bit lenullazasa
    uint8_t high_nible = data & HIGH_NIBLE_MASK;
    // also 4 bit kiszurese --> felso 4 bit kinullazasa
    // ballra toljul 4 el es maskoljuk akkor az also negy bitet kapjuk meg a felso negy bit helyen
    uint8_t low_nible = (data<<4) & HIGH_NIBLE_MASK;

    //ha karakter akkor 1, ha 0 akkor parancs
    uint8_t rs_bit = 0;
    if(mode == 1){
        rs_bit = LCD_RS;
    }


    // adat felso 4 bajtjanak kuldese
    LCD_PulseEnable(high_nible | LCD_BACKLIGHT | rs_bit);
    //adat also 4 bajtjanak kuldese
    LCD_PulseEnable(low_nible | LCD_BACKLIGHT | rs_bit);
}

//LCD felhasznaloi fugvenyek
void LCD_SendCommand(uint8_t cmd){
    LCD_SendByte(cmd, 0);
}

void LCD_SendChar(char c){
    LCD_SendByte(c, 1);
}

void LCD_PrintString(const char* str){
     while( *str ){
         LCD_SendChar( *str++ );
     }
}




int main(){
    //hw setup
    SetupTimer();
    SetupComp();

    while(true){
        switch(GlobalState){

            // SYNC fázis
///////////////////////////////////////////////////////////////////////////
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
                            GlobalState = PROCESSSING;
                        }
                        else{
                            GlobalState = BLANK_PERIOD;
                        }
                    }
                }
                break;
////////////////////////////////////////////////////////////////////////////
            case BLANK_PERIOD:

            //suket fazis elejenek ideje
            cli();
            uint16_t CurrentTime = TCNT1;
            sei();

            uint16_t PassedTime = CurrentTime - SYNC_previousT;

            // 60ms =  3750 timer step
            if(PassedTime > 3750){
                SpikeFlag = false;
                // input capture flag torles
                TIFR1 = (1<<ICF1);
                //timer it ujra engedelyezes
                TIMSK1 |= (1<<ICIE1);

                GlobalState = SYNC;
            }
            break;

////////////////////////////////////////////////////////////////////////////
            case PROCESSSING:

                //puffer init fazis
                if(PufferIsFull == false){
                    puffer[puffer_index] = SYNC_deltaT;
                    running_sum += SYNC_deltaT;
                    puffer_index += 1;
                    if(puffer_index == 8){
                        puffer_index = 0;
                        PufferIsFull = true;
                    }
                }
                else{

                    // korpuffer frissitese
                    running_sum -= puffer[puffer_index];
                    puffer[puffer_index] = SYNC_deltaT;
                    running_sum += SYNC_deltaT;
                    puffer_index += 1;
                    if(puffer_index == 8){
                        puffer_index = 0;
                    }

                    // statisztikai adatok
                    Avarage_deltaT = running_sum >> 3;
                    Bph  = 225000000UL / Avarage_deltaT;

                    // TODO lcd, uart kommunikacio

                }

                GlobalState = BLANK_PERIOD;
                break;

        }
    }
}

//      usart communication
//
//
