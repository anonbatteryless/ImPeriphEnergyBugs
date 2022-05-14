#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
#include <math.h>
#include <libcapybara/board.h>
#include <libcapybara/reconfig.h>
#include <libmspbuiltins/builtins.h>
#include <libfxl/fxl6408.h>
#include <liblsm/accl.h>
#include <liblsm/gyro.h>
#include <libapds/proximity.h>
#include <librustic/checkpoint.h>
//#include <libparticle/particle.h>
#include <libparticle/particle.h>

#define NUM_P 10



#undef __hifram
#define __hifram __nv

/* Turn on perf test if we need all of the debug signals or prints disabled*/
#define PERF_TEST

/* Turn on to test what happens if we're always disabling right after using the
 * peripheral*/
//#define ALWAYS_DISABLE

/* Turn on to test the optimization where we merge everythign together*/
//#define TEST_MERGE

/* Just for testing librustic functionality */
//#define TEST_JIT

/* Enables the sleep optimization where we don't transition back into sleep */
#define SLEEP_OPTIMIZATION

__hifram float photo_field[73] =
{0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31 ,13.75
,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15 ,3.48
,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31
,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15
,3.48 ,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67
,13.31 ,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74
,5.15};


__hifram float accel_cos[73] =
{1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19 ,0.06
,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93 ,-0.97
,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19
,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93
,-0.97 ,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31
,0.19 ,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88
,-0.93 };


capybara_task_cfg_t pwr_configs[3] = {
  CFG_ROW(0, CONFIGD, HIGHP,0),
  CFG_ROW(1, CONFIGD, MEDLOWP2,0),
  CFG_ROW(2, CONFIGD, LOWP,0),
};

/* The following is all part of code that should be included in a driver library
 * but isn't because our LLVM setup leaves something to be desired
 */
#if 1
#include <libpacarana/pacarana.h>
//TODO make this a macro & make sure it lines up with the pass' numbering
//TODO make states the rows and funcs the columns--func numbers are fixed
#ifndef ALWAYS_DISABLE
#include "accel_table.h" /*DL*/
#include "photo_table.h" /*DL*/
int* __paca_all_tables[NUM_PERIPHS] = { /*DL*/
	accel_table, /*DL*/
  photo_table /*DL*/
}; /*DL*/
#else
int* __paca_all_tables[NUM_PERIPHS] = { /*DL*/
};
#endif
#else
#define REGISTER(...)
#define STATE_CHANGE(...)
#define SLEEP(...)
#define RESTORE(...)
#define DRIVER
#define NO_PERIPH
#endif

#define RATE 0x80

REGISTER(photo); /*DL*/
REGISTER(accel); /*DL*/

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(photo),&STATE_READ(accel)};

void delay_1() {
  __delay_cycles(64000);
}

void DRIVER accel_init() {
  int temp = 1;
  while(temp) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    temp = accel_only_init_odr_hm(RATE, 1);
    if(temp) { 
      fxl_clear(BIT_SENSE_SW); 
      __delay_cycles(4800);
      fxl_set(BIT_SENSE_SW);
    }
  }
  //TODO make this correspond to rate codes, 4= 80, 3=40, 2=10
  STATE_CHANGE(accel,4);
}

#ifndef ALWAYS_DISABLE
void a_reenable() {
  //
  if(!STATE_CHECK(accel,0)) {
    fxl_set(BIT_SENSE_SW);
    delay_1();
  }
#if RATE == 0x80
	accel_only_init_odr_hm(RATE,true);
  __delay_cycles(4800); \
	STATE_CHANGE(accel,4);
#elif RATE == 0x40
	accel_only_init_odr_hm(RATE,false);
	STATE_CHANGE(accel,3);
#endif
}
#endif

#ifndef ALWAYS_DISABLE
SLEEP(accel,lsm_accel_disable);
RESTORE(accel,a_reenable);

SLEEP(photo,disable_photoresistor);
RESTORE(photo, enable_photoresistor);
#endif

