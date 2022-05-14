// Unit tests for new pacarana timer based toggler mechanism

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
//#ifdef PACARANA
#include <libpacarana/pacarana.h>
//#endif
// Library for JIT checkpointing
#include <librustic/checkpoint.h>

// Libraries for the IMU and temp sensor
#include <liblsm/gyro.h>
#include <liblsm/accl.h>
#include "temp_sensor.h"
#include <libfxl/fxl6408.h>

#include "accel_table.h"
#include "gyro_table.h"


#define TEST_PUDU
//#define ALWAYS_TOGGLE

#define TEST_INNER_ACCEL
//#define TEST_LOTSA_USES
//#define TEST_GYRO_INNER

int* __paca_all_tables[NUM_PERIPHS] = {
	accel_table,
  gyro_table,
};

REGISTER(gyro);
REGISTER(accel);



PACA_SET_STATUS_PTRS  = {&accel_status, &gyro_status, NULL, NULL};

void g_init(){
  //printf("g_init!\r\n");
  gyro_only_init_odr_hm(0x80, 1);
  STATE_CHANGE(gyro,1);
}
void a_init(){
  //printf("a_init!\r\n");
  int temp = 1;
  while(temp) {
    P1OUT |= BIT5;
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;
    temp = accel_only_init_odr_hm(0x80, 1);
    if(temp) { 
      fxl_clear(BIT_SENSE_SW); 
      __delay_cycles(4800);
      fxl_set(BIT_SENSE_SW);
    }
  }
  //accel_only_init_odr_hm(0x80, 1);
  STATE_CHANGE(accel,1);
}

void delay_1() {
  __delay_cycles(48000);
}
__nv pacarana_cfg_t inits[4] = {
  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1,g_init,a_init),
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1,g_init),
  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1,a_init),
  PACA_CFG_ROW(BIT_SENSE_SW, 1, delay_1),
};


SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(accel),&STATE_READ(gyro)};


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

#if 0
#define BIT_FLIP(port,bit) \
do {\
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit;\
 }while(0);
#else
#define BIT_FLIP(port,bit)
#endif

void g_disable() {
  //printf("disG %u\r\n",STATE_READ(gyro));
  if (STATE_READ(gyro) != -1) {
    lsm_gyro_sleep();
	  STATE_CHANGE(gyro,0);
  }
  //PRINTF("G2: %i\r\n",STATE_READ(gyro));
}

void g_reenable() {
  //printf("ReenG %u %u\r\n",STATE_READ(gyro), STATE_READ(gyro) == -1);
  if (STATE_READ(gyro) < 0) {
	  gyro_only_init_odr_hm(0x80, 1);
  }
  else {
    lsm_gyro_reenable();
    __delay_cycles(19200);
  }
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
	STATE_CHANGE(gyro,1);
}

void a_disable() {
  //printf("disA %u\r\n",STATE_READ(accel));
  if (STATE_READ(accel) != -1) {
    lsm_accel_disable();
    STATE_CHANGE(accel,0);
  }
  BIT_FLIP(1,0);
  BIT_FLIP(1,1);
  BIT_FLIP(1,0);
}

void a_reenable() {
  //printf("ReenA! %u\r\n",STATE_READ(accel));
  if (STATE_READ(accel) < 0) {
    a_init();
  }
  else {
    accel_odr_reenable(0x80);
  }
  STATE_CHANGE(accel,1);
  BIT_FLIP(1,1);
  BIT_FLIP(1,0);
}

__nv int gnded_flag = 0;


#ifdef TEST_LOTSA_USES
void test_function() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif
#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function gyro:");
  print_float(g_val);
}
void test_function1() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif
#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function1 gyro:");
  print_float(g_val);
}
void test_function2() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif

#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function2 gyro:");
  print_float(g_val);
}

