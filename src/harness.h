#ifndef _HARNESS_H_
#define _HARNESS_H_
#include <msp430.h>
#include <liblsm/gyro.h>
#include <libfxl/fxl6408.h>

#ifndef PACARANA
#define REGISTER(...)
#define STATE_CHANGE(...)
#define DRIVER
#else
#warning "Including pacarana stuff!!"
#include <libpacarana/pacarana.h>
#endif
#define BIT_FLIP(port,bit) \
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit; \

// HIGH performance vs normal performance mask for gyro/accelerometer
#define HIGHPERF_MASK 0b10000000


#if defined(GYRO)
#ifndef RATE
	#define RATE 0x80
#endif
REGISTER(gyro);
// Define init
#define SENSOR_INIT() \
	do { \
  __delay_cycles(48000);\
  __delay_cycles(48000);\
  fxl_set(BIT_SENSE_SW);\
  __delay_cycles(48000);\
	gyro_only_init_odr_hm(RATE, RATE & HIGHPERF_MASK);\
	__delay_cycles(19200);\
	STATE_CHANGE(gyro,1);\
	} while(0);

#warning "defining SENSOR_SAMPLE"
// Define capture
#define SENSOR_SAMPLE gyroscope_read_x
// Define a longer read
#define SENSOR_MULTI_SAMPLE(x,y,z) read_raw_gyro(&x,&y,&z)

void g_init() {
	gyro_only_init_odr_hm(RATE, RATE & HIGHPERF_MASK);\
	__delay_cycles(19200);
#if RATE == 0x80
  STATE_CHANGE(gyro,1);
#else
  STATE_CHANGE(gyro,2);
#endif
}

// Definitions for pacarana
void g_disable() {
  if (STATE_READ(gyro) != -1) {
    lsm_gyro_sleep();
	  STATE_CHANGE(gyro,0);
  }
  //printf("Dis!\r\n");
}

void g_reenable() {
#if RATE == 0x80
  if (STATE_READ(gyro) < 0) {
    fxl_set(BIT_SENSE_SW);
    __delay_cycles(48000);
	  gyro_only_init_odr_hm(0x80, 1);
	  __delay_cycles(19200);
  }
  else {
    lsm_gyro_reenable();
    __delay_cycles(19200);
  }
	STATE_CHANGE(gyro,1);
#elif RATE == 0x40
  if (STATE_READ(gyro) < 0) {
    fxl_set(BIT_SENSE_SW);
    __delay_cycles(48000);
	  gyro_only_init_odr_hm(0x40, 0);
    __delay_cycles(19200); // We had unnecessarily long init here...
    //__delay_cycles(153800);
  }
  else {
    lsm_gyro_reenable();
    __delay_cycles(153800);
  }
	STATE_CHANGE(gyro,2);
#endif
}

SLEEP(gyro,g_disable);
RESTORE(gyro,g_reenable);

// Define disable
#define SENSOR_DISABLE() \
	do { \
		lsm_accel_disable();\
		lsm_gyro_sleep();\
		STATE_CHANGE(gyro,1);\
	} while(0);

// Define reenable based on rate
#if RATE == 0x80
	#define SENSOR_REENABLE() \
	do { \
		accel_odr_reenable(RATE);\
		lsm_gyro_reenable();\
		__delay_cycles(19200);\
		STATE_CHANGE(gyro,4);\
	} while(0);
#elif RATE == 0x40
	#define SENSOR_REENABLE() \
	do { \
		accel_odr_reenable(RATE);\
		lsm_gyro_reenable();\
		__delay_cycles(153800); \
		STATE_CHANGE(gyro,2);\
	} while(0);
  #endif
//#endif

#elif defined(ACCEL)
#ifndef RATE
	#define RATE 0x80
#endif
// Define init
#define SENSOR_INIT() \
	do { \
		int temp = 1; \
		while(temp) {\
			fxl_clear(BIT_SENSE_SW); \
			__delay_cycles(48000); \
			fxl_set(BIT_SENSE_SW); \
			temp = accel_only_init_odr_hm(RATE, RATE & HIGHPERF_MASK);\
		} \
	} while(0);

// Define capture
#define SENSOR_SAMPLE accelerometer_read_x
#define SENSOR_MULTI_SAMPLE(x,y,z) dummy_accel_read(&x,&y,&z)

// Define disable
#define SENSOR_DISABLE() \
  do { \
		lsm_accel_disable();\
	} while(0);
// Define reenable based on rate
#if RATE == 0x80
#define SENSOR_REENABLE() \
	do { \
  accel_odr_reenable(RATE); \
  __delay_cycles(4800); \
	} while(0); 
#else
#define SENSOR_REENABLE() \
	do { \
  accel_odr_reenable(RATE); \
	} while(0); 
#endif

//#endif

#elif defined(COLOR)
// Define init
#define SENSOR_INIT() \
do { \
  __delay_cycles(48000); \
  fxl_set(BIT_SENSE_SW); \
  __delay_cycles(48000);\
  fxl_set(BIT_APDS_SW);	\
  __delay_cycles(48000); \
  apds_color_init(); \
	} while(0);
// Define capture
#define SENSOR_SAMPLE apds_read_r
uint16_t _temp_c;
#define SENSOR_MULTI_SAMPLE(x,y,z) apds_dummy_read_color(&x,&y,&z,&_temp_c)
// Define disable
#define SENSOR_DISABLE() apds_color_disable()
// Define reenable
#define SENSOR_REENABLE() \
do { \
	apds_color_reenable();\
	__delay_cycles(20000); \
	} while(0);

//#endif

#elif defined(PROX)
// Define init
#define SENSOR_INIT() \
do { \
  __delay_cycles(48000); \
  fxl_set(BIT_SENSE_SW); \
  __delay_cycles(48000); \
  fxl_set(BIT_APDS_SW); \
  __delay_cycles(48000); \
  proximity_init(); \
  enableProximitySensor(); \
} while(0);

// Define capture
#define SENSOR_SAMPLE proximity_read
#define SENSOR_MULTI_SAMPLE(x,y,z) read_proximity_val(&x)
// Define disable
#define SENSOR_DISABLE() apds_proximity_disable();
// Define reenable based on rate
#define SENSOR_REENABLE() apds_proximity_reenable();
//#endif

#elif defined(TEMP)
// Define init
#define SENSOR_INIT() \
do { \
	ADC12CTL0 &= ~ADC12ENC;\
	ADC12CTL0 = ADC12SHT0_2 + ADC12ON;\
	ADC12CTL1 = ADC12SHP + ADC12CONSEQ_0;\
	ADC12CTL3 &= ADC12TCMAP;\
	ADC12MCTL0 = ADC12INCH_30;\
	ADC12CTL0 |= ADC12ENC;\
	STATE_CHANGE(temp,2);\
} while(0);

// Define capture
#define SENSOR_SAMPLE temp_read
// Define disable
#define SENSOR_DISABLE() temp_disable();
// Define reenable based on rate
#define SENSOR_REENABLE() temp_reenable();
#endif
#endif // _HARNESS_H_
