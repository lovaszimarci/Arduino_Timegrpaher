#include "UART.h"
#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>


void UART_Init(){
    // baud rate beallitasa
    uint16_t ubrr_value = 103; // datasheet alapjan 16MHz frekvencian
    UBRR0H = (uint8_t)(ubrr_value>>8); // UBRR0H a 12 bites regiszter felso 4 bitje
    UBRR0L = (uint8_t)(ubrr_value); // a 12 bites regiszter also 8 bitje

    // kontrol es statusz regiszterek allitasa
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // receiver es transmitter modok enable
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);// karakter meret beallitas 1-1 = 8 bit datasheet alapjan

}


void UART_Transmit(char data){
    while (!(UCSR0A & (1 << UDRE0))); // megvarjuk ameddig ures a puffer
    UDR0 = data;
}

void UART_Print(const char *str){
    while( *str ){
        UART_Transmit( *str++);
    }
}


static char UART_Receive(void) {
    //varakozas ameddig meg nem jon a karakter
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}


uint16_t UART_AskForBph(){
    char buffer[10];
    uint8_t index = 0;
    char c;
    UART_Print("\r\n--- TIMEGRAPHER SETUP ---\r\n");
        UART_Print("Kerlek ird be a referencia BPH-t (pl. 21600): ");

        while (1) {
            c = UART_Receive(); // varunk a gombnyomasra

            // ha entert nyomatak
            if (c == '\r' || c == '\n') {
                if (index > 0) { // Ha kuldtek is valamit nem csak entert
                    buffer[index] = '\0'; // Szöveg lezárása
                    UART_Print("\r\nBPH Elmentve! Indul a meres...\r\n");
                    break; // Kilépünk a végtelen ciklusból!
                }
            }
            // Ha számot gépeltek (0-9) és van még hely
            else if (c >= '0' && c <= '9' && index < 9) {
                buffer[index] = c;
                index++;
                UART_Transmit(c); // visszakuldjuk hogy lassa mit gepel a user
            }
        }

        // ascii karaktereketet valodi szamma alakitjuk
        return atoi(buffer);
}
