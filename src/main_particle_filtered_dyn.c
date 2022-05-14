// Particle filter application for testing with Pudu dynamic, circa April 2021
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
/*Test with Pudu timer functionality*/
//#define TEST_PUDU
#ifndef TEST_PUDU
#undef RUN_PACA_SEPARATE_PF
#define RUN_PACA_SEPARATE_PF
#endif

//#define FINAL_PHASE
/* Turn on perf test if we need all of the debug signals or prints disabled*/
//#define PERF_TEST

/* Turn on to test what happens if we're always disabling right after using the
 * peripheral*/
//#define ALWAYS_TOGGLE


/* Just for testing librustic functionality */
//#define TEST_JIT


__hifram float photo_field[73] =
{0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31 ,13.75
,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15 ,3.48
,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31
,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15
,3.48 ,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67
,13.31 ,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74
,5.15};


/*__hifram float accel_cos[73] =
{1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19 ,0.06
,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93 ,-0.97
,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19
,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93
,-0.97 ,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31
,0.19 ,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88
,-0.93 };
*/


/* The following is all part of code that should be included in a driver library
 * but isn't because our LLVM setup leaves something to be desired
 */
#if 1
#include <libpacarana/pacarana.h>
int* __paca_all_tables[NUM_PERIPHS] = { /*DL*/
};
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

PACA_SET_STATUS_PTRS = {&accel_status,NULL,NULL,NULL,&photo_status};

SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(photo),&STATE_READ(accel)};

void delay_1() {
  __delay_cycles(64000);
}

void DRIVER accel_init() {
  int temp = 1;
  while(temp) {
    temp = accel_only_init_odr_hm(RATE, 1);
    if(temp) { 
      fxl_clear(BIT_SENSE_SW); 
      __delay_cycles(4800);
      fxl_set(BIT_SENSE_SW);
    }
  }
  //TODO make this correspond to rate codes, 4= 80, 3=40, 2=10
  STATE_CHANGE(accel,1);
}

void a_reenable() {
  if (STATE_READ(accel) < 0) {
    fxl_set(BIT_SENSE_SW);
    delay_1();
    accel_init();
  }
  else {
#if RATE == 0x80
	accel_only_init_odr_hm(RATE,true);
  __delay_cycles(4800); \
	STATE_CHANGE(accel,1);
#elif RATE == 0x40
	accel_only_init_odr_hm(RATE,false);
	STATE_CHANGE(accel,2);
#endif
  }
}


void a_disable() {
  if (STATE_READ(accel) != -1) {
    lsm_accel_disable();
    STATE_CHANGE(accel,0);
  }
}

void photo_reenable() {
  enable_photoresistor();
  STATE_CHANGE(photo,1);
}
void photo_disable() {
  disable_photoresistor();
  STATE_CHANGE(photo,0);
}

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

__nv pacarana_cfg_t inits[5] = {
/*0*/  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1, enable_photoresistor, accel_init),
/*1*/  PACA_CFG_ROW(BIT_SENSE_SW, 1, enable_photoresistor),
/*2*/  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1, accel_init),
/*3*/  PACA_CFG_ROW(BIT_SENSE_SW, 1, delay_1),
/*4*/  PACA_CFG_ROW(0, 0, NULL),
};


#ifdef TEST_PUDU
#define READ_PHOTO_PRE LIBPACARANA_TOGGLE_PRE(4,photo_reenable);
#define READ_ACCEL_PRE LIBPACARANA_TOGGLE_PRE(0,a_reenable);

#define READ_PHOTO_POST LIBPACARANA_TOGGLE_POST(photo,4,photo_disable);
#define READ_ACCEL_POST LIBPACARANA_TOGGLE_POST(accel,0,a_disable);

#else
  #ifdef ALWAYS_TOGGLE
    #define READ_PHOTO_PRE \
      UNSAFE_ATOMIC_START; photo_reenable();
    #define READ_ACCEL_PRE \
      UNSAFE_ATOMIC_START; a_reenable();

    #define READ_PHOTO_POST \
      photo_disable(); printf("%u",__paca_index_extended);\
      UNSAFE_ATOMIC_END;
    #define READ_ACCEL_POST \
      a_disable(); printf("%u",__paca_index_extended);UNSAFE_ATOMIC_END;

  #else //No toggle
    #define READ_PHOTO_PRE UNSAFE_ATOMIC_START;
    #define READ_ACCEL_PRE UNSAFE_ATOMIC_START;

    #define READ_PHOTO_POST printf("%u",__paca_index_extended); UNSAFE_ATOMIC_END;
    #define READ_ACCEL_POST printf("%u",__paca_index_extended); UNSAFE_ATOMIC_END;

  #endif
#endif


// For emulation
unsigned ran_before = 0;
void DRIVER event_photo()
{
  float photo;
  READ_PHOTO_PRE; photo = read_photoresistor_fl(); READ_PHOTO_POST;
  photo = photo/100;
	photo_pf_buf* out = GET_EMPTY_FIFO(photo_data);
	out->photo = photo;
	PUSH_FIFO(photo_data, out);
}

