 #ifndef LCD_I2C_H
 #define LCD_I2C_H

 #include <stdint.h>

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

 //mivel 2db 4 bites adatot kuld egy 8 bites adathoz ezert szet kell valasztani a 8 bitet, ehhez kell a maszk
 #define HIGH_NIBLE_MASK 0xF0 // (0b11110000)


 //i2c vezerlok
 void I2C_init(void);
 void I2C_start(void);
 void I2C_write(uint8_t data);
 void I2C_stop(void);

 //lcd vezerlok
 void LCD_Init(void);
 void LCD_SendCommand(uint8_t cmd);
 void LCD_SendChar(char c);
 void LCD_PrintString(const char* str);
 void LCD_SetCursor(uint8_t row, uint8_t col);


 #endif
