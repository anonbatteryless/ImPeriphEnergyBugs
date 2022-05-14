// Example bugs to showcase Pudu-- these are all tested on Capybara v2.0 with
// the following bank config:
// 0: 2 x 330uf, 1: 3x7mF, 2:0, 3: 1x7mF, 4: 1x7mF
// These were compiled with gcc -O2
// This vesion has all 5 bugs (I'm sorry)

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

//#define TEST_KARMA
// Test apds
// To test apds, run with bank 4 attached
//#define TEST_APDS_BUG
//#define TEST_APDS_FIX
//#define TEST_ASYNCH_APDS

// Test lights left on
// To test lights left on, rig up led between P1.5 and ground (no, we don't add
// a resistor because the msp430 is already pretty current limited and the
// output is only at 2.5V). Then connect Bank  1 and go for it.  Use a dla to
// watch P1.0 and P1.1. You'll notice that the start/stop signals around the
// radio fail to complete atomically once the system gets locked into the fail
// stop case.  Fail stop occurs when the timer is disabled, but the led was
// still on when we started to try to send the radio packet
#define TEST_LIGHTS_BUG
//#define TEST_LIGHTS_FIX


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
//#define TEST_MULTI_BUG
//#define TEST_MULTI_FIX
//#define TEST_ASYNCH_MULTI


// To test the Pudu pass, enable the following flag and compile this app using
// clang (e.g. make bld/clang/clean; make bld/clang/all RUN_BUGS=1 USE_SENSORS=)
// Don't forget to compile the gcc deps with the mlarge flag commented out in
// makefile.gcc, and don't forget to change the definition of __nv in
// libmsp/src/include/mem.h to .persistent from .upper.rodata. Clang can't do
// MSP430X instructions that the upper.rodate section needs.
#define RUN_PUDU_PASS

volatile int16_t gx_glob,gy_glob,gz_glob;
volatile int16_t ax_glob,ay_glob,az_glob;
int volatile new_dist = 0, new_imu = 0, new_gest = 0;
uint8_t volatile __internal_dist;
gest_dir __internal_gest;

int* __paca_all_tables[NUM_PERIPHS] = {
	accel_table,
  gyro_table,
};


REGISTER(gyro);
REGISTER(accel);
REGISTER(apds);
REGISTER(led);
REGISTER(photo);
REGISTER(asynch);
REGISTER(imu);
REGISTER(prox);

#ifdef RUN_PUDU_PASS
USING_PACARANA_INTS
PACA_SET_STATUS_PTRS = {&accel_status,&gyro_status,NULL,NULL,NULL};
#else
#undef PACARANA_INT_START
#undef PACARANA_INT_END
#define PACARANA_INT_START
#define PACARANA_INT_END

#endif

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

