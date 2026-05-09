#ifndef UART_H
#define UART_H
#include <stdint.h>

void UART_Init(void);
void UART_Transmit(char data);
void UART_Print(const char* str);
uint16_t UART_AskForBph(void);

#endif
