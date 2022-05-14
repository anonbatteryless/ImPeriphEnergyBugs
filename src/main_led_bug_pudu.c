// Example bugs to showcase Pudu-- these are all tested on Capybara v2.0 with
// the following bank config:
// 0: 2 x 330uf, 1: 3x7mF, 2:0, 3: 1x7mF, 4: 1x7mF
// These were compiled with gcc -O2

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

// Test lights left on
// To test lights left on, rig up led between P1.5 and ground (no, we don't add
// a resistor because the msp430 is already pretty current limited and the
// output is only at 2.5V). Then connect Bank  1 and go for it.  Use a dla to
// watch P1.0 and P1.1. You'll notice that the start/stop signals around the
// radio fail to complete atomically once the system gets locked into the fail
// stop case.  Fail stop occurs when the timer is disabled, but the led was
// still on when we started to try to send the radio packet
//#define TEST_LIGHTS_BUG
#define TEST_LIGHTS_FIX


int* __paca_all_tables[NUM_PERIPHS] = {
};


REGISTER(led);
REGISTER(photo);
REGISTER(asynch);

PACA_SET_STATUS_PTRS = {NULL,NULL,NULL,&led_status,&photo_status};

int16_t check_heading(int16_t gx, int16_t gy, int16_t gz, int16_t ax, int16_t ay, int16_t az,int16_t dist) {
  if (gx > 1000 || gx < -1000) {
    return 0;
  }
  if (gy > 1000 || gy < -1000) {
    return 0;
  }
  if (gz > 1000 || gz < -1000) {
    return 0;
  }
  if (ax > az || ay > az) {
    return 0;
  }
  //Else...
  if (dist > 60) {
    return 0;
  }
  return 1;
}

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(led),&STATE_READ(photo),&STATE_READ(asynch)};

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


/* Set of dummy functions for Pudu pass*/
void ENABLE(Timer1_A0_ISR) {
  init_timer();
  STATE_CHANGE(asynch,1);
}

void DISABLE(Timer1_A0_ISR) {
  TA1CTL = MC__STOP;
  STATE_CHANGE(asynch,0);
}

__nv int gnded_flag = 0;

void photo_reenable() {
  printf("Re!\r\n");
  enable_photoresistor();
  STATE_CHANGE(photo,1);
}
void photo_disable() {
  printf("dis!\r\n");
  disable_photoresistor();
  STATE_CHANGE(photo,0);
}

__nv pacarana_cfg_t inits[8] = {
/*0*/ PACA_CFG_ROW(0,4,enable_photoresistor,init_led,led_on,init_timer),
/*1*/ PACA_CFG_ROW(0,3,enable_photoresistor,init_led,init_timer),
/*2*/ PACA_CFG_ROW(0,3,enable_photoresistor,init_led,led_on),
/*3*/ PACA_CFG_ROW(0,2,enable_photoresistor,init_led),
/*4*/ PACA_CFG_ROW(0,3,init_led,led_on,init_timer),
/*5*/ PACA_CFG_ROW(0,2,init_led,init_timer),
/*6*/ PACA_CFG_ROW(0,2,init_led,led_on),
/*7*/ PACA_CFG_ROW(0,1,init_led),
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
  printf("Capy init! %i %i\r\n",STATE_READ(led),STATE_READ(asynch));
  unsigned cfg = 0;
  if (STATE_READ(photo) > 0) {
    if (STATE_READ(led) > 0) {
      if (STATE_READ(asynch) > 0) {
        cfg = 0;
      }
      else {
        cfg = 2;
      }
    }
    else {
      if (STATE_READ(asynch) > 0) {
        cfg = 1;
      }
      else {
        cfg = 3;
      }
    }
  }
  else {
    if (STATE_READ(led) > 0) {
      if (STATE_READ(asynch) > 0) {
        cfg = 4;
      }
      else {
        cfg = 6;
      }
    }
    else {
      if (STATE_READ(asynch) > 0) {
        cfg = 5;
      }
      else {
        cfg = 7;
      }
    }
  }
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  entry();
  while(1) {
    UNSAFE_ATOMIC_START;
    // Init led
    init_led();
    // Init led timer for toggling
    ENABLE(Timer1_A0_ISR);
    // enable photoresistor
    enable_photoresistor();
    STATE_CHANGE(photo,2);
    UNSAFE_ATOMIC_END
    uint16_t distance = 0;
    do {
      BIT_FLIP(1,0);
      __delay_cycles(80000);
      BIT_FLIP(1,0);
      BIT_FLIP(1,0);
      LIBPACARANA_TOGGLE_PRE(4,photo_reenable);
      uint16_t light = read_photoresistor();
      LIBPACARANA_TOGGLE_POST(photo,4,photo_disable);
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      if (light < LOW_OUTPUT) {
        distance = 0;
      }
      else {
        distance = 3500 - light;
      }
      UNSAFE_ATOMIC_START;
      printf("Dist is %u %u\r\n",distance,light);
      UNSAFE_ATOMIC_END;
    } while(distance > 0 && distance < 1500);

    //ASYNCH STOP
    //disable_timer();
    DISABLE(Timer1_A0_ISR);
    #ifdef TEST_LIGHTS_FIX
    led_off();
    STATE_CHANGE(led,0);
    #endif
    // Put distance in pkt with a bunch of other info
    for (unsigned i = 2; i < LIBRADIO_BUFF_LEN; ++i) {
        radio_buff[i] = 'A';
    }
    radio_buff[0] = distance & 0xff;
    radio_buff[1] = (distance & 0xff00) >> 8;
    UNSAFE_ATOMIC_START;
    printf("Start %u\r\n",STATE_READ(led));
    BIT_FLIP(1,0);
    BIT_FLIP(1,0);
    radio_send_one_off();
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    printf("Stop\r\n");
    UNSAFE_ATOMIC_END;
  }
}

void __attribute ((interrupt(TIMER1_A0_VECTOR))) PACARANA_INT Timer1_A0_ISR(void) {
  __disable_interrupt();
  // Toggle led state
  P1OUT ^= BIT5;
  if (STATE_READ(led)) {
    BIT_FLIP(1,0);
    STATE_CHANGE(led,0);
  }
  else {
    BIT_FLIP(1,1);
    STATE_CHANGE(led,1);
  }
  TA1R = 0;
  __enable_interrupt();
}
