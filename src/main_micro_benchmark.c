// This file is for testing if our methodology for finding the break even point
// between enabling/disabling peripherals and leaving them running is correct

// Built-in libraries for the msp430
#include <msp430.h>
#include <libmspbuiltins/builtins.h>
#include <libmsp/clock.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
// Functions for the capybara board
#include <libcapybara/board.h>
// Functions for the accelerometer
#include <liblsm/accl.h>
#include <liblsm/gyro.h>
// Definitions supporting the alpaca language
#include <libalpaca/alpaca.h>

#include "harness.h"
//#include <libpacarana/pacarana.h>
void init();

#define HIGHPERF_MASK 0b10000000

TASK(task_measure)
TASK(task_calc)
TASK(task_end)
TASK(task_profile)

#ifndef WORKLOAD_CYCLES
#define WORKLOAD_CYCLES 100
#endif


#ifndef ITER_START
// This sets the starting point for the number of iterations, if we don't set
// ITER_INC then this is the constant iteration count
#warning "ITER_START UNDEFINED"
#define ITER_START 200
#endif


__nv int stored_x;
__nv int stored_y;
__nv int stored_z;
__nv unsigned count = 0;
__nv unsigned long ITER = ITER_START;
// This function runs first on every reboot
void init() {
  capybara_init();
  SENSOR_INIT();
}


// Reads from the accelerometer
void task_measure() {
  uint16_t x,y,z;
#ifdef REENABLE
  SENSOR_REENABLE()
#endif
  BIT_FLIP(1,1);
  for (int i = 0; i < 10; i++) {
    SENSOR_MULTI_SAMPLE(x,y,z);
    if (i > 2) {
      stored_x = x;
      stored_y = y;
      stored_z = z;
    }
    else {
      __delay_cycles(100);
    }
  }
	// Adding a 2ms delay to force a settle
	for (int i = 0; i < 800; i++) {}
  BIT_FLIP(1,2);
#ifdef REENABLE
  SENSOR_DISABLE();
#endif
  TRANSITION_TO(task_calc);
}

void task_calc() {
  uint16_t x,y,z;
  for (unsigned long j = 0; j < ITER; j++) {
    x++;
    y++;
    z++;
    if (x > 50000) {
      x = 0;
      y = 0;
      z = 0;
    }
    stored_x = x;
    stored_y = y;
    stored_z = z;
  }
  if (count < WORKLOAD_CYCLES) {
    count++;
    TRANSITION_TO(task_measure);
  }
  else {
    TRANSITION_TO(task_end);
  }
}

void task_end() {
  BIT_FLIP(1,0);
  count = 0;
  TRANSITION_TO(task_measure);
}

void task_profile() {
  BIT_FLIP(1,0);
#ifndef REENABLE
#ifndef PROF_COMPUTE
  for (int i = 0; i <  50; i++) {
    __delay_cycles(1000);
  }
#else
  uint16_t x,y,z;
  for (int j = 0; j < ITER; j++) {
    x++;
    y++;
    z++;
    if (x > 50000) {
      x = 0;
      y = 0;
      z = 0;
    }
    stored_x = x;
    stored_y = y;
    stored_z = z;
  }
#endif
#else
  // Second phase
  SENSOR_REENABLE();
  __delay_cycles(100);
  SENSOR_DISABLE();
  for (int i = 0; i <  50; i++) {
    __delay_cycles(1000);
  }
#endif
  TRANSITION_TO(task_profile);
}

#ifndef PROF
ENTRY_TASK(task_measure)
#else
ENTRY_TASK(task_profile)
#endif
INIT_FUNC(init)

