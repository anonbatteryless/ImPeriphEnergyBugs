
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
#include <math.h>
#include <libparticle/particle.h>
#include "resample_funcs.h"

__nv float w[PARAM];
__nv float prev_ps[PARAM];
__nv float cur_ps[PARAM];
__nv float photo;
__nv float accel;
float sum;

__nv float photo_field[73] =
{0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31 ,13.75
,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15 ,3.48
,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67 ,13.31
,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74 ,5.15
,3.48 ,1.75 ,0.00 ,1.75 ,3.48 ,5.15 ,6.74 ,8.23 ,9.58 ,10.79 ,11.82 ,12.67
,13.31 ,13.75 ,13.97 ,13.97 ,13.75 ,13.31 ,12.67 ,11.82 ,10.79 ,9.58 ,8.23 ,6.74
,5.15};


__nv float accel_cos[73] =
{1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19 ,0.06
,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93 ,-0.97
,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31 ,0.19
,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88 ,-0.93
,-0.97 ,-0.99 ,1.00 ,0.99 ,0.97 ,0.93 ,0.88 ,0.81 ,0.73 ,0.64 ,0.54 ,0.43 ,0.31
,0.19 ,0.06 ,-0.06 ,-0.19 ,-0.31 ,-0.43 ,-0.54 ,-0.64 ,-0.73 ,-0.81 ,-0.88
,-0.93 };

void calc_weights_joint_init(void) {
  for(int i = 0; i < PARAM; i++) {
    cur_ps[i] = 15.0 + 2.5*i;
  }
  sum = 0;
  photo = (((float)my_rand()) / RAND_MAX) * 2000;
  accel = (((float)my_rand()) / RAND_MAX) * 1;
}

//calc_weights_joint(param, w[], photo, accel, cur_ps[]);

void calc_weights_joint_check(void) {
  for(int i = 0; i < PARAM; i++) {
    print_float(w[i]);
  }
  return;
}


