// File for testing performance using JIT checkpointing and intermittent power
// We're only testing with the gyro for now
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
#include <libcapybara/reconfig.h>
// Libraries for handling matrices and fixed point calculations
#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include "harness.h"
// Pacarana library
#ifdef PACARANA
#include <libpacarana/pacarana.h>
#endif
// Libraries for the gyro since this only works for the gyro right now
#include <liblsm/gyro.h>
#include <libfxl/fxl6408.h>
#include <librustic/checkpoint.h>

#ifdef SENSE_FUNCS
#include SENSE_FUNCS
#warning "including sense_funcs"
#endif

#ifdef FUNCS
#include FUNCS
#else
#include "common/mat_externs.h"
#endif
//#include "common/mat_externs.h"

// Dummy function
/*
void increment(int count) {
	count++;
}
*/
#ifndef NUM_TRIALS
#warning "NUM_TRIALS undefined- running default"
#define NUM_TRIALS 100
#endif

#ifndef FUNC
#warning "FUNC_UT undefined- running default"
#define FUNC increment
#endif

#ifndef INPUTS_UT
#warning "INPUTS_UT undefined- running default"
#define INPUTS_UT 0
#endif

#ifndef FUNC_PATH
#error "Nothing to test, no path defined"
#endif

#define FUNC_UT(...) FUNC(__VA_ARGS__)

#ifdef SETUP_FUNC
#define RUN_SETUP(...) SETUP_FUNC(__VA_ARGS__)
#else
#warning "setup func undefined!"
#define RUN_SETUP(...) do{} while(0);
#endif

#ifdef CHECK_FUNC
#define RUN_CHECK(...) CHECK_FUNC(__VA_ARGS__)
#else
#warning "Check function undefined!!"
#define RUN_CHECK(...) do{} while(0);
#endif

#ifdef CLEANUP_FUNC
#define RUN_CLEANUP(...) CLEANUP_FUNC(__VA_ARGS__)
#else
#warning "Cleanup function undefined!!"
#define RUN_CLEANUP(...) do{} while(0);
#endif

#ifdef PACARANA
//TODO make this a macro & make sure it lines up with the pass' numbering
//TODO make states the rows and funcs the columns--func numbers are fixed
#include "accel_table.h"
#include "gyro_table.h"
int* __paca_all_tables[NUM_PERIPHS] = {
	accel_table,
  gyro_table,
};
#endif

capybara_task_cfg_t pwr_configs[3] = {
  CFG_ROW(0, CONFIGD, HIGHP,0),
  CFG_ROW(1, CONFIGD, MEDLOWP2,0),
  CFG_ROW(2, CONFIGD, LOWP,0),
};

void delay_1() {
  __delay_cycles(64000);
}

void DRIVER gyro_init() {
  gyro_init_data_rate_hm(RATE, RATE & HIGHPERF_MASK);
  STATE_CHANGE(gyro,4);
  //PRINTF("G1: %i\r\n",STATE_READ(gyro));
}

__nv pacarana_cfg_t inits[3] = {
  // 0-- gyro is fully disabled
  PACA_CFG_ROW(0, 0, NULL),
  //PACA_CFG_ROW(0, 1, delay_1),
  //PACA_CFG_ROW(BIT_SENSE_SW,1, delay_1),
  //PACA_CFG_ROW(BIT_SENSE_SW,0, NULL),
  // 1-- gyro is enabled
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, gyro_init),
  // 2-- gyro is on in sleep mode
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, gyro_init, g_disable),
};

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(gyro)};

__nv uint8_t capy_cfg = 0;

int DRIVER main(void) {
	capybara_init();
#ifndef HPVLP
  PRINTF("||||||\r\n");
#endif
  BIT_FLIP(1,0);
  capybara_transition(capy_cfg);
  BIT_FLIP(1,0);
  //PRINTF("0x%x %u (out)\r\n", SEMI_SAFE_ATOMIC_BUFFER[0], *SEMI_SAFE_ATOMIC_BUFFER[0]);
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);
  //PRINTF("0x%x %u (out)\r\n", SEMI_SAFE_ATOMIC_BUFFER[0], *SEMI_SAFE_ATOMIC_BUFFER[0]);
  volatile unsigned cfg = 0;
#ifndef HPVLP
  PRINTF("State gyro %u\r\n",STATE_READ(gyro));
#endif
#ifdef USE_SENSORS
	//Initialize sensors here
  #ifdef SLEEP_OPT
  switch(STATE_READ(gyro)) {
    case 0:
      cfg = 0;
      break;
    case 1:
      STATE_CHANGE(gyro,0);
      cfg = 0;
      break;
    case 4:
      cfg = 1;
      break;
    default:
      cfg = 0;
      break;
  }
  #else
  switch(STATE_READ(gyro)) {
    case 0:
      cfg = 0;
      break;
    case 1:
      cfg = 2;
      break;
    case 4:
      cfg = 1;
      break;
    default:
      cfg = 0;
      break;
  }
  #endif
  paca_init(cfg);
  #ifndef HPVLP
  PRINTF("Config: %u\r\n",cfg);
  #endif