__hifram float ps[NUM_P];
__hifram float ps2[NUM_P];
__hifram float w[NUM_P];
// Double buffering
__hifram float *cur_ps;
__hifram float *prev_ps;


// FIFO definitions --> just handles pointers, doesn't actually manipulate data
#define FIFO_SIZE 3
#define FULL_CNT 5
#define FIFO(name,struc) \
__hifram struc name ## _fifo[FIFO_SIZE]; \
	__hifram unsigned name ## _it_tail_next = 0;\
	__hifram unsigned name ## _it_tail = 0;\
	__hifram unsigned name ## _it_head = 0;\
	__hifram unsigned name ## _full_cnt = 0;\

#define GET_EMPTY_FIFO(name) \
	&name ## _fifo[name ## _it_tail];

#define	PUSH_FIFO(name,input) \
	if (name ## _it_tail == FIFO_SIZE - 1) {\
		name ## _it_tail_next = 0;\
	} else {\
		name ## _it_tail_next = name ## _it_tail + 1;\
	}\
	if (name ##_it_tail_next == name ## _it_head) {\
		name ## _full_cnt++;\
		name ## _it_tail_next = name ## _it_tail;\
	} else {\
		name ## _it_tail = name ## _it_tail_next;\
	}

#define POP_FIFO(name) \
	&name ## _fifo[name ## _it_head];\
	if (name ## _it_head == FIFO_SIZE - 1) {\
		name ## _it_head = 0;\
	} else {\
		name ## _it_head++;\
	}



FIFO(photo_data, photo_pf_buf);
FIFO(accel_data, accel_pf_buf);

#define FREQkHz 8000

#define ROTATE_4X 32000

#ifdef ALWAYS_DISABLE
#define SLEEP(...)
#define RESTORE(...)
#endif

#ifndef PERF_TEST
#define BIT_FLIP(port,bit) \
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit;
#else
#define BIT_FLIP(...)
#undef PRINTF
#define PRINTF(...)
#endif

__hifram float p_golden;

#ifdef SLEEP_OPTIMIZATION
__nv pacarana_cfg_t inits[4] = {
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, enable_photoresistor, accel_init),
  PACA_CFG_ROW(BIT_SENSE_SW, 1, enable_photoresistor),
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, accel_init),
  PACA_CFG_ROW(0, 0, NULL),
};
#else
// We need the extra states for when we want the accelerometer to wake up in
// sleep mode.  The sure-fire way to know it's alive is to run it through full
// initialization.
__nv pacarana_cfg_t inits[6] = {
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, enable_photoresistor, accel_init),
  PACA_CFG_ROW(BIT_SENSE_SW, 4, delay_1, enable_photoresistor, accel_init,
  lsm_accel_disable),
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, accel_init, lsm_accel_disable),
  PACA_CFG_ROW(BIT_SENSE_SW, 1, enable_photoresistor),
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, accel_init),
  PACA_CFG_ROW(0, 0, NULL),
};
#endif

// For emulation
//__hifram unsigned ran_before = 0;
unsigned ran_before = 0;
void DRIVER event_photo()
{
  int do_init = 0;
  if(STATE_CHECK(photo,2)) {
    do_init = 1;
    #ifdef LIBCAPYBARA_CONT_POWER
    PRINTF("init'ing photo\r\n");
    #endif
  }
  UNSAFE_ATOMIC_START;
#ifndef ALWAYS_DISABLE
  if (do_init) {
    enable_photoresistor();
    // Strictly here because the function is in a library compiled with gcc so
    // we can't analyze it. This change is the driver writer's responsibility
    STATE_CHANGE(photo,2);
  }
#else
  enable_photoresistor();
  // Strictly here because the function is in a library compiled with gcc so
  // we can't analyze it. This change is the driver writer's responsibility
  STATE_CHANGE(photo,2);
#endif
  float photo;
  photo = read_photoresistor_fl();
#ifdef ALWAYS_DISABLE
  disable_photoresistor();
  STATE_CHANGE(photo,0);
#endif
  UNSAFE_ATOMIC_END;
  photo = photo/100;
	photo_pf_buf* out = GET_EMPTY_FIFO(photo_data);
	out->photo = photo;
	PUSH_FIFO(photo_data, out);
}

