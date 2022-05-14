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

// Toss in any header files we need to have access to globals here
#include "common/mat_externs.h"
#include "resample_funcs.h"
#include "pack_funcs.h"

// Dummy function
/*
void increment(int count) {
	count++;
}
*/
#ifndef NUM_TRIALS
#warning "NUM_TRIALS undefined- running default"
#define NUM_TRIALS 10
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


void fft_init(void);
void fft_output_check(void);
void fft1d(mat_t *dest_real, mat_t *dest_imag, mat_t *inter_real, mat_t *inter_imag,
	uint16_t row_idx, uint16_t col_idx, uint16_t num_elements, uint16_t stride);

int main(void) {
	capybara_init();
	printf("Start no sensors\r\n");
	// Run setup code
	BIT_FLIP(1,0);
	RUN_SETUP();
	// Loop forever
	while(1) {
		// Indicate start of test run
		BIT_FLIP(1,0);
		for (int i_ = 0; i_ < NUM_TRIALS; i_++) {
			BIT_FLIP(1,1);
			FUNC_UT(INPUTS_UT);
			BIT_FLIP(1,2);
			if (i_ != NUM_TRIALS - 1) {
				RUN_CLEANUP();
			}
		}
			RUN_CHECK();
			// we add a cleanup after check because cleanup may clear out crucial
			// variables
			RUN_CLEANUP();
	}
}
