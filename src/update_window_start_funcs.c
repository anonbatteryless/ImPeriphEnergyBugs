#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
#include <math.h>
#include "pack_funcs.h"

samp_t samples[WINDOW_SIZE];
samp_t avg;

void update_window_start_init() {
  for (unsigned i = 0; i < WINDOW_SIZE;i++) {
    samples[i].temp = 461 + i*10;
    samples[i].mx = 0 + i*5;
    samples[i].my = 2 + i*5;
    samples[i].mz = 4 + i*5;
    samples[i].ax = 22 + i*5;
    samples[i].ay = -35 - i*5;
    samples[i].az = 4000 + i*20;
  }
}


// INPUTS="'WINDOW_SIZE,samples,&avg'"
void update_window_start(int n,samp_t *windows, samp_t *avg) {
  // not samp_t because need 32-bit to prevent overflow
  long sum_temp = 0;
  long sum_mx = 0, sum_my = 0, sum_mz = 0;
  long sum_ax = 0, sum_ay = 0, sum_az = 0;
#ifdef ENABLE_GYRO
  long sum_gx = 0, sum_gy = 0, sum_gz = 0;
#endif // ENABLE_GYRO


  for(unsigned j = 0; j < n; j++){
    samp_t sample = *(windows + j);
    print_sample(&sample);

    sum_temp += sample.temp;

    sum_mx += sample.mx;
    sum_my += sample.my;
    sum_mz += sample.mz;

    sum_ax += sample.ax;
    sum_ay += sample.ay;
    sum_az += sample.az;

#ifdef ENABLE_GYRO
    sum_gx += sample.gx;
    sum_gy += sample.gy;
    sum_gz += sample.gz;
#endif // ENABLE_GYRO
  }
  LOG("sum done\r\n");

  avg->temp = sum_temp / WINDOW_SIZE;
  avg->mx = sum_mx / WINDOW_SIZE;
  avg->my = sum_my / WINDOW_SIZE;
  avg->mz = sum_mz / WINDOW_SIZE;

  avg->ax = sum_ax / WINDOW_SIZE;
  avg->ay = sum_ay / WINDOW_SIZE;
  avg->az = sum_az / WINDOW_SIZE;

#ifdef ENABLE_GYRO
  avg->gx = sum_gx / WINDOW_SIZE;
  avg->gy = sum_gy / WINDOW_SIZE;
  avg->gz = sum_gz / WINDOW_SIZE;
#endif // ENABLE_GYRO

  LOG("avg: "); print_sample(avg);
  return;
}

void print_sample(samp_t *s) {
  PRINTF("{T:%i,"
      "M:{%i,%i,%i},"
      "A:{%i,%i,%i},"
#ifdef ENABLE_GYRO
      "G:{%i,%i,%i}"
#endif // ENABLE_GYRO
      "}\r\n",
      s->temp,
      s->mx,s->my,s->mz,
      s->ax, s->ay, s->az
#ifdef ENABLE_GYRO
      ,s->gx, s->gy, s->gz
#endif // ENABLE_GYRO
      );
}

void update_window_start_check() {
  PRINTF("Check!\r\n");
  print_sample(&avg);
}