void test_function3() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif
#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function gyro:");
  print_float(g_val);
}
void test_function4() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif
#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function4 gyro:");
  print_float(g_val);
}
void test_function5() {
#ifdef TEST_PUDU
  LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
  UNSAFE_ATOMIC_START; g_reenable();
#else
  UNSAFE_ATOMIC_START;
#endif
#endif
  float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
  g_disable(); UNSAFE_ATOMIC_END;
#else
  UNSAFE_ATOMIC_END;
#endif
#else
  LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  printf("test function5 gyro:");
  print_float(g_val);
}
#endif

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
  /*__delay_cycles(48000);
  __delay_cycles(48000);
  fxl_set(BIT_SENSE_SW);
  __delay_cycles(48000);
	gyro_init_data_rate_hm(0x80, 1);
  STATE_CHANGE(gyro,1);
  accel_only_init_odr_hm(0x80, 1);
  STATE_CHANGE(accel,1);
  */
  pre_atomic_reboot(SEMI_SAFE_ATOMIC_BUFFER);//TODO make this init real
  RUN_PACA_SEPARATE_PF;
  printf("Capy init! %x %x\r\n",STATE_READ(accel),STATE_READ(gyro));
  // Start with both turned on
  //set cfg based on peripheral states
  unsigned cfg = 0;
  if (STATE_READ(gyro) > 0) {
    if (STATE_READ(accel) > 0 ) {
      cfg = 0;
    }
    else {
      STATE_CHANGE(accel,-1);
      cfg = 1;
    }
  }
  else {
    if (STATE_READ(accel) > 0) {
      STATE_CHANGE(gyro,-1);
      cfg = 2;
    }
    else {
      STATE_CHANGE(gyro,-1);
      STATE_CHANGE(accel,-1);
      cfg = 3;
    }
  }
  paca_init(cfg);
  printf("Ready!%x %x\r\n",STATE_READ(accel),STATE_READ(gyro));
  entry();
#ifdef TEST_GYRO_INNER
  for(int i = 0; i < 100; i++) {
    BIT_FLIP(1,0);
    printf("accel\r\n");
    // Use accel
#ifdef TEST_PUDU
    LIBPACARANA_TOGGLE_PRE(0,a_reenable);
#else
#ifdef ALWAYS_TOGGLE
    UNSAFE_ATOMIC_START; a_reenable();
#else
    UNSAFE_ATOMIC_START;
#endif
#endif
    float accel_val = accelerometer_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
    a_disable(); UNSAFE_ATOMIC_END;
#else
    UNSAFE_ATOMIC_END;
#endif
#else
    LIBPACARANA_TOGGLE_POST(accel, 0, a_disable);
#endif
    // wait
    print_float(accel_val);
    __delay_cycles(100000);
    // Use gyro
    for (int j = 0; j < 10; j++) {
    printf("gyro:\r\n");
#ifdef TEST_PUDU
      LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
      UNSAFE_ATOMIC_START; g_reenable();
#else
      UNSAFE_ATOMIC_START;
#endif
#endif
      float x_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
      g_disable();UNSAFE_ATOMIC_END;
#else
      UNSAFE_ATOMIC_END;
#endif
#else
      LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
      // wait
      print_float(x_val);
      __delay_cycles(10000);
    }
  }
  printf("Done\r\n");
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  while(1);
#endif
/*------------------------------------------*/
#ifdef TEST_INNER_ACCEL
  for(int i = 0; i < 100; i++) {
#ifndef TEST_PUDU
    LIBPACARANA_BIT_FLIP(1,0);
#endif
    printf("gyro\r\n");
    // Use accel
#ifdef TEST_PUDU
    LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
    UNSAFE_ATOMIC_START; g_reenable();
#else
    UNSAFE_ATOMIC_START;
#endif
#endif
    /*if (STATE_READ(gyro) < 1) {
      printf("Error! Gyro state is %u\r\n",STATE_READ(gyro));
      while(1);
    }
    printf("Read gyro:");*/
    float g_val = gyroscope_read_x();
    //printf("done g\r\n");
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
    g_disable(); UNSAFE_ATOMIC_END;
#else
    UNSAFE_ATOMIC_END;
#endif
#else
    LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
    // wait
    print_float(g_val);
    __delay_cycles(100000);
    // Use gyro
    for (int j = 0; j < 10; j++) {
    printf("accel:\r\n");
#ifdef TEST_PUDU
      LIBPACARANA_TOGGLE_PRE(0,a_reenable);
#else
#ifdef ALWAYS_TOGGLE
      UNSAFE_ATOMIC_START; a_reenable();
#else
      UNSAFE_ATOMIC_START;
#endif
#endif
      /*if (STATE_READ(accel) < 1) {
        printf("Error! accel state %x\r\n",STATE_READ(accel));
        while(1);
      }
      printf("read: %x %x ",STATE_READ(accel), STATE_READ(gyro));*/
      float a_val = accelerometer_read_x();
      //printf("done\r\n");
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
      a_disable();UNSAFE_ATOMIC_END;
#else
      UNSAFE_ATOMIC_END;
#endif
#else
      LIBPACARANA_TOGGLE_POST(accel, 0, a_disable);
#endif
      // wait
      print_float(a_val);
      __delay_cycles(10000);
    }
  }
  printf("Done\r\n");
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  while(1);
#endif
/*-------------------------------------------------------*/
#ifdef TEST_LOTSA_USES
#ifdef TEST_PUDU
    LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
    UNSAFE_ATOMIC_START; g_reenable();
