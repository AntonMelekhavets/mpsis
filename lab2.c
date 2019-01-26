#include <msp430.h>

/*
 * main.c
 */
int state = 0; // 0-не нажата; 1-нажата
int led_counter = 0; //счетчик для включения определенного LEDa

void delay() {
	TA1CCR0 = 2000;
	TA1CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;
	TA1CCTL0 = CCIE; //разрешение прерываний по захвату/сравнению
}

void initButton() //инициализирует кнопку S1
{
	P1DIR = BIT7;
	P1REN |= BIT7; //разрещение подтягиваюшего резистора
	P1OUT |= BIT7;
	P1SEL = 0; // как i/o
}
void initLeds() {
	P1DIR = BIT0; //направление для диода
	P1OUT &= ~BIT0; // тоже можно
	P8DIR = BIT1 | BIT2;
	P8OUT = 0;
	P8SEL = 0;
}

void startTimer() {
	// Настраиваем таймер – источник тактирования – SMCLK,
	// делитель – 8, режим работы – прямой счет,
	TA2CCR0 = 40000;
	TA2CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;
	TA2CCTL0 = CCIE; //разрешение прерываний по захвату/сравнению
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void) {
	P1IE = 0;
	//попробовать отключить прерывания или поменять фронт
	if (P1IN & BIT7) {
		switch (state) {
		case 0: {
			state = 1;
			break;
		}
		case 1: {
			state = 0;
			break;
		}

		}
		P1IFG = 0; //сброс флагов
	}
	delay();
}

#pragma vector=TIMER2_A0_VECTOR
__interrupt void TA2_ISR(void) {
	if (state == 1) {
		switch (led_counter) {
		case 0: {
			P1OUT |= BIT0;
			break;
		}
		case 1: {
			P8OUT |= BIT1;
			break;
		}
		case 2: {
			P8OUT |= BIT2;
			break;
		}
		}
		led_counter++;
	} else {
		P1OUT &= ~BIT0; //выключение ledов
		P8OUT &= ~BIT1;
		P8OUT &= ~BIT2;
		led_counter = 0;
	}
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TA1_ISR(void) {
	P1IE |= BIT7;
	TA1CCTL0 = 0;
}


int main(void) {
	WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer

	initButton();
	initLeds();
	startTimer();

	P1IFG = 0;
	P1IE |= BIT7;
	P1IES |= BIT7; // прерывание по переходу из 1 в 0(нажатие кнопки)

	__bis_SR_register(LPM0_bits + GIE);
	__no_operation();
	return 0;
}
