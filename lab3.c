#include <msp430.h>

volatile short FREQ = 0; 	// 0 - *10; 1 = *60
volatile short LPM = 0; 	// 0 - active mode, 1 - LPM2 mode.
volatile short lowVcore = 0; // 0 - отключен, 1 - выключен

// LED1: P1.0
// LED3: P8.2
// LED4: P1.1
// LED5: P1.2
// LED8: P1.5
// S1: P1.7
// S2: P2.2

void increaseVcore(unsigned int level) {
	PMMCTL0_H = PMMPW_H;	// открыть PMM регистры для доступа
	SVSMHCTL = SVSHE | SVSHRVL0 * level | SVMHE | SVSMHRRL0 * level;// управление SVM/SVS на входе
	SVSMLCTL = SVSLE | SVMLE | SVSMLRRL0 * level;// установка SVM в новый уровень

	while (!(PMMIFG & SVSMLDLYIFG))
		;	// ожидание пока установится SVM

	PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);	// очистка ранее установленных флагов
	PMMCTL0_L = PMMCOREV0 * level;	// установка Vcore в новый уровень

	if ((PMMIFG & SVMLIFG))	// ожидание пока будет достигнут новый уровень
		while (!(PMMIFG & SVMLVLRIFG))
			;

	SVSMLCTL = SVSLE | SVSLRVL0 * level | SVMLE | SVSMLRRL0 * level;// управление SVM/SVS на выходе
	PMMCTL0_H = 0;	// закрыть PMM регистры для доступа
}

void decreaseVcore(unsigned int level) {
	PMMCTL0_H = PMMPW_H;	// открыть PMM регистры для доступа
	SVSMLCTL = SVSLE | SVSLRVL0 * level | SVMLE | SVSMLRRL0 * level;// управление SVM/SVS на входе

	while (!(PMMIFG & SVSMLDLYIFG))
		;	// ожидание пока установится SVM

	PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);	// очистка ранее установленных флагов
	PMMCTL0_L = PMMCOREV0 * level;	// установка Vcore в новый уровень
	PMMCTL0_H = 0;	// открыть PMM регистры для доступа
}

void setFreq10() {
	UCSCTL1 = DCORSEL_2; 		// выбираем нужный диапазон частот
	UCSCTL2 = FLLD_1 + 9; 		// N = 10
	UCSCTL3 |= SELREF__REFOCLK; // источник для FLL - REFOCLK
	UCSCTL4 |= SELA__REFOCLK | SELS__REFOCLK | SELM__REFOCLK; //источник для ACLK, SMCLK, MCLK - REFOCLK
}

void setFreq60() {
	UCSCTL1 = DCORSEL_2; 		// выбираем нужный диапазон частот
	UCSCTL2 = FLLD_1 + 59; 		// N = 60
	UCSCTL3 |= SELREF__REFOCLK; // источник для FLL - REFOCLK
	UCSCTL4 |= SELA__REFOCLK | SELS__REFOCLK | SELM__REFOCLK; //источник для ACLK, SMCLK, MCLK - REFOCLK
}

#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void) {
	P1IE &= ~BIT7; // disabling interrupts from S1 while processing current interrupt

	TA1CCTL0 = CCIE;
	TA1CCR0 = 2;
	TA1CTL = TASSEL__ACLK + MC__UP + ID__8 + TACLR + TAIE;
	TA1CTL &= ~TAIFG;
}

#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR(void) {
	P2IE &= ~BIT2; // disabling interrupts from S2 while processing current interrupt

	TA0CCTL0 = CCIE;
	TA0CCR0 = 2;
	TA0CTL = TASSEL__ACLK + MC__UP + ID__8 + TACLR + TAIE;
	TA0CTL &= ~TAIFG;
}

#pragma vector = WDT_VECTOR
__interrupt void WDT_ISR(void) {
	P1OUT ^= BIT5;
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER_ISR(void) {
	TA0CTL = 0;
	if (P2IFG & BIT2) {
		if (LPM == 1) {
			LPM = 0;
			P8OUT &= ~BIT2;
			P1OUT |= BIT0;
			_BIC_SR_IRQ(SCG1 + CPUOFF);
			// выход из LPM2
		} else {
			P1OUT &= ~BIT0;
			P8OUT |= BIT2;
			LPM = 1;
			_BIS_SR_IRQ(SCG1 + CPUOFF);
			// вход в LPM2
		}
	}

	P2IFG &= ~BIT2; 		// reseting interrupt 2nd button
	P2IE |= BIT2; 			// enabling interrupt 2nd button
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_ISR(void) {
	TA1CTL = 0;

	if (P1IFG & BIT7) {
		if (FREQ == 0) {
			FREQ = 1;
			P1OUT &= ~BIT1; 			// выключение led4
			increaseVcore(2);
			increaseVcore(3);
			setFreq60();
			P1OUT |= BIT2; 				// включение led5
			WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL + WDTIS2 + WDTSSEL0 + WDTIS0; //250ms
		} else {
			FREQ = 0;
			P1OUT &= ~BIT2; 			// выключение led5
			decreaseVcore(2);
			decreaseVcore(1);
			setFreq10();
			P1OUT |= BIT1; 				// включение led4
			WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL + WDTIS2 + WDTSSEL0; //1000ms
		}
	}

	P1IFG &= ~BIT7; // reseting interrupt 1st button
	P1IE |= BIT7; 	// enabling interrupt 1st button
}

int main(void) {

	// настройка сторожевого таймера для работы в интервальном режиме
	WDTCTL = WDTPW + WDTTMSEL + WDTCNTCL + WDTIS2 + WDTSSEL0; //1000ms

	// настройка светодиодов на выход
	P1DIR |= BIT0;
	P1DIR |= BIT1;
	P1DIR |= BIT2;
	P1DIR |= BIT5;
	P8DIR |= BIT2;
	// вывод частоты на периферию
	P7DIR |= BIT7;
	P7SEL |= BIT7;

	P1OUT |= BIT0; 		// включение led1
	P1OUT &= ~BIT2; 	// выключение led5
	P1OUT |= BIT1; 		// включение led4
	P1OUT &= ~BIT5; 	// выключение led8
	P8OUT &= ~BIT2; 	// выключение led3

	P1DIR &= ~BIT7; 	// настройка кнопок на вход
	P2DIR &= ~BIT2;

	P1OUT |= BIT7; 		// настройка подтягивающего резистора
	P2OUT |= BIT2;

	P1REN |= BIT7; 		// разрешение подтягивающего резистора
	P2REN |= BIT2;

	P1IES |= BIT7; 	// прерывание по спаду (переход из 1 в 0 - нажатие кнопки)
	P1IFG &= ~BIT7; 	// обнуление флага прерывания кнопки
	P1IE |= BIT7; 		// разрешение прерывания от кнопки

	P2IES |= BIT2; 		// аналогичная настройка второй кнопки
	P2IFG &= ~BIT2;
	P2IE |= BIT2;

	setFreq10();

	SFRIE1 |= WDTIE; //разрешение прерываний от сторожевого таймера

	_enable_interrupt();

	while(1) {

	};
	//__no_operation();

	return 0;
}
