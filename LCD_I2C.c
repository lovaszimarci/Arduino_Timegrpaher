#include "LCD_I2C.h"
#include <avr/io.h>
#include <util/delay.h>

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



void LCD_Init(){


    // az lcd kijelzőt 4 bites működésre a HD44780 datasheet alapján programoztam fel
    // a datasheet tartalmazza az osszes itt talalhato magic numbert
    //https://www.alldatasheet.com/datasheet-pdf/download/63673/HITACHI/HD44780.html

    // tapfesz utani delay
    _delay_ms(50);

    //elso ebresztes, datasheet alapjan 0x03
    LCD_PulseEnable( (0x03 << 4) | LCD_BACKLIGHT );
    _delay_ms(5);

    //masodik ebresztes
    LCD_PulseEnable( (0x03 << 4) | LCD_BACKLIGHT );
    _delay_us(150);

    //harmadik ebresztes
    LCD_PulseEnable( (0x03 << 4) | LCD_BACKLIGHT );
    _delay_us(150);

    //atkapcsolas 4 bites modba
    LCD_PulseEnable( (0x02 << 4) | LCD_BACKLIGHT );
    _delay_ms(1);

    // mivel mostmar 4 bites modban van a kijelzo igy lehet hasznalni a sajat fugvenyeket

    //kijelzo parameterinek allitasa: 4 bites mod, 2 sor, 5x8-as font ()
    LCD_SendCommand(0x28);
    _delay_ms(1);

    // kijelzo kikapcsolasa, ez biztonsagi lepes a torles elott
    LCD_SendCommand(0x08);
    _delay_ms(1);

    // kepernyo teljes torlese
    LCD_SendCommand(0x01);
    _delay_ms(3);

    // a kurzor beviteli modja: mindig lepjen egyet jobbra
    LCD_SendCommand(0x06);
    _delay_ms(1);

    // kijelzo visszakapcsolasa es kurzor elrejtese
    LCD_SendCommand(0x0C);
    _delay_ms(1);

}



//LCD kurzor mozgatasa
// 1. sor cime: 0x00
// 2. sor cime: 0x40
// kurzor mozgato parancs: 0x80
// pl masodik sorba mozgatas: 0x80 + 0x40 = 0xC0
void LCD_SetCursor(uint8_t row, uint8_t col){
    uint8_t addres;
    if(row == 0){
        addres = 0x80 + col;
    }
    else{
        addres = 0xC0 + col;
    }
    LCD_SendCommand(addres);
}
