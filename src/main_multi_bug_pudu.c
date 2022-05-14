// Example bugs to showcase Pudu-- these are all tested on Capybara v2.0 with
// the following bank config:
// 0: 2 x 330uf, 1: 3x7mF, 2:0, 3: 1x7mF, 4: 1x7mF
// These were compiled with gcc -O2
// This file is just for the multi-peripheral bug and it's designed to work with
// the pudu timing mechanism, to test karma go back to main_bugs.c

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


// We're assuming this is tested with INIT_0 defined in libpacarana
// To test this, you need to connect just bank 4 on capybara.
// Place your hand above the apds sensor on Capy and move it around until an
// "emergency" is triggered. An emergency gets triggered if the object [your
// hand] is practically touching the apds, or if the hand is close and the
// device is turning (as indicated by the gyro) or the z acceleration vector is
// not the largest (i.e. as compared to x and y acceleration).
// If the emergency is triggered just with the proximity sensor active, you'll
// successfully make it through on the next reboot if not on that go.  If you
// end up enabling the IMU too, the emergency maneuvers will fail because
// there's not enough energy.
#define TEST_MULTI_BUG
//#define TEST_MULTI_FIX
#define TEST_ASYNCH_MULTI


// To test the Pudu pass, enable the following flag and compile this app using
// clang (e.g. make bld/clang/clean; make bld/clang/all RUN_BUGS=1 USE_SENSORS=)
//#define RUN_PUDU_PASS

int volatile new_dist = 0, new_imu = 0;
uint8_t volatile __internal_dist;

int* __paca_all_tables[NUM_PERIPHS] = {
};

REGISTER(accel);
REGISTER(asynch);
REGISTER(imu);
REGISTER(prox);

PACA_SET_STATUS_PTRS = {NULL,&imu_status,NULL,&prox_status,NULL};


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

// Function designed to turn off all sensors as quickly as possible. This will
// incur a higher restart cost, but that's ok, we're trying to make it through
// the maneuvers regardless of which extra sensors we've enabled.
// This really needs to be contained within an atomic block, but it needs to be
// separate from atomic blocks that _can't_ have the sensors enabled
void DRIVER sensor_ripcord() {
  STATE_CHANGE(imu,-1);
  STATE_CHANGE(prox,-1);
  fxl_clear(BIT_SENSE_SW); //Turn off all sensors as quickly as possible
}
void DRIVER imu_init() {
  gyro_init_data_rate_hm(0x80,1); //Turn on gyro & accel in IMU
	__delay_cycles(19200);
  //Clear imu pipeline
  int16_t gx,gy,gz,ax,ay,az;
  for (int i = 0; i < 5; i++) {
    BIT_FLIP(1,1);
    read_raw_gyro(&gx,&gy,&gz);
    printf("%i %i %i\r\n",gx,gy,gz);
    __delay_cycles(8000);
  }
  STATE_CHANGE(imu,1);
}

void DRIVER imu_reenable() {
  if (STATE_READ(imu) < 0) {
    imu_init();
  }
  accel_odr_reenable(0x80);
  lsm_gyro_reenable();
  __delay_cycles(19200);
  STATE_CHANGE(imu,1);
}

void DRIVER imu_disable() {
  lsm_accel_disable();
  lsm_gyro_sleep();
  LIBPACARANA_TOGGLES(1) = 1; //hit the toggle so we'll reenabled at a use
  STATE_CHANGE(imu,0);
}


void delay_1() {
  __delay_cycles(48000);
}

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(imu),&STATE_READ(prox),&STATE_READ(asynch)};

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
#ifdef TEST_ASYNCH_MULTI
  init_timer();
#endif
}

void DISABLE(Timer1_A0_ISR) {
  TA1CTL = MC__STOP;
}

uint8_t read_prox_asynch(){
  while(!new_dist) {
    __delay_cycles(80000);
    printf("Prox %u\r\n",new_dist);
  }
  new_dist = 0;
  return __internal_dist;
}
void read_imu_asynch(int16_t *gx,int16_t *gy,int16_t *gz,int16_t *ax,int16_t *ay,int16_t *az) {
  while(!new_imu) {
    BIT_FLIP(1,1);
    __delay_cycles(8000);
    printf("imu %u\r\n",new_imu);
  }
  read_raw_accel(ax,ay,az);
  read_raw_gyro(gx,gy,gz);
  new_imu = 0;
  return;
}

void DRIVER disable_timer() {
  TA1CTL = MC__STOP;
  STATE_CHANGE(asynch,0);
}

void DRIVER proximity_sense_init() {
  proximity_init();
  enableProximitySensor();
  STATE_CHANGE(prox,1);
}

void DRIVER proximity_sense_disable() {
  apds_proximity_disable();
  LIBPACARANA_TOGGLES(3) = 1; //hit the toggle so we'll reenabled at a use
  STATE_CHANGE(prox,0);
}

void DRIVER proximity_sense_reenable() {
  if (STATE_READ(prox) < 0) {
    proximity_init();
  }
  apds_proximity_reenable();
  STATE_CHANGE(prox,1);
}

__nv int gnded_flag = 0;

#define TIMEOUT 10