#endif
  int flag = 0;
  /*BIT_FLIP(1,3);
  for (int i = 0; i < STATE_READ(gyro); i++) {
    BIT_FLIP(1,3);
  }*/
  //PRINTF("Gn: %i\r\n",STATE_READ(gyro));
  entry();
  //volatile int _temp0;
  STATE_CHANGE(gyro,0);
  // Just for the first go-round
  //PRINTF("here0\r\n");
  UNSAFE_ATOMIC_START;//Start
  //volatile int _temp1;
  __delay_cycles(48000);
  __delay_cycles(48000);
  fxl_set(BIT_SENSE_SW);
  __delay_cycles(48000);
  gyro_init();
  //volatile int _temp2;
  UNSAFE_ATOMIC_END;//Stop
#if 0
  int counter = 0;
  //Test code
  while(1) {
    counter++;
    PRINTF("Running jit %i\r\n", counter);
    for(int i = 0; i < 100; i++) {
      __delay_cycles(48000);
    }
    if(counter == 10) {
      if (STATE_READ(gyro) == 4) {
        UNSAFE_ATOMIC_START;//Start
        PRINTF("Disabling\r\n");
        SENSOR_DISABLE();
        UNSAFE_ATOMIC_END;//End
      }
      else {
        UNSAFE_ATOMIC_START; //Start
        P1OUT |= BIT2;
        P1DIR |= BIT2;
        //PRINTF("run\r\n");
        fxl_set(BIT_SENSE_SW);
        __delay_cycles(48000);
        gyro_init();
		    SHORT_DATA_FUNC(SENSOR_SAMPLE,SHORT_SAMPLE_LEN);
        P1OUT &= ~BIT2;
        //PRINTF("done\r\n");
        UNSAFE_ATOMIC_END;//Done
      }
      BIT_FLIP(1,2);
      counter = 0;
    }
  }
#endif
	// Run setup code
  BIT_FLIP(1,3);
  BIT_FLIP(1,1);
  BIT_FLIP(1,3);
	RUN_SETUP();
	// Loop forever
#ifdef USE_SENSORS
		// Add code here to read in data from sensors
		// DATA_FUNC is defined for each function under test
		// SENSOR_SAMPLE is a function pointer that is defined for each peripheral
    BIT_FLIP(1,3);
		DATA_FUNC(SENSOR_SAMPLE);
    BIT_FLIP(1,3);
#endif
    // Switch to low power cfg after sampling
	for(int i = 0; i < NUM_TRIALS; i++) {
#ifndef HPVLP
    PRINTF("start trial %i\r\n",i);
#endif
		// Indicate start of test run
		BIT_FLIP(1,1);
#if defined(USE_SENSORS) && defined(REENABLE)
		// Add code here to disable sensors
    #ifndef SLEEP_OPT
		SENSOR_DISABLE();
    #else
    #warning "sleep opt enabled"
    //PRINTF("Gn+1: %i\r\n",STATE_READ(gyro));
    if (!STATE_CHECK(gyro,4)) {
      //PRINTF("here1\r\n");
      UNSAFE_ATOMIC_START;//Start
#ifndef HPVLP
      PRINTF("Disabling\r\n");
#endif
      SENSOR_DISABLE();
      //PRINTF("GN+1: %i\r\n",STATE_READ(gyro));
      UNSAFE_ATOMIC_END;//Stop
    }
    #endif
#endif
		BIT_FLIP(1,2);
		FUNC_UT(INPUTS_UT);
		RUN_CHECK();
		// we add a cleanup after check because cleanup may clear out crucial
		// variables
		RUN_CLEANUP();
    BIT_FLIP(1,4);
#if defined(USE_SENSORS) && defined(REENABLE)
    //PRINTF("Reenable\r\n");
		// Add code here to enable sensors
    #ifndef SLEEP_OPT
		SENSOR_REENABLE();
    #else
    UNSAFE_ATOMIC_START;//Start
    //PRINTF("here2\r\n");
    BIT_FLIP(1,3);
    //PRINTF("Reen gy: %i-- %i\r\n",STATE_READ(gyro),permanent_dbg);
    if(!STATE_CHECK(gyro,0)) {
      __delay_cycles(48000);
      __delay_cycles(48000);
      fxl_set(BIT_SENSE_SW);
      __delay_cycles(48000);
      gyro_init();
    }
    else {
      SENSOR_REENABLE();
#ifndef HPVLP
      PRINTF("Reenabling2\r\n");
#endif
    }
      UNSAFE_ATOMIC_END;//Stop
    #endif
#endif
#ifdef USE_SENSORS
#ifndef SHORT_SAMPLE
		// Add code here to read in data from sensors
    // Note: added atomic_start/end calls in data_funcs
		DATA_FUNC(SENSOR_SAMPLE);
#else
    //volatile int _loc0 = 1;
    //PRINTF("here3 %x\r\n", STATE_READ(gyro));
    //UNSAFE_ATOMIC_START;
    //volatile int _loc1 = 1;
		SHORT_DATA_FUNC(SENSOR_SAMPLE,SHORT_SAMPLE_LEN);
    //volatile int _loc2 = 1;
    //UNSAFE_ATOMIC_END;
    //volatile int _loc3 = 1;
#ifndef HPVLP
    PRINTF("Done short! %i\r\n",i);
#endif
#endif
#endif
		BIT_FLIP(1,1);
	}
  //PRINTF("Done!\r\n");
  while(1);
}
