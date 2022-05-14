#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
#include <math.h>
#include "pack_funcs.h"

pkt_t pkt;
samp_t windows[PKT_NUM_WINDOWS];

/* downsample to signed 4-bit int: 4-bit of downsampled data (i.e [-2^3,+2^3])*/
#define PKT_FIELD_MAG_BITS 4
#define SENSOR_BITS_MAG    12
#define NOT_FULL_SCALE_FACTOR_MAG 2 /* n, where scaling factor is decreased by 2^n because
                                       the actual dynamic range of the quantity is smaller than full scale */
#define MAG_DOWNSAMPLE_FACTOR (1 << (SENSOR_BITS_MAG - PKT_FIELD_MAG_BITS - NOT_FULL_SCALE_FACTOR_MAG))


#define PKT_FIELD_ACCEL_BITS 4
#define SENSOR_BITS_ACCEL    16
#define NOT_FULL_SCALE_FACTOR_ACCEL 0 /* n, where scaling factor is decreased by 2^n because
                                       the actual dynamic range of the quantity is smaller than full scale */
#define ACCEL_DOWNSAMPLE_FACTOR (1 << (SENSOR_BITS_ACCEL - PKT_FIELD_ACCEL_BITS - NOT_FULL_SCALE_FACTOR_ACCEL))

#define ACCEL_MIN  (-(1 << (PKT_FIELD_ACCEL_BITS - 1))) // -1 because signed
#define ACCEL_MAX  ((1 << (PKT_FIELD_ACCEL_BITS - 1)) - 1) // -1 because signed, -1 because max value


#define PKT_FIELD_GYRO_BITS 4
#define SENSOR_BITS_GYRO    16
#define NOT_FULL_SCALE_FACTOR_GYRO 1 /* n, where scaling factor is decreased by 2^n because
                                       the actual dynamic range of the quantity is smaller than full scale */
#define GYRO_DOWNSAMPLE_FACTOR (1 << (SENSOR_BITS_GYRO - PKT_FIELD_GYRO_BITS - NOT_FULL_SCALE_FACTOR_GYRO))

#define GYRO_MIN  (-(1 << (PKT_FIELD_GYRO_BITS - 1))) // -1 because signed
#define GYRO_MAX  ((1 << (PKT_FIELD_GYRO_BITS - 1)) - 1) // -1 because signed, -1 because max value

static int scale_mag_sample(int v, int neg_edge, int pos_edge, int overflow)
{
  int scaled;
  if (v == -4096) {
      scaled = overflow;
  } else {
    scaled = v / MAG_DOWNSAMPLE_FACTOR;
    if (scaled < neg_edge) {
      scaled = neg_edge;
    } else if (scaled > pos_edge) {
      scaled = pos_edge;
    }
  }
  return scaled;
}

static int scale_lsm_sample(int v, int factor, int min, int max)
{
  v /= factor;
  if (v < min)
    v = min;
  if (v > max)
    v = max;
  return v;
}


void pack_init(void) {
  // Just need to initialize averages
  for (int i = 0; i < PKT_NUM_WINDOWS; i++) {
    windows[i].temp = 461 + i*10;
    windows[i].mx = 0 + i*5;
    windows[i].my = 2 + i*5;
    windows[i].mz = 4 + i*5;
    windows[i].ax = 22 + i*5;
    windows[i].ay = -35 - i*5;
    windows[i].az = 4000 + i*20;
  }
}

// PKT_NUM_WINDOWS, windows, &pkt
void pack(int n, samp_t *win_avgs, pkt_t *loc_pkt) {
  // zero-out
  for (unsigned i = 0; i < sizeof(pkt_t); ++i) {
      *(((uint8_t *)loc_pkt) + i) = 0x0;
  }

  for( unsigned i = 0; i < n; i++ ){
    //unsigned w = loc_pkt_window_indexes[i];
    samp_t win_avg = *(win_avgs + i);
    /*PRINTF("%u | %u %u %u | %u %u %u\r\n", win_avg.temp, win_avg.mx,
    win_avg.my, win_avg.mz, win_avg.ax, win_avg.ay, win_avg.az);*/

    loc_pkt->windows[i].temp = win_avg.temp; // use full byte

    // Normally, sensor returns a value in [-2048, 2047].
    // On either overflow, sensor return -4096.
    //
    // We shrink the valid range to [-2047,2047] and
    // reserve -2048 for overflow.
    //
    // Then, we also truncate.
    int abs_edge = 1 << (PKT_FIELD_MAG_BITS - 1); // -1, ie. div by 2, because signed
    int neg_edge = -(abs_edge - 1); // reserve for overflow
    int pos_edge = abs_edge - 1;
    int overflow = -abs_edge;

    loc_pkt->windows[i].mx = scale_mag_sample(win_avg.mx, neg_edge, pos_edge, overflow);
    loc_pkt->windows[i].my = scale_mag_sample(win_avg.my, neg_edge, pos_edge, overflow);
    loc_pkt->windows[i].mz = scale_mag_sample(win_avg.mz, neg_edge, pos_edge, overflow);

    // Accel and gyro are simple (since there's no special overflow value)
    loc_pkt->windows[i].ax = scale_lsm_sample(win_avg.ax, ACCEL_DOWNSAMPLE_FACTOR, ACCEL_MIN, ACCEL_MAX);
    loc_pkt->windows[i].ay = scale_lsm_sample(win_avg.ay, ACCEL_DOWNSAMPLE_FACTOR, ACCEL_MIN, ACCEL_MAX);
    loc_pkt->windows[i].az = scale_lsm_sample(win_avg.az, ACCEL_DOWNSAMPLE_FACTOR, ACCEL_MIN, ACCEL_MAX);
#ifdef ENABLE_GYRO
    loc_pkt->windows[i].gx = scale_lsm_sample(win_avg.gx, GYRO_DOWNSAMPLE_FACTOR, GYRO_MIN, GYRO_MAX);
    loc_pkt->windows[i].gy = scale_lsm_sample(win_avg.gy, GYRO_DOWNSAMPLE_FACTOR, GYRO_MIN, GYRO_MAX);
    loc_pkt->windows[i].gz = scale_lsm_sample(win_avg.gz, GYRO_DOWNSAMPLE_FACTOR, GYRO_MIN, GYRO_MAX);
#endif // ENABLE_GYRO
  }
/*    PRINTF("loc_pkt (len %u): ", sizeof(pkt_t));
    for (unsigned i = 0; i < sizeof(pkt_t); ++i) {
        PRINTF("%02x ", *(((uint8_t *)loc_pkt) + i));
    }
    PRINTF("\r\n");*/

}


void pack_check() {
    PRINTF("pkt (len %u): ", sizeof(pkt_t));
    for (unsigned i = 0; i < sizeof(pkt_t); ++i) {
        PRINTF("%02x ", *(((uint8_t *)&pkt) + i));
    }
    PRINTF("\r\n");

}