void DRIVER event_accel()
{ int16_t x,y,z;
  int do_init = 0;
  if(STATE_CHECK(accel,4)) {
    do_init = 1;
  }
  UNSAFE_ATOMIC_START;
#ifndef ALWAYS_DISABLE
  if (do_init) {
    fxl_set(BIT_SENSE_SW); delay_1(); accel_init();
  }
#else
    fxl_set(BIT_SENSE_SW); delay_1(); accel_init();
    STATE_CHANGE(accel,4);
#endif
  accelerometer_read(&x,&y,&z);
  PRINTF("accel vals: %i %i %i\r\n",x,y,z);
#ifdef ALWAYS_DISABLE
  lsm_accel_disable();
  STATE_CHANGE(accel,0);
#endif
  UNSAFE_ATOMIC_END;
	accel_pf_buf* out = GET_EMPTY_FIFO(accel_data);
	out->x = x;
	out->y = y;
	out->z = z;
	PUSH_FIFO(accel_data, out);
}

void event_tx()
{
	// Print for testing
	for (unsigned i = 0; i < 10; ++i) {
    UNSAFE_ATOMIC_START;
		print_float(cur_ps[i]);
    UNSAFE_ATOMIC_END;
	}
	//PRINTF("\r\n");
}

__nv unsigned test_counter = 0;
__nv bool use_accel = false;

