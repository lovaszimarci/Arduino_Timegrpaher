#include "HW_INIT.h"
#include <avr/io.h>

//====================================
//TIMER FELPROGRAMOZASA
//====================================

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
prescale = 256
--> 16000000/256 = 62500Hz --> egy másodperc alatt 62500-at lép a timer
1 timer lépés = 0,000016s = 0.016 milisec = 16 mikrosec

Tmax = 65536 x 0.000016s = 1.048576s
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

void SetupDS3231(){

    // az ora modult a D2 lábra kötöttük be
    EICRA |= (1 << ISC01); // megszakitas lefuto elre lesz
    EIMSK |= (1 << INT0); // megszakitas engedelyezese
}