__nv pacarana_cfg_t inits[7] = {
/*0*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,delay_1,proximity_sense_init),
/*1*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,1,delay_1),
/*2*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,proximity_sense_init,imu_init),
/*3*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,delay_1,imu_init),
/*4*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,proximity_sense_init,init_timer),
/*5*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,4,delay_1,proximity_sense_init,imu_init,init_timer),
/*6*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,imu_init,init_timer),
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
  printf("Capy init! %i %i\r\n",STATE_READ(prox),STATE_READ(imu));
  unsigned cfg = 0;
    if (STATE_READ(prox) > 0) {
      if (STATE_READ(imu) > 0) {
        //Prox & imu are active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 2;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 5;
        }
        else {
          cfg = 2;
        }
    #endif
      }
      else {
        //Prox is active, imu is not
    #ifndef TEST_ASYNCH_MULTI
        cfg = 0;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 4;
        }
        else {
          cfg = 0;
        }
    #endif
      }
    }
    else {
      //Prox is off
      if (STATE_READ(imu) > 0){
        //Imu is active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 3;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 6;
        }
        else {
          cfg = 3;
        }
    #endif
      }
      else {
        //Nothing is active
        cfg = 1;
      }
    }
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  entry();
  while(1) {// Loop from embedded systems code
    int heading = 1;
    //Initialize dist sensor
    UNSAFE_ATOMIC_START;
    proximity_sense_init();
    STATE_CHANGE(prox,1);
    #ifdef TEST_ASYNCH_MULTI
    //init_multi_timer();
    ENABLE(Timer1_A0_ISR);
    printf("timer up!\r\n");
    #endif
    UNSAFE_ATOMIC_END;
    do {
      //Read dist
      uint8_t dist;
      #ifndef TEST_ASYNCH_MULTI
      LIBPACARANA_TOGGLE_PRE(3,proximity_sense_reenable);
      printf("Prox state: %i\r\n",STATE_READ(prox));
      dist = proximity_read_byte();
      printf("Dist is: %u\r\n",dist);
      LIBPACARANA_TOGGLE_POST(prox,3,proximity_sense_disable);
      #else
      UNSAFE_ATOMIC_START;
      dist = read_prox_asynch();
      printf("Dist is: %u\r\n",dist);
      UNSAFE_ATOMIC_END;
      #endif
      if (dist > 70) {
        // If dist is way too close, break
        heading = 0;
      }
      else if (dist > 20) {
        //If dist is just a little too close, grab dist and angle together for
        //better fidelity reading
        int16_t gx,gy,gz,ax,ay,az;
        BIT_FLIP(1,0);
        UNSAFE_ATOMIC_START;
        BIT_FLIP(1,0);
        printf("Start\r\n");
        if (STATE_READ(prox) < 1) {
          proximity_sense_init();
          STATE_CHANGE(prox,1);
        }
        if (STATE_READ(imu) < 1) {
          BIT_FLIP(1,0);
          BIT_FLIP(1,0);
          imu_init();// Note: you definitely need to apply Pudu to figure out if
          //__delay_cycles(80000);
          BIT_FLIP(1,0);
          BIT_FLIP(1,0);
        }
      UNSAFE_ATOMIC_END;
      // it's a good idea to toggle here.
        #ifndef TEST_ASYNCH_MULTI
        LIBPACARANA_TOGGLE_PRE(1,imu_reenable);
        read_raw_accel(&ax,&ay,&az);
        for (int i = 0; i < 6; i++) {
          read_raw_gyro(&gx,&gy,&gz);
          printf("%i %i %i\r\n",gx,gy,gz);
          __delay_cycles(8000);
        }
        LIBPACARANA_TOGGLE_POST(imu,1,imu_disable);
        LIBPACARANA_TOGGLE_PRE(3,proximity_sense_reenable);
        dist = proximity_read_byte();
        LIBPACARANA_TOGGLE_POST(prox,3,proximity_sense_disable);
        #else
        //printf("Running asynch!\r\n");
        UNSAFE_ATOMIC_START;
        read_imu_asynch(&gx,&gy,&gz,&ax,&ay,&az);
        dist = read_prox_asynch();
        UNSAFE_ATOMIC_END;
        #endif
        UNSAFE_ATOMIC_START;
        printf("%i %i %i\r\n",gx,gy,gz);
        printf("%i %i %i ; %i %i %i\r\n",gx,gy,gz,ax,ay,az);
        heading = check_heading((int)gx,(int)gy,(int)gz,(int)ax,(int)ay,(int)az,dist);
        UNSAFE_ATOMIC_END;
        // Original attempted fix
        if (!heading) {
          printf("Rip!\r\n");
          sensor_ripcord();
        }
      }
    } while(heading);
    // If all else fails, execute emergency maneuvers
    #ifdef TEST_MULTI_FIX
    UNSAFE_ATOMIC_START;
    sensor_ripcord();
    UNSAFE_ATOMIC_END;
    #endif
    UNSAFE_ATOMIC_START;
    printf("Emergency maneuvers!\r\n");
    BIT_FLIP(1,0);
    for (int i = 0; i < 20; i++) {
      __delay_cycles(80000);
    }
    BIT_FLIP(1,1);
    UNSAFE_ATOMIC_END;
  }
}

void __attribute ((interrupt(TIMER1_A0_VECTOR))) PACARANA_INT Timer1_A0_ISR(void) {
#ifdef TEST_ASYNCH_MULTI
  TA1CCTL0 &= ~CCIE;
  if (STATE_READ(prox) > 0) {
    __internal_dist = proximity_read_byte();
    new_dist = 1;
  }
  if (STATE_READ(imu) > 0) {
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
    new_imu = 1;
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  }
  TA1R = 0;
  TA1CCTL0 |= CCIE;
#endif
}

