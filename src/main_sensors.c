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
// Libraries for the gyro and apds
#include <liblsm/gyro.h>
#include <liblsm/accl.h>
#include <libapds/proximity.h>
#include <libapds/color.h>
#include "temp_sensor.h"
#include <libfxl/fxl6408.h>

#ifdef SENSE_FUNCS
#include SENSE_FUNCS
#warning "including sense_funcs"
#endif

#ifdef FUNCS
#include FUNCS
#else
#include "common/mat_externs.h"
#endif

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
#endif

int DRIVER main(void) {
	capybara_init();
	printf("capybara init'd\r\n");
#ifdef USE_SENSORS
	//Initialize sensors here
	SENSOR_INIT();
	printf("Sensors initialized!\r\n");
#endif
	// Run setup code
	BIT_FLIP(1,0);
	BIT_FLIP(1,0);
	RUN_SETUP();
	// Loop forever
		BIT_FLIP(1,0);
#ifdef USE_SENSORS
		// Add code here to read in data from sensors
		// DATA_FUNC is defined for each function under test
		// SENSOR_SAMPLE is a function pointer that is defined for each peripheral
		DATA_FUNC(SENSOR_SAMPLE);
#endif
	while(1) {
		// Indicate start of test run
		BIT_FLIP(1,1);
#if defined(USE_SENSORS) && defined(REENABLE)
		// Add code here to disable sensors
		SENSOR_DISABLE();
#endif
		BIT_FLIP(1,2);
		FUNC_UT(INPUTS_UT);
		RUN_CHECK();
		// we add a cleanup after check because cleanup may clear out crucial
		// variables
		RUN_CLEANUP();
		BIT_FLIP(1,4);
#if defined(USE_SENSORS) && defined(REENABLE)
		// Add code here to enable sensors
		SENSOR_REENABLE();
#endif
		BIT_FLIP(1,0);
#ifdef USE_SENSORS
#ifndef SHORT_SAMPLE
		// Add code here to read in data from sensors
		DATA_FUNC(SENSOR_SAMPLE);
#else
		SHORT_DATA_FUNC(SENSOR_SAMPLE,SHORT_SAMPLE_LEN);
#endif
#endif
	}
}
