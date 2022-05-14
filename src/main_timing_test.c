// Test for running the new timer based pacarana system

#include <stdlib.h>
#include <stdint.h>
//#include <string.h>
#include <stdio.h>
// Built-in libraries for the msp430
#include <msp430.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/clock.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>
#include <libmsp/mem.h>
#include <libio/console.h>
// Functions for the capybara board
#include <libcapybara/board.h>
// Libraries for handling matrices and fixed point calculations
#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include "harness.h"
// Pacarana library
#ifdef PACARANA
#include <libpacarana/pacarana.h>
#endif
// Library for JIT checkpointing
#include <librustic/checkpoint.h>

// Libraries for the IMU and temp sensor
#include <liblsm/gyro.h>
#include <liblsm/accl.h>
#include "temp_sensor.h"
#include <libfxl/fxl6408.h>

#include "accel_table.h"
#include "gyro_table.h"
#include "color_table.h"
#include "prox_table.h"
#include "temp_table.h"
int* __paca_all_tables[NUM_PERIPHS] = {
	accel_table,
  gyro_table,
  color_table,
  prox_table,
  temp_table
};

__nv pacarana_cfg_t inits[1] = {
  PACA_CFG_ROW(0, 0, NULL),
};

REGISTER(gyro);
REGISTER(accel);

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(accel),&STATE_READ(gyro)};


void print_float(float f)
{
	if (f < 0) {
		PRINTF("-");
		f = -f;
	}

	PRINTF("%u.",(int)f);

	// Print decimal -- 3 digits
	PRINTF("%u", (((int)(f * 1000)) % 1000));
  PRINTF(" ");
}
void print_long(uint32_t l) {
  uint16_t cast_lower, cast_upper;
  cast_upper = (uint16_t)((l & 0xFFFF0000)>>16);
  cast_lower = (uint16_t)(l & 0xFFFF);
  PRINTF("%x%x ",cast_upper,cast_lower);
}

#if 0
#define BIT_FLIP(port,bit) \
do {\
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit;\
 }while(0);
#else
#define BIT_FLIP(port,bit)
#endif

void g_disable() {
	lsm_accel_disable();
	lsm_gyro_sleep();
	STATE_CHANGE(gyro,0);
  //PRINTF("G2: %i\r\n",STATE_READ(gyro));
}

void g_reenable() {
	accel_odr_reenable(0x80);
	lsm_gyro_reenable();
	__delay_cycles(19200);
	STATE_CHANGE(gyro,1);
}


__nv int gnded_flag = 0;

int DRIVER main(void) {
	capybara_init();
  P1DIR &= ~BIT2;
  P1REN |= BIT2;
  P1OUT |= BIT2;
  while(!gnded_flag) {
    if (P1IN & (BIT2)) {
      gnded_flag = 1;
      break;
    }
  }
  printf("Capy init!\r\n");
  __delay_cycles(48000);
  __delay_cycles(48000);
  fxl_set(BIT_SENSE_SW);
  __delay_cycles(48000);
	gyro_init_data_rate_hm(0x80, 1);
  accel_only_init_odr_hm(0x80, 1);
  STATE_CHANGE(accel,1);
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);//TODO make this init real
  paca_init(0);
  printf("Ready!\r\n");
  entry();
  int flag = 0;
  while(1) {
    //gyro use
    LIBPACARANA_STOP_TIMER(0);
    UNSAFE_ATOMIC_START;
    if (LIBPACARANA_TOGGLES(0)) {
      printf("Reen accel! %u\r\n",flag);
      accel_odr_reenable(0x80);
    }
    float accel_val = accelerometer_read_x();
    UNSAFE_ATOMIC_END;
    unsigned int a_pc;
    __asm__ volatile ("MOVX.A R0, %0 ": "=r" (a_pc));
    LIBPACARANA_START_TIMER(a_pc,0, 2);
    if (LIBPACARANA_TOGGLES(0)) {
      UNSAFE_ATOMIC_START;lsm_accel_disable();UNSAFE_ATOMIC_END;
      printf("Dis accel!\r\n");
    }
    LIBPACARANA_GRAB_START_TIME(a_pc);
    printf("ACCEL: ");
    print_float(accel_val);
    printf("\r\n");
    if(flag > 100) { printf("Done\r\n"); while(1); }
    else { printf("flag = %i\r\n",flag);}
    //BIT_FLIP(1,1);
    LIBPACARANA_STOP_TIMER(1);
    BIT_FLIP(1,0);
    BIT_FLIP(1,0);
    //BIT_FLIP(1,1);
    UNSAFE_ATOMIC_START;
    if (LIBPACARANA_TOGGLES(1)) {
      printf("Reenabling! %u\r\n",flag);
      g_reenable();
    }
    float x_val = gyroscope_read_x();
    UNSAFE_ATOMIC_END;
    //TODO wrap in a macro
    unsigned int pc;
    __asm__ volatile ("MOVX.A R0, %0 ": "=r" (pc));
    //printf("pc: %x\r\n",pc);
    //unsigned int ps = (flag & 0x1) + 1; // periph_status
    unsigned int ps = 1;
    LIBPACARANA_START_TIMER(pc,1, ps);
    if (LIBPACARANA_TOGGLES(1)) {
      UNSAFE_ATOMIC_START;g_disable(); UNSAFE_ATOMIC_END;
      printf("Disabling!\r\n");
    }
    BIT_FLIP(1,0);
    BIT_FLIP(1,0);
    LIBPACARANA_GRAB_START_TIME(pc);
    x_val *= 100;
    flag++;
    print_float(x_val);
    uint32_t val = x_val;
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    for(unsigned i = 0; i < 0xff; i++) {
      //Start with bit count
      int j;
      volatile int nn;
      int temp = i + val;
      for (j = nn = 0; temp && (j < (sizeof(long) * 8)); ++j, temp >>= 1) {
        nn += (int)(temp & 1L);
      }
      //printf("Count is: %x\r\n",nn);
    }
    printf("Second phase!\r\n");
    if (flag & 0b1100) {
      //TODO wrap all this nonsense in a macro or move to driver libraries
      LIBPACARANA_STOP_TIMER(1);
      UNSAFE_ATOMIC_START;
      if (LIBPACARANA_TOGGLES(1)) {
        printf("Reenabling2! %u\r\n",flag);
        g_reenable();
      }
      float new_val = gyroscope_read_x();
      UNSAFE_ATOMIC_END;
      unsigned int pc2;
      __asm__ volatile ("MOVX.A R0, %0 ": "=r" (pc2));
      LIBPACARANA_START_TIMER(pc2,1, ps);
      if (LIBPACARANA_TOGGLES(1)) {
        UNSAFE_ATOMIC_START;g_disable(); UNSAFE_ATOMIC_END;
        printf("Disabling2!\r\n");
      }
      LIBPACARANA_GRAB_START_TIME(pc2);
      printf("new float:");
      print_float(new_val);
    }
  }
}