#else
    UNSAFE_ATOMIC_START;
#endif
#endif
    float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
    g_disable(); UNSAFE_ATOMIC_END;
#else
    UNSAFE_ATOMIC_END;
#endif
#else
    LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
  test_function();
  __delay_cycles(40000);
  test_function1();
  for(int i = 0; i < 100; i++) {
    BIT_FLIP(1,0);
    printf("gyro lots\r\n");
    // Use accel
#ifdef TEST_PUDU
    LIBPACARANA_TOGGLE_PRE(1,g_reenable);
#else
#ifdef ALWAYS_TOGGLE
    UNSAFE_ATOMIC_START; g_reenable();
#else
    UNSAFE_ATOMIC_START;
#endif
#endif
    float g_val = gyroscope_read_x();
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
    g_disable(); UNSAFE_ATOMIC_END;
#else
    UNSAFE_ATOMIC_END;
#endif
#else
    LIBPACARANA_TOGGLE_POST(gyro, 1, g_disable);
#endif
    // wait
    print_float(g_val);
    __delay_cycles(100000);
    // Use gyro
    for (int j = 0; j < 10; j++) {
    printf("accel:\r\n");
#ifdef TEST_PUDU
      LIBPACARANA_TOGGLE_PRE(0,a_reenable);
#else
#ifdef ALWAYS_TOGGLE
      UNSAFE_ATOMIC_START; a_reenable();
#else
      UNSAFE_ATOMIC_START;
#endif
#endif
      /*if (STATE_READ(accel) < 1) {
        printf("Error! accel state %x\r\n",STATE_READ(accel));
        while(1);
      }
      printf("read: %x %x ",STATE_READ(accel), STATE_READ(gyro));*/
      float a_val = accelerometer_read_x();
      //printf("done\r\n");
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
      a_disable();UNSAFE_ATOMIC_END;
#else
      UNSAFE_ATOMIC_END;
#endif
#else
      LIBPACARANA_TOGGLE_POST(accel, 0, a_disable);
#endif
      // wait
      print_float(a_val);
      __delay_cycles(10000);
#ifdef TEST_PUDU
      LIBPACARANA_TOGGLE_PRE(0,a_reenable);
#else
#ifdef ALWAYS_TOGGLE
      UNSAFE_ATOMIC_START; a_reenable();
#else
      UNSAFE_ATOMIC_START;
#endif
#endif
      /*if (STATE_READ(accel) < 1) {
        printf("Error! accel state %x\r\n",STATE_READ(accel));
        while(1);
      }
      printf("read: %x %x ",STATE_READ(accel), STATE_READ(gyro));*/
      float a_val1 = accelerometer_read_x();
      //printf("done\r\n");
#ifndef TEST_PUDU
#ifdef ALWAYS_TOGGLE
      a_disable();UNSAFE_ATOMIC_END;
#else
      UNSAFE_ATOMIC_END;
#endif
#else
      LIBPACARANA_TOGGLE_POST(accel, 0, a_disable);
#endif
    }
    for(int j = 0; j < 5; j++) {
      test_function4();
      test_function5();
    }
    test_function2();
    __delay_cycles(40000);
    test_function3();
    __delay_cycles(40000);
  }
  printf("Done lotsa\r\n");
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  while(1);
#endif
}