#ifdef TEST_MERGE
void merged_func(unsigned param) {
#else
inline void  merged_func() __attribute__((always_inline));
void inline merged_func(unsigned param) {
#endif
  UNSAFE_ATOMIC_START;
  PRINTF("Starting merged\r\n");
  UNSAFE_ATOMIC_END;
  photo_pf_buf* in = POP_FIFO(photo_data);
  float photo = in->photo;
  // JUST A TEST!
  UNSAFE_ATOMIC_START;
  print_float(photo);
  UNSAFE_ATOMIC_END;

  // Move the particles
  // Shifts all of the particles based on the movement model
  UNSAFE_ATOMIC_START;
  P1OUT |= BIT2;
  P1DIR |= BIT2;
  __delay_cycles(ROTATE_4X);
  P1OUT &= ~BIT2;
  PRINTF("Printing particles!\r\n");
  UNSAFE_ATOMIC_END;
  move(param,cur_ps);
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START;
  PRINTF("Done!\r\n");
  UNSAFE_ATOMIC_END;
#endif
  // Incorporate accel results into particle weight calculation
  // Generate particle weight
  if (use_accel) {
    accel_pf_buf* in = POP_FIFO(accel_data);
    float accel = calc_cos(in);
#ifndef PERF_TEST
    PRINTF("start joint\r\n");
#endif
    calc_weights_joint(param, w, photo, accel, cur_ps);
  }
  else {
#ifndef PERF_TEST
    PRINTF("start single\r\n");
#endif
    calc_weights_single(param, w, photo, cur_ps);
  }
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START;
  PRINTF("Got wts!\r\n");
  UNSAFE_ATOMIC_END;
#endif
  // Normalize weights
  float sum = 0;
  for (unsigned i = 0; i < param; ++i) {
    sum += w[i]*w[i];
  }
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START;
  PRINTF("sum: ");
  print_float(sum);
  PRINTF("\r\n");
  UNSAFE_ATOMIC_END;
#endif
  sum = my_sqrt(sum);
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START;
  print_float(sum);
  PRINTF("\r\n");
  UNSAFE_ATOMIC_END;
#endif
  // Check for weight sizes
  // If too small enable accelerometer if it's not already enabled
  if (use_joint(sum)) {
    use_accel = true;
  }
  else {
    use_accel = false;
  }
  resample(param, w, sum, prev_ps, cur_ps);
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START;
  PRINTF("Done resampling!, %i\r\n",use_accel);
  UNSAFE_ATOMIC_END;
#endif

  float *tmp = prev_ps;
  prev_ps = cur_ps;
  cur_ps = tmp;

  event_tx();

  return;
}

// Events and tasks declaration from here
int DRIVER main(void)
{
#if 0
  msp_watchdog_disable(); 
  msp_gpio_unlock();
  msp_clock_setup(); 
#endif
  capybara_init();
  capybara_transition(2);
  P1OUT |= BIT1;
  P1DIR |= BIT1;
  P1OUT &= ~BIT1;
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);
  //Derive state number here
  unsigned cfg = 0;
  // Add in switch statements based on typestates we're tracking
  // We do full re-init if states are supposed to be active, if we're supposed
  // to be sleeping we just chill for now
#ifdef SLEEP_OPTIMIZATION
  switch(((unsigned)(STATE_READ(photo) > 1) << 1) +
    (unsigned)(STATE_READ(accel) > 1)) {
    case 0b11:
      cfg = 0;
      BIT_FLIP(1,4);
      BIT_FLIP(1,4);
      break;
    case 0b10:
      BIT_FLIP(1,5);
      BIT_FLIP(1,4);
      // Since accel has a sleep state (1), and we don't want to turn it on to
      // have to sleep it, we're just setting this to 0.
      STATE_CHANGE(accel,0);
      cfg = 1;
      break;
    case 0b01:
      BIT_FLIP(1,4);
      BIT_FLIP(1,5);
      cfg = 2;
      break;
    default:
      BIT_FLIP(1,5);
      BIT_FLIP(1,5);
      STATE_CHANGE(accel,0);
      cfg = 3;
      break;
  }
#else
  if ( STATE_READ(photo) > 0) {
    switch(STATE_READ(accel)) {
      case 0:
        cfg = 3;
        break;
      case 1:
        cfg = 1;
        break;
      case 2:
      case 3:
      case 4:
        cfg = 0;
        break;
      default:
        cfg = 5;
        break;
    }
  }
  else {
    switch(STATE_READ(accel)) {
      case 0:
        cfg = 5;
        break;
      case 1:
        cfg = 2;
        break;
      case 2:
      case 3:
      case 4:
        cfg = 4;
        break;
      default:
        cfg = 5;
        break;
    }
  }
#endif
  paca_init(cfg);
#ifdef LIBCAPYBARA_CONT_POWER
  PRINTF("done init!\r\n");
#endif
  BIT_FLIP(1,4);
  entry();
  // Only added to square up starting state
  STATE_CHANGE(photo,0);
  STATE_CHANGE(accel,0);
  BIT_FLIP(1,4);
#ifdef TEST_JIT
  while(1) {
    PRINTF("ST\r\n");
    for (unsigned i = 0; i < 50; i++) {
      test_counter++;
      BIT_FLIP(1,0);
      for (unsigned j = 0; j < 4;j++) {
        if (test_counter & 1<< j){
          BIT_FLIP(1,5);
        }
        else {
          BIT_FLIP(1,4);
        }
      }

      if (test_counter > 0xF) {
        test_counter = 0;
      }
    }
    UNSAFE_ATOMIC_START;
    for(int i = 0; i < 25; i++) {
      PRINTF("50\r\n");
      test_counter++;
      if (test_counter > 0xfff4) {
        test_counter = 0;
      }
    }
    UNSAFE_ATOMIC_END;
  }
#endif
  unsigned param = 10;
  // Init particles
  ran_before = 1;
  for (unsigned i = 0; i < param; ++i) {
    float temp;
    temp = get_random_pos();
    ps[i] = temp;
  }
  cur_ps = ps;
  prev_ps = ps2;

  while(1) {
    /*P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;*/
    event_photo();
    // Pull result from e_mic
    if (use_accel) {
      event_accel();
    }
    merged_func(param);
    P1OUT |= BIT0;
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;
  }
  return 0;
}