int16_t check_progression(int16_t *g_samps, int16_t *a_samps) {
  if (*(g_samps) > *(g_samps + 3) && *(g_samps + 3) > *(g_samps + 6)) {
    return 0;
  }
  if (*(g_samps + 1) > *(g_samps + 4) && *(g_samps + 4) > *(g_samps + 7)) {
    return 0;
  }
  if (*(g_samps + 2) > *(g_samps + 5) && *(g_samps + 5) > *(g_samps + 8)) {
    return 0;
  }
  if (*(a_samps) > *(a_samps + 3) && *(a_samps + 3) > *(a_samps + 6)) {
    return 0;
  }
  if (*(a_samps + 1) > *(a_samps + 4) && *(a_samps + 4) > *(a_samps + 7)) {
    return 0;
  }
  if (*(a_samps + 2) > *(a_samps + 5) && *(a_samps + 5) > *(a_samps + 8)) {
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

__nv volatile imu_rate = 0x80;

void DRIVER imu_init() {
  if (imu_rate == 0x80) {
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
  else  {
    //Rate = 0x40
    gyro_init_data_rate_hm(0x40,0); //Turn on gyro & accel in IMU
    STATE_CHANGE(imu,2);
  }
}

void DRIVER imu_reenable() {
  if (imu_rate == 0x80) {
    accel_odr_reenable(0x80);
    lsm_gyro_reenable();
    __delay_cycles(19200);
    STATE_CHANGE(imu,1);
  }
  else {
    accel_odr_reenable(0x40);
    lsm_gyro_reenable();
    __delay_cycles(153800);
    STATE_CHANGE(imu,2);
  }
}

void DRIVER imu_disable() {
  lsm_accel_disable();
  lsm_gyro_sleep();
  STATE_CHANGE(imu,0);
}

void DRIVER g_init(){
  //printf("g_init!\r\n");
  gyro_only_init_odr_hm(0x80, 1);
  STATE_CHANGE(gyro,1);
}
void DRIVER a_init(){
  //printf("a_init!\r\n");
  int temp = 1;
  while(temp) {
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

void apds_delay() {
  WAIT_APDS_DELAY;
}


#ifdef TEST_APDS_BUG
SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(apds),&STATE_READ(gyro),&STATE_READ(asynch)};
#elif defined(TEST_LIGHTS_BUG)
SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(led),&STATE_READ(photo),&STATE_READ(asynch)};
#else
SEMI_SAFE_ATOMIC_BUFFER_SET = {&STATE_READ(imu),&STATE_READ(prox),&STATE_READ(asynch)};
#endif

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



void DRIVER init_led() {
  P1OUT &= ~BIT5;
  P1DIR |= BIT5;
  STATE_CHANGE(led,0);
  return;
}

void DRIVER led_on() {
  P1OUT |= BIT5;
  STATE_CHANGE(led,1);
}

void DRIVER led_off() {
  P1OUT &= ~BIT5;
  STATE_CHANGE(led,0);
}

void DRIVER init_timer() {
  //Init timer to toggle
  printf("Timer going on\r\n");
  TA1CCR0 = 50;
  TA1CTL = TASSEL__ACLK | MC__UP | ID_3 | TAIE_1;
  TA1CCTL0 |= CCIE;
  TA1R = 0;
  STATE_CHANGE(asynch,1);
}

void DRIVER init_multi_timer() {
  TA1CCR0 = 100;
  TA1CTL = TASSEL__ACLK | MC__UP | ID_3 | TAIE_1;
  TA1CCTL0 |= CCIE;
  TA1R = 0;
  STATE_CHANGE(asynch,1);
}

/* Set of dummy functions for Pudu pass*/
void ENABLE(Timer1_A0_ISR) {
#ifdef TEST_ASYNCH_APDS
  init_timer();
#endif
#ifdef TEST_LIGHTS_BUG
  init_timer();
#endif
#ifdef TEST_ASYNCH_MULTI
  init_timer();
#endif
}

void DISABLE(Timer1_A0_ISR) {
  TA1CTL = MC__STOP;
}

gest_dir apds_read_asynch() {
  printf("Asynch\r\n");
  while(!new_gest) {
    BIT_FLIP(1,0);
    __delay_cycles(80000);
    printf("Gest %u\r\n",new_gest); 
    BIT_FLIP(1,0);
  } 
  printf("Start gest\r\n");
  __internal_gest = apds_get_gesture();
  printf("Stop_gest\r\n");
  new_gest = 0;
  return __internal_gest;
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
  /*
  *gx = gx_glob;
  *gy = gy_glob;
  *gz = gz_glob;
  *ax = ax_glob;
  *ay = ay_glob;
  *az = az_glob;
  */
  new_imu = 0;
  return;
}

void DRIVER disable_timer() {
  TA1CTL = MC__STOP;
  STATE_CHANGE(asynch,0);
}

void DRIVER g_disable() {
  //printf("disG %u\r\n",STATE_READ(gyro));
  if (STATE_READ(gyro) != -1) {
    lsm_gyro_sleep();
	  STATE_CHANGE(gyro,0);
  }
  //PRINTF("G2: %i\r\n",STATE_READ(gyro));
}

void DRIVER g_reenable() {
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

void DRIVER a_disable() {
  //printf("disA %u\r\n",STATE_READ(accel));
  if (STATE_READ(accel) != -1) {
    lsm_accel_disable();
    STATE_CHANGE(accel,0);
  }
  BIT_FLIP(1,0);
  BIT_FLIP(1,1);
  BIT_FLIP(1,0);
}

void DRIVER a_reenable() {
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

void DRIVER proximity_sense_init() {
  proximity_init();
  enableProximitySensor(); 
  STATE_CHANGE(prox,1);
}

void DRIVER proximity_sense_disable() {
  apds_proximity_disable();
  STATE_CHANGE(prox,1);
}

void DRIVER proximity_sense_reenable() {
  apds_proximity_reenable();
  STATE_CHANGE(prox,0);
}

__nv int gnded_flag = 0;

#define TIMEOUT 10

__nv pacarana_cfg_t inits[19] = {
/*0*/  PACA_CFG_ROW(BIT_SENSE_SW, 3, delay_1,g_init,a_init),
/*1*/  PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1,g_init),
/*2*/ PACA_CFG_ROW(BIT_SENSE_SW, 2, delay_1,a_init),
/*3*/ PACA_CFG_ROW(BIT_SENSE_SW, 1, delay_1),
/*4*/ PACA_CFG_ROW(0,0,NULL),
/*5*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,apds_init,apds_delay),
/*6*/ PACA_CFG_ROW(0,4,enable_photoresistor,init_led,led_on,init_timer),
/*7*/ PACA_CFG_ROW(0,3,enable_photoresistor,init_led,init_timer),
/*8*/ PACA_CFG_ROW(0,3,enable_photoresistor,init_led,led_on),
/*9*/ PACA_CFG_ROW(0,2,enable_photoresistor,init_led),
/*10*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,delay_1,proximity_sense_init),
/*11*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,1,delay_1),
/*12*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,proximity_sense_init,imu_init),
/*13*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,2,delay_1,imu_init),
/*14*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,proximity_sense_init,init_multi_timer),
/*15*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,4,delay_1,proximity_sense_init,imu_init,init_multi_timer),
/*16*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,delay_1,imu_init,init_multi_timer),
/*17*/ PACA_CFG_ROW(BIT_SENSE_SW | BIT_APDS_SW,3,apds_init,apds_delay,init_timer),
/*18*/ PACA_CFG_ROW(0,1,init_timer),
};


#ifdef RUN_PUDU_PASS
int task_main(void) {
#else
int DRIVER main(void) {
#endif
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
  printf("Capy init! %i %i\r\n",STATE_READ(prox),STATE_READ(imu));
  unsigned cfg = 0;
#ifdef TEST_APDS_BUG
#ifndef TEST_ASYNCH_APDS
  #ifndef TEST_KARMA
  if (STATE_READ(apds) > 0) {
    cfg = 5;
  }
  else {
    cfg = 4;
  }
  #else
  // If testing karma, re-init on use
  STATE_CHANGE(apds,0);
  cfg = 4;
  #endif
#else//ASYNCH
  #ifndef TEST_KARMA
  if (STATE_READ(asynch) > 0) {
    if (STATE_READ(apds) > 0) {
      cfg = 17;
    }
    else {
      cfg = 18;
    }
  }
  else {
    if (STATE_READ(apds) > 0) {
      cfg = 5;
    }
    else {
      cfg = 4;
    }
  }
  #else
  // If testing karma, re-init on use
  if (STATE_READ(asynch) > 0) {
    cfg = 17;//asynch on, so bring apds back up
    STATE_CHANGE(apds,1);
  }
  else {
    cfg = 4; //asynch off, so everything else is too
    STATE_CHANGE(apds,0);
  }
  #endif
#endif//ASYNCH_APDS
#elif defined(TEST_LIGHTS_BUG)
#ifndef TEST_KARMA
  if (STATE_READ(led) > 0) {
    if (STATE_READ(asynch) > 0) {
      cfg = 6;
    }
    else {
      cfg = 8;
    }
  }
  else {
    if (STATE_READ(asynch) > 0) {
      cfg = 7;
    }
    else {
      cfg = 9;
    }
  }
  #else
  if (STATE_READ(asynch) > 0) {
    if (STATE_READ(led) > 0) {
      cfg = 6;
    }
    else {
      cfg = 7;
    }
  }
  else {
    cfg = 9;
  }
  #endif
#else //MULTI
  #ifndef TEST_KARMA
    if (STATE_READ(prox) > 0) {
      if (STATE_READ(imu) > 0) {
        //Prox & imu are active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 12;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 15;
        }
        else {
          cfg = 12;
        }
    #endif
      }
      else {
        //Prox is active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 10;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 14;
        }
        else {
          cfg = 10;
        }
    #endif
      }
    }
    else {
      if (STATE_READ(imu) > 0){
        //Imu is active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 13;
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 16;
        }
        else {
          cfg = 13;
        }
    #endif
      }
      else {
        //Nothing is active
        cfg = 11;
      }
    }
  #else //TEST_KARMA
    if (STATE_READ(prox) > 0) {
      if (STATE_READ(imu) > 0) {
        //Prox & imu are active
    #ifndef TEST_ASYNCH_MULTI
        // With karma, without asynch, leave disabled
        cfg = 11;
        STATE_CHANGE(prox,0);
        STATE_CHANGE(imu,0);
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 15;
        }
        else {
          cfg = 11;
          STATE_CHANGE(prox,0);
          STATE_CHANGE(imu,0);
        }
    #endif
      }
      else {
        //Prox is active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 11;
        STATE_CHANGE(prox,0);
        STATE_CHANGE(imu,0);
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 14;
        }
        else {
          cfg = 11;
          STATE_CHANGE(prox,0);
          STATE_CHANGE(imu,0);
        }
    #endif
      }
    }
    else {
      if (STATE_READ(imu) > 0){
        //Imu is active
    #ifndef TEST_ASYNCH_MULTI
        cfg = 11;
        STATE_CHANGE(prox,0);
        STATE_CHANGE(imu,0);
    #else
        if (STATE_READ(asynch) > 0) {
          cfg = 16;
        }
        else {
          cfg = 11;
          STATE_CHANGE(prox,0);
          STATE_CHANGE(imu,0);
        }
    #endif
      }
      else {
        //Nothing is active
        cfg = 11;
        STATE_CHANGE(prox,0);
        STATE_CHANGE(imu,0);
      }
    }
  #endif//TEST_KARMA
#endif
  paca_init(cfg);
  printf("Ready! %u\r\n",cfg);
  entry();
#ifdef TEST_APDS_BUG
  while(1) { // Typical loop structure in embedded systems
    UNSAFE_ATOMIC_START;
    apds_init();
    STATE_CHANGE(apds,1);
    WAIT_APDS_DELAY;
#ifdef TEST_ASYNCH_APDS
    ENABLE(Timer1_A0_ISR);
    printf("Timer on!\r\n");
#endif
    UNSAFE_ATOMIC_END;
    for(int i = 0; i < TIMEOUT; i++) {
      UNSAFE_ATOMIC_START;
#ifdef TEST_KARMA
      printf("Count: %i\r\n",i);
      if (STATE_READ(apds) < 1) {
        apds_init();
        WAIT_APDS_DELAY;
        STATE_CHANGE(apds,1);;
      }
#endif
#ifdef TEST_ASYNCH_APDS
      printf("Reading asynch!\r\n");
      gest_dir gesture = apds_read_asynch();
#else
      gest_dir gesture = apds_get_gesture();
#endif
      UNSAFE_ATOMIC_END;
      if (gesture > DIR_NONE) {
        UNSAFE_ATOMIC_START;
        printf("Gest is %u\r\n",gesture);
#ifdef TEST_KARMA
      if (STATE_READ(apds) < 1) {
        apds_init();
        WAIT_APDS_DELAY;
        STATE_CHANGE(apds,1);;
      }
#endif
        apds_disable();
        STATE_CHANGE(apds,0);
        UNSAFE_ATOMIC_END;
        break;
      }
      printf("i is: %x\r\n",i);
    }
#ifdef TEST_APDS_FIX
    UNSAFE_ATOMIC_START;
    apds_disable();
    STATE_CHANGE(apds,0);
    UNSAFE_ATOMIC_END;
#endif
    UNSAFE_ATOMIC_START;
    BIT_FLIP(1,0);
    printf("Start! %u\r\n",STATE_READ(apds));
    for (int j = 0; j < 200; j++) {
      __delay_cycles(80000); // Stand in for computing
    }
    printf("Stop!\r\n");
    BIT_FLIP(1,1);
    UNSAFE_ATOMIC_END;
  }
#endif
#ifdef TEST_LIGHTS_BUG
  while(1) {
    // Init led
    init_led();
    // Init led timer for toggling
    #if 0
    init_timer(); //ASYNCH START
    #else
    ENABLE(Timer1_A0_ISR);
    #endif
    // enable photoresistor
    enable_photoresistor();
    STATE_CHANGE(photo,2);
    uint16_t distance = 0;
    do {
      BIT_FLIP(1,1);
      __delay_cycles(80000);
      BIT_FLIP(1,1);
      uint16_t light = read_photoresistor();
      if (light < LOW_OUTPUT) {
        distance = 0;
      }
      else {
        distance = 3500 - light;
      }
      UNSAFE_ATOMIC_START;
      printf("Dist is %u %u\r\n",distance,light);
      UNSAFE_ATOMIC_END;
    } while(distance > 0 && distance < 1500);

    //ASYNCH STOP
    //disable_timer();
    DISABLE(Timer1_A0_ISR);
    #ifdef TEST_LIGHTS_FIX
    led_off();
    STATE_CHANGE(led,0);
    #endif
    // Put distance in pkt with a bunch of other info
    for (unsigned i = 2; i < LIBRADIO_BUFF_LEN; ++i) {
        radio_buff[i] = 'A';
    }
    radio_buff[0] = distance & 0xff;
    radio_buff[1] = (distance & 0xff00) >> 8;
    UNSAFE_ATOMIC_START;
    printf("Start %u\r\n",STATE_READ(led));
    BIT_FLIP(1,0);
    BIT_FLIP(1,0);
    radio_send_one_off();
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    printf("Stop\r\n");
    UNSAFE_ATOMIC_END;
  }
#endif
#ifdef TEST_MULTI_BUG
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
      UNSAFE_ATOMIC_START;
      printf("Checking dist\r\n");
      if (STATE_READ(prox) < 1) {
        proximity_sense_init();
        STATE_CHANGE(prox,1);
      }
      #ifndef TEST_ASYNCH_MULTI
      dist = proximity_read_byte();
      #else
      dist = read_prox_asynch();
      #endif
      printf("Dist is: %u\r\n",dist);
      UNSAFE_ATOMIC_END;
      if (dist > 70) {
        // If dist is way too close, break
        heading = 0;
      }
      else if (dist > 30) {
        //If dist is a bit too close, grab dist and angle together for
        //better fidelity reading
        int16_t gx,gy,gz,ax,ay,az;
        UNSAFE_ATOMIC_START;
        printf("Start\r\n");
        if (STATE_READ(prox) < 1) {
          proximity_sense_init();
          STATE_CHANGE(prox,1);
        }
        if (STATE_READ(imu) < 1) {
          imu_rate = 0x80;
          imu_init();// Note: you definitely need to apply Pudu to figure out if
          //__delay_cycles(80000);
          STATE_CHANGE(imu,1);
        }           // it's a good idea to toggle here.
        #ifndef TEST_ASYNCH_MULTI
        read_raw_accel(&ax,&ay,&az);
        for (int i = 0; i < 6; i++) {
          read_raw_gyro(&gx,&gy,&gz);
          printf("%i %i %i\r\n",gx,gy,gz);
          __delay_cycles(8000);
        }
        dist = proximity_read_byte();
        #else
        //printf("Running asynch!\r\n");
        read_imu_asynch(&gx,&gy,&gz,&ax,&ay,&az);
        dist = read_prox_asynch();
        #endif
        printf("%i %i %i\r\n",gx,gy,gz);
        printf("%i %i %i ; %i %i %i\r\n",gx,gy,gz,ax,ay,az);
        heading = check_heading((int)gx,(int)gy,(int)gz,(int)ax,(int)ay,(int)az,dist);
        // Original attempted fix
        if (!heading) {
          sensor_ripcord();
        }
        printf("Stop\r\n");
        UNSAFE_ATOMIC_END;
      }
      else if (dist > 10) {
        //If we're right on the cusp of something too close, grab a couple slow
        //imu samples to see if there's a trend
        UNSAFE_ATOMIC_START;
        imu_rate = 0x40;
        imu_init();
        STATE_CHANGE(imu,2);
        int16_t g_samples[9];
        int16_t a_samples[9];
        for(int i = 0; i < 9; i+=3) {
          read_raw_gyro(g_samples+i,g_samples+i+1,g_samples+i+2);
          read_raw_accel(a_samples+i,a_samples+i+1,a_samples+i+2);
        }
        heading = check_progression(g_samples,a_samples);
        if (!heading) {
          sensor_ripcord();
        }
        UNSAFE_ATOMIC_END;
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
#endif
}

#ifdef RUN_PUDU_PASS
int main(void) {
  task_main();
}
#endif

#if defined(TEST_LIGHTS_BUG) || defined(TEST_ASYNCH_MULTI) || defined(TEST_ASYNCH_APDS)
#ifndef RUN_PUDU_PASS
void __attribute ((interrupt(TIMER1_A0_VECTOR))) PACARANA_INT Timer1_A0_ISR(void) {
#else
void PACARANA_INT Timer1_A0_ISR(void) {
#endif
  PACARANA_INT_START
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
#ifdef TEST_ASYNCH_APDS
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  TA1CCTL0 &= ~CCIE;
  new_gest = 1;

  TA1R = 0;
  TA1CCTL0 |= CCIE;
  BIT_FLIP(1,0);
  BIT_FLIP(1,0);
#endif
#ifdef TEST_LIGHTS_BUG
  __disable_interrupt();
  // Toggle led state
  P1OUT ^= BIT5;
  if (STATE_READ(led)) {
    BIT_FLIP(1,0);
    STATE_CHANGE(led,0);
  }
  else {
    BIT_FLIP(1,1);
    STATE_CHANGE(led,1);
  }
  TA1R = 0;
  __enable_interrupt();
#endif
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
  PACARANA_INT_END
}


#ifdef RUN_PUDU_PASS
__attribute__((section("__interrupt_vector_timer1_a0"),aligned(2))) void
(*__vector_timer_a1)(void) = Timer1_A0_ISR;
#endif
#endif//TEST_LIGHTS_BUG || TEST_ASYNCH_MULTI
