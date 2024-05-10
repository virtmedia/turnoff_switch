/*
 * turnoff_switch.c
 *
 * Created: 2020-07-04 20:24:08
 * Author : Virtmedia
 
 
 
 PB0 - KICK output transistor control
 PB1 - TIME resistor divider sensor
 PB2 - SW signal from switch (pull-up, active low)
 PB3 - RST
 
 Program flow:
1. startup 
ddr config
2. PB0 set to output HIGH - provide power to the load
-enable adc
3. MCU measures the resistor divider, and decide about turn off delay;
PB2 = input pullup, wait until high
-disable adc
-start watchdog
4. Mcu waits using timer/watchdog
5. When detected falling edge on SW pin, or timeout -> go to step 6
6. MCU sets PB0 to HIGH, turning off the system.
 
Resistor sensing:
-READ ADC from ADC1: range 0-255 -> checked 4 msb (16 states)
 delay time based on lookup table:
 0	:	10 sec
 1	:	30 sec
 2	:	1 min
 3	:	2 min
 4	:	5 min
 5	:	10 min
 6	:	15 min
 7	:	30 min
 8	:	1h
 9	:	2h
 10	:	3h
 11	:	4h
 12	:	8h
 13	:	12h
 14	:	18h
 15	:	infinite

 */ 

#define F_CPU	1000000UL

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define KICK_PIN	PB0
#define TIME_PIN	PB1
#define SW_PIN		PB2


volatile uint16_t timeout = 0xFFFF;

const uint16_t timeouts_lookup[16] = {10, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 14400, 28800, 43200, 64800, 65535};

int main(void)
{
	_delay_ms(20);
	DDRB = _BV(KICK_PIN);
	DIDR0 = _BV(ADC1D);
	PORTB = _BV(KICK_PIN)|_BV(SW_PIN);
	_delay_ms(100);
	ADMUX = ADC1;
	ADCSRA = _BV(ADEN)|_BV(ADSC)|_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);
    while(ADCSRA & _BV(ADSC));
	timeout = ADCL+1; 
	//one more conversion - 1st conversion may have problems
	ADCSRA |= _BV(ADEN);
	while(ADCSRA & _BV(ADSC));
	timeout = timeouts_lookup[(ADCL>>4) & 0x0F]; 
	ADCSRA &= ~_BV(ADEN);
	//timeout = 50;
	//watchdog configure
	WDTCSR = _BV(WDIE)|_BV(WDP1)|_BV(WDP2); //1.0s timeout, interrupt
	wdt_reset();
	sei();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
    while (1) 
    {
		//TODO: add functionality of powering down after pressing the switch
		//has to detect pressing down, then should wait until back to up, then should wait ~30 ms and then kill
		sleep_cpu();
    }
}


ISR(WDT_vect)
{
	if(timeout != 65535){
		if((--timeout) == 0){
			wdt_disable();
			ADCSRA &= ~_BV(ADEN);
			PORTB &= ~_BV(KICK_PIN);
			//DDRB &= ~_BV(KICK_PIN);
			timeout = 1;
			while(1);
		}
	}

}