void DRIVER event_accel()
{
  int16_t x,y,z;
  READ_ACCEL_PRE; accelerometer_read(&x,&y,&z); READ_ACCEL_POST;
#ifndef PERF_TEST
  UNSAFE_ATOMIC_START; PRINTF("accel vals: %i %i %i\r\n",x,y,z);
  UNSAFE_ATOMIC_END;
#endif
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
#ifndef FINAL_PHASE
__nv bool use_accel = false;
#else
__nv bool use_accel = true;
#endif
__nv int16_t last_z = 0;
#define MAX_CHANGE 500

inline void  merged_func() __attribute__((always_inline));
void inline merged_func(unsigned param) {
  UNSAFE_ATOMIC_START;
  PRINTF("Starting merged\r\n");
  UNSAFE_ATOMIC_END;
  photo_pf_buf* in = POP_FIFO(photo_data);
  float photo = in->photo;

  // Move the particles
  // Shifts all of the particles based on the movement model
  //NOTE: to simulate a ~50uJ actuation, we need to connect a 470 Ohm resistor
  //from P1.1 to ground, then we'll set it high for 4ms (ROTATE_4X) to hit ~50uJ
  UNSAFE_ATOMIC_START;
  P1OUT |= BIT1;
  P1DIR |= BIT1;
  __delay_cycles(ROTATE_4X);
  P1OUT &= ~BIT1;
  UNSAFE_ATOMIC_END;
  // Incorporate accel results into particle weight calculation
  // Generate particle weight
  if (use_accel) {
    for (int i = 0; i < 100; i++){
      int16_t x,y,z;
      READ_ACCEL_PRE;
      accelerometer_read(&x,&y,&z);
      READ_ACCEL_POST;
      // Check change in z acceleration
      if (last_z == 0) {last_z = z;} // Easy way to clear initial 0
      int16_t diff = last_z - z;
      if (diff < 0) { diff = diff*-1;}
      if (diff > 0 && diff >MAX_CHANGE) {break; }
      last_z = z;
    }
#ifndef FINAL_PHASE
    use_accel = false;
#else
    use_accel = true;
#endif
  }
  else {
  UNSAFE_ATOMIC_START;
  PRINTF("Moving particles!\r\n");
  UNSAFE_ATOMIC_END;
  move(param,cur_ps);
#if 0
  UNSAFE_ATOMIC_START;
  PRINTF("Before pick!\r\n");
  UNSAFE_ATOMIC_END;
#endif
#ifndef PERF_TEST
    PRINTF("start single\r\n");
#endif
    calc_weights_single(param, w, photo, cur_ps);
    use_accel = true;
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
    resample(param, w, sum, prev_ps, cur_ps);
#ifndef PERF_TEST
    UNSAFE_ATOMIC_START;
    PRINTF("Done resampling!, %i\r\n",use_accel);
    UNSAFE_ATOMIC_END;
#endif

    float *tmp = prev_ps;
    prev_ps = cur_ps;
    cur_ps = tmp;
  }
  event_tx();

  return;
}
__nv int gnded_flag = 0;

// Events and tasks declaration from here
int DRIVER main(void)
{
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
  P1OUT &= ~BIT2;
  P1REN &= ~BIT2;//disable pull up
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);
  //Derive state number here
  RUN_PACA_SEPARATE_PF;
  printf("Capy init! %i %i\r\n",STATE_READ(photo),STATE_READ(accel));
  unsigned cfg = 0;
  // Add in switch statements based on typestates we're tracking
  // We do full re-init if states are supposed to be active, if we're supposed
  // to be sleeping we just chill for now
  if (STATE_READ(accel) > 0) {
    if (STATE_READ(photo) > 0) {
      cfg = 0;
    }
    else {
      STATE_CHANGE(photo,-1);
      cfg = 2;
    }
  }
  else {
    if (STATE_READ(photo) > 0) {
      STATE_CHANGE(accel,-1);
      cfg = 1;
    }
    else {
      STATE_CHANGE(accel,-1);
      STATE_CHANGE(photo,-1);
      cfg = 4;
    }
  }
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  BIT_FLIP(1,5);
#ifdef LIBCAPYBARA_CONT_POWER
  PRINTF("done init!\r\n");
#endif
  entry();
  unsigned param = 10;
#ifndef TEST_PUDU
  __paca_index_extended = 15;//handicap because pudu has to print paca_extended
#endif
  // Init particles
  ran_before = 1;
  for (unsigned i = 0; i < param; ++i) {
    float temp;
    temp = get_random_pos();
    ps[i] = temp;
  }
  cur_ps = ps;
  prev_ps = ps2;

  for(int __i = 0; __i < 100; __i++) {
    BIT_FLIP(1,0);
    BIT_FLIP(1,0);
  UNSAFE_ATOMIC_START;
  PRINTF("loop start\r\n");
  UNSAFE_ATOMIC_END;

    event_photo();
    merged_func(param);
    BIT_FLIP(1,0);
  UNSAFE_ATOMIC_START;
  PRINTF("loop end\r\n");
  UNSAFE_ATOMIC_END;
  }
  while(1) {
    printf("done\r\n");
  }
  return 0;
}


