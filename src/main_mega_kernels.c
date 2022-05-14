// Harness for the kernels that change size
// Runs without INIT_0 in libpacarana
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
#include <libradio/radio.h>
#include <libapds/gesture.h>
// Libraries for handling matrices and fixed point calculations
#include <libfixed/fixed.h>
#include <libmat/mat.h>
#include "harness.h"
// Pacarana library
#include <libpacarana/pacarana.h>
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

#define NUM_INNER_LOOP 10

void delay_1() {
  __delay_cycles(48000);
}

void g_init_fast() {
	gyro_only_init_odr_hm(0x80, 1);\
	__delay_cycles(19200);
  STATE_CHANGE(gyro,3);
}

int mat_sizes[3] = {16,64,128};
// Decrease trials of benchmark as we increase size
int num_inner_trials[3] = {20,5,1};

int* __paca_all_tables[NUM_PERIPHS] = {
};

__nv pacarana_cfg_t inits[4] = {
  // 0-- gyro is fully disabled
  PACA_CFG_ROW(0, 0, NULL),
  // 1-- gyro is enabled
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, g_init),
  // 2-- gyro is on in sleep mode
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, g_init, g_disable),
  //3 -- a temp thing for running slow
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, g_init_fast),
};


// Select one of these to run
//#define TEST_PUDU
//#define TEST_NEVER_TOGGLE
#define TEST_ALWAYS_TOGGLE


#ifndef TEST_PUDU
#undef RUN_PACA_SEPARATE_PF
#define RUN_PACA_SEPARATE_PF
#endif

#ifdef TEST_PUDU
#pragma info "test pudu!"
#if RATE == 0x80
#define PRE_USE LIBPACARANA_TOGGLE_PRE(1,g_reenable)
#define POST_USE LIBPACARANA_TOGGLE_POST(gyro,1,g_disable)
#elif RATE == 0x40
#define PRE_USE LIBPACARANA_TOGGLE_PRE(1,g_reenable)
#define POST_USE LIBPACARANA_TOGGLE_POST(gyro,1,g_disable)
#endif//RATE
#elif defined(TEST_NEVER_TOGGLE)
#pragma info "never toggle!"
#define PRE_USE UNSAFE_ATOMIC_START
#define POST_USE UNSAFE_ATOMIC_END
#elif defined(TEST_ALWAYS_TOGGLE)
#pragma info "always toggle!"
#define PRE_USE UNSAFE_ATOMIC_START; g_reenable();
#define POST_USE g_disable(); UNSAFE_ATOMIC_END;
#else
#error "main_mega_kernel: No toggling procedure defined!"
#endif

REGISTER(accel);
REGISTER(led);//dummy

PACA_SET_STATUS_PTRS = {&accel_status,&gyro_status,NULL,NULL,NULL};

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(gyro),&STATE_READ(accel),&STATE_READ(led)};

__nv int gnded_flag = 0;

void main() {
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
  P1REN &= ~BIT2;//disable pull up
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);//TODO make this init real
  RUN_PACA_SEPARATE_PF;
  printf("Capy init! %i %i\r\n",STATE_READ(gyro),STATE_READ(accel));
  unsigned cfg = 0;
  if (STATE_READ(gyro) > 0) {
    if (STATE_READ(gyro)==3) {
      cfg = 3;
    }
    else {
      cfg = 1;
    }
  }
  else {
    cfg = 0;
    STATE_CHANGE(gyro,-1);
  }
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  BIT_FLIP(1,5);
  entry();
  UNSAFE_ATOMIC_START;
#if RATE == 0x40 // Run initial sampling at reasonable speed
	do { \
  __delay_cycles(48000);\
  __delay_cycles(48000);\
  fxl_set(BIT_SENSE_SW);\
  __delay_cycles(48000);\
	gyro_only_init_odr_hm(0x80, 1);\
	__delay_cycles(19200);\
	STATE_CHANGE(gyro,3);\
	} while(0);
#else
  SENSOR_INIT();
#endif
  UNSAFE_ATOMIC_END;
  int mat_dim =  mat_sizes[2];
  UNSAFE_ATOMIC_START;
  printf("Running setup!\r\n");
  UNSAFE_ATOMIC_END;
  RUN_SETUP(mat_dim);
  UNSAFE_ATOMIC_START;
  printf("Sampling snsors\r\n");
  UNSAFE_ATOMIC_END;
  DATA_FUNC(SENSOR_SAMPLE, mat_dim);// Already instrumented
  UNSAFE_ATOMIC_START;
#if RATE == 0x40
  g_init();
#else
  SENSOR_INIT();//switch back to slow version
#endif
  printf("Done sampling\r\n");
  UNSAFE_ATOMIC_END;
  size_t cur_mat_ind = 0;
  for(int i = 0; i < NUM_TRIALS; i++) {
		BIT_FLIP(1,1);
    mat_dim = mat_sizes[cur_mat_ind];
    UNSAFE_ATOMIC_START;
    printf("----mat size: %i----\r\n",mat_dim);
    UNSAFE_ATOMIC_END;
    for (int j = 0; j < num_inner_trials[cur_mat_ind]; j++) {
      PRE_USE;
      SHORT_DATA_FUNC(SENSOR_SAMPLE,SHORT_SAMPLE_LEN,mat_dim);
      POST_USE;
      //INPUTS: mat_dim, Rows, cols, mat_inputs
      BIT_FLIP(1,0);
      BIT_FLIP(1,0);
      FUNC_UT(mat_dim,INPUTS_UT);
      BIT_FLIP(1,0);
      RUN_CHECK(mat_dim);
    }
    if (cur_mat_ind < 2) {
      cur_mat_ind++;
    }
    else {
      cur_mat_ind = 0;
    }
		BIT_FLIP(1,1);
  }
  while(1){
    printf("Done!\r\n");
  };
}
