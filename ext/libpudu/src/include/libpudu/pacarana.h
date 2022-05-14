#ifndef _PACARANA_H_
#define _PACARANA_H_
#include <libmsp/mem.h>
#include <librustic/checkpoint.h>
#include <stdint.h>
#include <msp430.h>
#include <stdarg.h>
#include <stdio.h>

//TODO the definitions from the makefile aren't getting carried through, so
//we'll just dump them here (again) and deal with this later
#define LIBPACARANA_FULLY_DYNAMIC
#define LIBPACARANA_MULTI_CNT
#define LIBPACARANA_SEPARATE_PF

/*
  @brief Used to change the state of a device
  @args
    -device needs to be the prefix for all of the objects referring to the
    device, e.g. "gyro" will access gyro_states and gyro_status
    -new_status needs to be a valid state from the user defined "device"_states
    enum
*/
#define STATE_CHANGE(device, new_status) \
 device ## _status = new_status;

/*
  @brief checks if the current state of the device matches the state it should
  be in
  @args
    -device needs to be the prefix for the object referring to the device
    -correct is the state the device should be in
  @returns returns a 1 if device is not in the correct state
*/
#define STATE_CHECK(device, correct) \
  (device ## _status == correct) ? 0 : 1

/*
  @brief dumps the current state
*/
#define STATE_READ(device) \
  device ## _status

/*
  @brief checks the adjacency matrix to see if the transition into this state is
         legal
  @args
    -device needs to be the prefix...
    -new_status is the status the program wants to transition into
  @returns a 1 if transition is not legal
*/
#define STATE_CHECK_ALL(device,new_status) \
  ((1 << device ## _status) & ~(device ## _state_adj[ new_status])) > 0


/*
  @brief checks if current status is in the allowed states, intended for use at
         start of a task
  @args
    -device needs to be the prefix...
    -map is a bitmask that contains 1s in the slots that are allowed. This can
    be generated by hand or offline
  @returns a 1 if device is in the wrong state
*/
#define STATE_OK(device,map) \
  ((1 << device ## _status) & ~(map)) > 0


/*
  @brief macro to register a function as a driver function that will affect
  peripheral state
*/
#define DRIVER __attribute__((annotate("periph")))

/*
  @brief macro to register a function as a candidate for peripheral toggling
  because it's long, annotated with a knob variable as its first argument and it
  doesn't use peripherals
*/
#define NO_PERIPH __attribute__((annotate("candidate")))

/*
  @brief macro to register a device as a peripheral
*/
//#define INIT_0
#ifdef INIT_0
#define REGISTER(device) \
  typedef int device ## _states; \
  volatile __nv device ## _states __attribute__((annotate("periph_var"))) \
  device ## _status = 0;

#else
#define REGISTER(device) \
  typedef int device ## _states; \
  volatile __nv device ## _states __attribute__((annotate("periph_var"))) \
  device ## _status = 1;

#endif

#define CALL_DRIVER_1(periph, func, arg1, ...) \
  func(arg1, __VA_ARGS__); \
  STATE_CHANGE(periph,arg1);

/*
 @brief macro to register a function that directly changes an energy typestate
 @argment is of the form "direct:<periph name>: <arg no (0 indexing)>"
 */
#define DIRECT(combo) \
  __attribute__((annotate(combo)))

#ifndef CHKPTLEN
#define CHKPTLEN 256
#else
#pragma warning("setting checkpoint to different value")
#endif

// pass in periph name as string for first argument
#define POSSIBLE_STATES(periph, ...) \
  possible_states_inner(periph, __VA_ARGS__);


// Just adds the DRIVER dec, and if statement and variable for an isr
#define USING_PACARANA_INTS \
  int run_isr = 0;

// Pacarana interrupts are just drivers
#define PACARANA_INT DRIVER

// Adds if statement around isr contents since it's conditional on the isr
// actually firing
#define PACARANA_INT_START \
  if (run_isr) {

// End if-statement
#define PACARANA_INT_END \
  }

// Function that disables interrupts for a particular ISR
#define DISABLE(isr) \
   isr ## _disable_()

// Function that enables interrupts again for an ISR
#define ENABLE(isr) \
   isr ## _enable_()

// Declare a function that we'll use to grab the function that sleeps a peripheral
// We'll just quietly hack this for use with librustic...
// TODO make this less gross or at least easy to turn off
#ifdef LIBPACARANA_JIT
#define SLEEP(periph,func) \
	void _sleep__ ## periph (void){ \
    UNSAFE_ATOMIC_START;  /*PRINTF("sleep %x\r\n",& periph##_status);*/func(); \
    UNSAFE_ATOMIC_END; }

// Declare a function that we'll use to grab the function that restores a peripheral
#define RESTORE(periph,func) \
	void _restore__ ## periph (void) { \
    UNSAFE_ATOMIC_START; /*PRINTF("restore %x\r\n",& periph##_status);*/ func(); \
    UNSAFE_ATOMIC_END; }

#else
#define SLEEP(periph,func) \
	void _sleep__ ## periph (void){ \
    func(); }

// Declare a function that we'll use to grab the function that restores a peripheral
#define RESTORE(periph,func) \
	void _restore__ ## periph (void) { \
    func();}

#endif
void possible_states_inner(const char *,...); 

//TODO make these accessible from Makefile.target
#define NUM_PERIPHS 2
// Set this to 7 when we use the merged func
//#define NUM_FUNCS 7
// Set this to 10 when we use the separate funcs
#define NUM_FUNCS 10

extern int *__paca_all_tables[NUM_PERIPHS];
void __paca_test_log_func(int x);
int __paca_read_table(int func, int *state, int periph);
extern int __paca_flag;

// This is the maximum number of functions that need to be run to get everything
// initialized. It's hardcoded to 4, but that is definitely flexible, albeit
// difficult to automatically deduce from the program
#define PACA_SIZE 6

typedef struct {
  uint8_t fxl_byte;
  int num_funcs;
  void (*pointertable[PACA_SIZE])(void);
} pacarana_cfg_t;

extern pacarana_cfg_t inits[];

#define PACA_CFG_ROW(fxlByte,numFuncs,...) \
  {fxlByte,numFuncs,{__VA_ARGS__}}

#warning "defining paca_init"
void paca_init(unsigned cfg);

// Struct for holding timing information between peripheral uses
typedef struct {
  unsigned int ovfl :8; //timer overflows on start
  unsigned int ts : 16; //timer count on start
  unsigned int pf : 8; // power failure tracker
  //TODO see if we can grab the periph_status from the dynamic variable
  unsigned int periph_status: 16;
  unsigned int periph: 16;
#ifdef LIBPACARANA_FULLY_DYNAMIC
  unsigned int pc: 16; //Add pc, make sure this is initialized to 0
#endif
#ifndef LIBPACARANA_MULTI_CNT //Multiple counters for each peripheral state
  unsigned int cntr : 2;
#else
  unsigned int cntr : 16; // Allows for 8 possible states
#endif
  unsigned int active : 1;
} pacarana_periph_usage_t;

typedef unsigned int pacarana_active_vect_t;

#ifdef LIBPACARANA_FULLY_DYNAMIC
volatile extern __nv unsigned int __paca_index_extended;
#endif

//TODO set up flags in makefile to define number of periphs and number of
//possible states
#define NUM_STORED_PERIPHS  5
extern uint16_t __paca_break_evens[NUM_STORED_PERIPHS][4];
extern  __nv pacarana_periph_usage_t __paca_periph_uses[];
volatile extern __nv uint16_t __paca_global_ovfl;
volatile extern __nv uint16_t __paca_last_ovfl;
volatile extern __nv uint8_t __paca_toggles[NUM_STORED_PERIPHS];

#define LIBPACARANA_USES_VECTOR_SIZE 16

//#define LIBPACARANA_PRINTF

#if 0
#define LIBPACARANA_BIT_FLIP(port,bit) \
do {\
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit;\
 } while(0);
#else
#define BIT_FLIP(port,bit)
#define LIBPACARANA_BIT_FLIP(...)
#endif

//TODO there's gotta be a way to keep these actually hidden
int __paca_timer_start(unsigned int pc, int periph, int periph_status);
void __paca_timer_stop(int periph);
int __paca_dummy_func(int a, int b,...);

//TODO create a new macro that finds the associated periph number
#define LIBPACARANA_START_TIMER(pc, per, ps) \
  __paca_toggles[per] = __paca_timer_start(pc,per,ps);

#define LIBPACARANA_STOP_TIMER(per) __paca_timer_stop(per);

// Note: in both definitions for grab_start_time, we're cheating and pushing JIT
// off for a bit instead of doing a full checkpointed atomic block.
// we assume we can get away with a couple extra isntructions, particularly when
// the bit_flips are deactivated
#ifndef LIBPACARANA_FULLY_DYNAMIC

#warning "NOT DYNAMIC"
#define LIBPACARANA_GRAB_START_TIME(pc) \
do { __disable_interrupt(); \
     __paca_periph_uses[pc].ts = TA0R; \
     LIBPACARANA_BIT_FLIP(1,1); \
     __paca_periph_uses[pc].ovfl = __paca_global_ovfl; \
    __enable_interrupt();\
} while(0);

#else
#warning "Fully dynamic!"

//TODO this doesn't function if __paca_index_extended isn't printed out here, so
//we have the print statement... sigh. don't take it out until you debug exactly
//what's happening
//TODO check if this is true for the mega-kernels...
#define LIBPACARANA_GRAB_START_TIME(pc) \
do {__disable_interrupt(); \
    __paca_periph_uses[__paca_index_extended].ts = TA0R; \
    __paca_periph_uses[__paca_index_extended].ovfl = __paca_global_ovfl; \
    __enable_interrupt();\
    printf("%u",__paca_index_extended);\
} while(0);
#endif


#define LIBPACARANA_TOGGLES(x) __paca_toggles[x]

// TODO reduce the number of args to this, we can derive most of this info from
// the peripheral 
#define LIBPACARANA_TOGGLE_PRE(periph_num, re_call) \
  do {  LIBPACARANA_BIT_FLIP(1,0);\
    LIBPACARANA_STOP_TIMER(periph_num); \
    LIBPACARANA_BIT_FLIP(1,1);\
    UNSAFE_ATOMIC_START; \
    if(LIBPACARANA_TOGGLES(periph_num)) {re_call();} \
  } while(0);

#define LIBPACARANA_TOGGLE_POST(periph, periph_num, dis_call) \
  do { UNSAFE_ATOMIC_END; \
    LIBPACARANA_BIT_FLIP(1,0);\
    unsigned int __inner_pc;\
    __asm__ volatile ("MOVX.A R0, %0 ": "=r" (__inner_pc));\
    LIBPACARANA_START_TIMER(__inner_pc, periph_num, STATE_READ(periph));\
    LIBPACARANA_BIT_FLIP(1,1);\
    if (LIBPACARANA_TOGGLES(periph_num)) {\
      P1OUT |= BIT2; P1DIR |= BIT2; P1OUT &= ~BIT2;\
      UNSAFE_ATOMIC_START;dis_call();\
        UNSAFE_ATOMIC_END;}\
    LIBPACARANA_BIT_FLIP(1,0);\
    LIBPACARANA_GRAB_START_TIME(__inner_pc);\
    LIBPACARANA_BIT_FLIP(1,1);\
  } while(0);


void paca_update_pfs(void);

#ifdef LIBPACARANA_SEPARATE_PF
extern __nv int *__paca_status_ptrs[NUM_STORED_PERIPHS];

#define PACA_SET_STATUS_PTRS \
  __nv int *__paca_status_ptrs[NUM_STORED_PERIPHS]


#define RUN_PACA_SEPARATE_PF paca_update_pfs();

#else//not separate

#define PACA_SET_STATUS_PTRS

#define RUN_PACA_SEPARATE_PF

#endif




#endif