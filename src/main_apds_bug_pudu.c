// Example bugs to showcase Pudu-- these are all tested on Capybara v2.0 with
// the following bank config:
// 0: 2 x 330uf, 1: 3x7mF, 2:0, 3: 1x7mF, 4: 1x7mF
// These were compiled with gcc -O2
// This file is just for apds, and it's designed to work with the pudu timing
// mechanism, to test karma go back to main_bugs.c

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

//Libraries for other peripherals
#include <libapds/gesture.h>
#include <libradio/radio.h>

#include "accel_table.h"
#include "gyro_table.h"

// Test apds
// To test apds, run with bank 4 attached
#define TEST_APDS_BUG
//#define TEST_APDS_FIX
#define TEST_ASYNCH_APDS


int volatile new_gest = 0;
gest_dir __internal_gest;

int* __paca_all_tables[NUM_PERIPHS] = {
};


REGISTER(apds);
REGISTER(asynch);
REGISTER(led);//dummy

PACA_SET_STATUS_PTRS = {NULL,NULL,NULL,&apds_status,NULL};


void apds_delay() {
  WAIT_APDS_DELAY;
}


SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(apds),&STATE_READ(asynch),&STATE_READ(led)};

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

#if 1
#define BIT_FLIP(port,bit) \
do {\
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit;\
 }while(0);
#else
#define BIT_FLIP(port,bit)
#endif

void DRIVER init_led() {
  P1OUT &= ~BIT5;
  P1DIR |= BIT5;
  STATE_CHANGE(led,0);
  return;
}

void DRIVER led_on() {
  P1OUT |= BIT5;
  STATE_CHANGE(led,1);
}

void DRIVER led_off() {
  P1OUT &= ~BIT5;
  STATE_CHANGE(led,0);
}

void DRIVER init_timer() {
  //Init timer to toggle
  printf("Timer going on\r\n");
  TA1CCR0 = 50;
  TA1CTL = TASSEL__ACLK | MC__UP | ID_3 | TAIE_1;
  TA1CCTL0 |= CCIE;
  TA1R = 0;
  STATE_CHANGE(asynch,1);
}


void DRIVER apds_reenable() {
  printf("re!\r\n");
  apds_init();
  WAIT_APDS_DELAY;
  STATE_CHANGE(apds,1);
}

void DRIVER apds_loc_disable() {
  printf("dis!\r\n");
  apds_disable();
  STATE_CHANGE(apds,0);
}

/* Set of dummy functions for Pudu pass*/
void ENABLE(Timer1_A0_ISR) {
#ifdef TEST_ASYNCH_APDS
  init_timer();
#endif
}

void DISABLE(Timer1_A0_ISR) {
  TA1CTL = MC__STOP;
}

gest_dir apds_read_asynch() {
  printf("Asynch\r\n");
  while(!new_gest) {
    BIT_FLIP(1,0);
    __delay_cycles(80000);
    printf("Gest %u\r\n",new_gest); 
    BIT_FLIP(1,0);
  } 
  printf("Start gest\r\n");
  __internal_gest = apds_get_gesture();
  printf("Stop_gest\r\n");
  new_gest = 0;
  return __internal_gest;
}


__nv int gnded_flag = 0;

#define TIMEOUT 10

__nv pacarana_cfg_t inits[4] = {
/*0*/ PACA_CFG_ROW(0,0,NULL),
/*1*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,apds_init,apds_delay),
/*2*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,apds_init,apds_delay,init_timer),
/*3*/ PACA_CFG_ROW(0,1,init_timer),
};


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
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);//TODO make this init real
  RUN_PACA_SEPARATE_PF;
  printf("Capy init! %i %i\r\n",STATE_READ(apds),STATE_READ(asynch));
  unsigned cfg = 0;
#ifndef TEST_ASYNCH_APDS
  if (STATE_READ(apds) > 0) {
    cfg = 1;
  }
  else {
    cfg = 0;
  }
#else//ASYNCH
  if (STATE_READ(asynch) > 0) {
    if (STATE_READ(apds) > 0) {
      cfg = 2;
    }
    else {
      cfg = 3;
    }
  }
  else {
    if (STATE_READ(apds) > 0) {
      cfg = 1;
    }
    else {
      cfg = 0;
    }
  }
#endif//ASYNCH_APDS
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  entry();
  while(1) { // Typical loop structure in embedded systems
    UNSAFE_ATOMIC_START;
    apds_init();
    STATE_CHANGE(apds,1);
    WAIT_APDS_DELAY;
#ifdef TEST_ASYNCH_APDS
    ENABLE(Timer1_A0_ISR);
    printf("Timer on!\r\n");
#endif
    UNSAFE_ATOMIC_END;
    for(int i = 0; i < TIMEOUT; i++) {
      //UNSAFE_ATOMIC_START;
#ifdef TEST_ASYNCH_APDS
      printf("Reading asynch!\r\n");
      gest_dir gesture = apds_read_asynch();
#else
      LIBPACARANA_TOGGLE_PRE(3,apds_reenable);
      gest_dir gesture = apds_get_gesture();
      LIBPACARANA_TOGGLE_POST(apds,3,apds_loc_disable);
#endif
      //UNSAFE_ATOMIC_END;
      if (gesture > DIR_NONE) {
        UNSAFE_ATOMIC_START;
        printf("Gest is %u\r\n",gesture);
        apds_disable();
        STATE_CHANGE(apds,0);
        UNSAFE_ATOMIC_END;
        break;
      }
      printf("i is: %x\r\n",i);
    }
#ifdef TEST_APDS_FIX
    UNSAFE_ATOMIC_START;
    apds_disable();
    STATE_CHANGE(apds,0);
    UNSAFE_ATOMIC_END;
#endif
    UNSAFE_ATOMIC_START;
    BIT_FLIP(1,0);
    printf("Start! %u\r\n",STATE_READ(apds));
    for (int j = 0; j < 200; j++) {
      __delay_cycles(80000); // Stand in for computing
    }
    printf("Stop!\r\n");
    BIT_FLIP(1,1);
    UNSAFE_ATOMIC_END;
  }
}

void __attribute ((interrupt(TIMER1_A0_VECTOR))) PACARANA_INT Timer1_A0_ISR(void) {
#ifdef TEST_ASYNCH_APDS
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  TA1CCTL0 &= ~CCIE;
  new_gest = 1;

  TA1R = 0;
  TA1CCTL0 |= CCIE;
  BIT_FLIP(1,0);
  BIT_FLIP(1,0);
#endif
}

