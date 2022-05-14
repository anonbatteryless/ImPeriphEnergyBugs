#include "temp_sensor.h"


float temp_read_and_init() {
	ADC12CTL0 &= ~ADC12ENC;
	ADC12CTL0 = ADC12SHT0_2 + ADC12ON;
	ADC12CTL1 = ADC12SHP + ADC12CONSEQ_0;
	ADC12CTL3 &= ADC12TCMAP;
	ADC12MCTL0 = ADC12INCH_30;
	ADC12CTL0 |= ADC12ENC;
	ADC12CTL0 &= ~ADC12SC;  // 'start conversion' bit must be toggled
	ADC12CTL0 |= ADC12SC; // start conversion
  while (ADC12CTL1 & ADC12BUSY); // wait for conversion to complete
	//ADC12CTL3 &= ~ADC12TCMAP;
	unsigned val = ADC12MEM0;
	float casted = (float) val;
	return casted;
}

unsigned temp_read() {
	ADC12CTL0 &= ~ADC12SC;  // 'start conversion' bit must be toggled
	ADC12CTL0 |= ADC12SC; // start conversion
  while (ADC12CTL1 & ADC12BUSY); // wait for conversion to complete
	//ADC12CTL3 &= ~ADC12TCMAP;
	unsigned val = ADC12MEM0;
	return val;
}

void DRIVER temp_disable() {
	ADC12CTL3 &= ~ADC12TCMAP;
	STATE_CHANGE(temp,1);
}

void DRIVER temp_reenable() {
	ADC12CTL3 &= ADC12TCMAP; // enable temperature sensor
	ADC12MCTL0 = ADC12INCH_30; // temp sensor
	ADC12CTL0 |= ADC12ENC; // enable ADC
	STATE_CHANGE(temp,2);
}
