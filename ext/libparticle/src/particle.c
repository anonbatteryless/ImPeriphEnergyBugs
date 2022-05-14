#include "particle.h"
#include <msp430.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libpacarana/pacarana.h>

#undef __hifram
#define __hifram __nv

#define PI 3.1415926535
#define HALF_PI 1.570796

#define STEP 5

#if  1
void print_float(float f)
{
	if (f < 0) {
		PRINTF("-");
		f = -f;
	}

	PRINTF("%u.",(int)f);
	//PRINTF("%u.",(unsigned)f);

	// Print decimal -- 3 digits
	PRINTF("%u", (((int)(f * 1000)) % 1000));
	//PRINTF("%u", (((unsigned)(f * 1000)) % 1000));
  PRINTF(" ");
}
void print_long(uint32_t l) {
  uint16_t cast_lower, cast_upper;
  cast_upper = (uint16_t)((l & 0xFFFF0000)>>16);
  cast_lower = (uint16_t)(l & 0xFFFF);
  PRINTF("%x%x ",cast_upper,cast_lower);
}
#else
#define print_long(...)
#define print_float(...)
#endif
/************* All the math stuffs of my own ***************/
/* ------------------------------my_rand()-------------------------------------
	 my_rand, implementation of random number generater described by S. Park
	 and K. Miller, in Communications of the ACM, Oct 88, 31:10, p. 1192-1201.
	 ---------------------------------------------------------------------------- */
#define RAND_MAX 65535

__hifram unsigned seed = 7738;

unsigned my_rand()
{
	uint32_t a = 16807L, m = 2147483647L, q = 127773L, r = 2836L;
	//uint16_t a = 16807L, m = 2147483647L, q = 127773L, r = 2836L;

	uint32_t lo, hi, test;
	//uint16_t lo, hi, test;

	hi = seed / q;
	lo = seed % q;
	test = a * lo - r * hi;

	if (test > 0) seed = test;        /* test for overflow */
	else seed = test + m;
	return (unsigned)(seed % 65536);
}

float my_sqrt(float x) {
	float z = x / 2;
	for (unsigned i = 0; i < 1000; ++i) {
		z -= (z*z - x) / (2*z);
		if ((z*z - x) < x * 0.01) {
			return z;
		}
	}
	return z;
}

#if 0
// we're just using a map of the cosine values because acos is too big a
// function to include
float calc_cos(accel_pf_buf *in) {
  // z/sqrt(x**2+y**2+z**2)
  float x_sq = (in->x)*(in->x);
  float y_sq = (in->y)*(in->y);
  float z_sq = (in->z)*(in->z);
  float sum = x_sq + y_sq + z_sq;
  float magnitude = in->z/my_sqrt(sum);
  // Sign calculation
  if (in->y < 0) {
    magnitude *= -1.0;
  }
  return magnitude;
}
#endif
/**************** Math ends here **************/

// Return between -180 ~ 180
float get_random_pos()
{
	//float r = (((float)rand()) / RAND_MAX) * 360;
	unsigned r = my_rand();
	float res = (((float)r) / RAND_MAX) * 360;
	return (res - 180);
}


__hifram float z1;
__hifram float z1_ready = 0;

float get_random_gauss(float mu, float sigma)
{
	if (z1_ready) {
		z1_ready = 0;
		return z1 * sigma + mu;
	}

	float u1 = ((float)(my_rand())) / RAND_MAX;
	float u2 = ((float)(my_rand())) / RAND_MAX;

	float z0 = my_sqrt(-2.0 * logf(u1)) * cos(2*PI*u2);
	z1 = my_sqrt(-2.0 * logf(u1)) * cos(2*PI*u2 - HALF_PI);
	z1_ready = 1;
	return z0 * sigma + mu;
}

void NO_PERIPH move(int param, float *pos)
{ for (int i = 0; i < param; i++) {
    *(pos + i) += VEL * T_SENSE;
    *(pos + i) += get_random_gauss(0.0, MOVE_ERR) * T_SENSE;
    *(pos + i) = fmod(((*(pos + i)) + 180), 360) - 180;
  }
	return;
}

// Read the field value according to the pos from the map
// This is the part where we use our map to figure out what we should be seeing
float read_field(float *field, const float pos)
{
	float p = pos + 180;

	// Linear interpolation
	float val0 = field[((int)p) / STEP];
	float x = p - (((int)p) / STEP)*STEP;
	float val1 = field[((int)p) / STEP + 1];

	return (val0 * (STEP - x) + val1 * x) / STEP;
}

void NO_PERIPH calc_weights_single(int n, float w[], const float photo, float pos[])
{
	float prob;
  for (int i = 0 ; i < n ; i++) {
    prob = 1.0;
    // Get the mag of the pos
    float my_photo = read_field(photo_field,pos[i]);
    prob *= gaussian(my_photo, SIGMA1, photo);
    w[i] = prob;

  }
	return;
}

#if 0
void NO_PERIPH calc_weights_joint(int n, float w[], const float photo,
  const float accel, float pos[])
{
	float prob = 1.0;
  for (int i = 0; i < n ; i++) {
    // Get the mag of the pos
    float my_photo = read_field(photo_field,pos[i]);
    prob *= gaussian(my_photo, SIGMA1, photo);

    // Add in probability of seeing accel value
    float my_accel = read_field(accel_cos,pos[i]);
    prob *= gaussian(my_accel, SIGMA2, accel);
    w[i] = prob;
  }
	return;
}
#endif

void NO_PERIPH resample(int n, float w[], float sum, float prev_ps[], float cur_ps[])
{
  float max_w = 0;
  for (unsigned i = 0; i < n; ++i) {
    w[i] /= sum;
    if (w[i] > max_w) {
      max_w = w[i];
    }
  }
  // Resample particles so we get the higher probability particles
  unsigned idx = (int)((((float)my_rand()) / RAND_MAX) * n);
  //unsigned idx = (unsigned)((((float)my_rand()) / RAND_MAX) * n);
  //unsigned idx = (((n*my_rand()) / RAND_MAX));
  float beta = 0.0;
  for (unsigned i = 0; i < n; ++i) {
    beta += (((float)my_rand()) / RAND_MAX) * max_w * 2;
    while (beta > w[idx]) {
      beta -= w[idx];
      idx = (idx + 1) % n;
    }
    prev_ps[i] = cur_ps[idx];
  }
  return;
}

float gaussian(float mu, float sigma, float x)
{
	return exp2f(- ((mu - x) * (mu - x) / 100000) / (sigma * sigma) / 2.0) / my_sqrt(2.0 * PI * (sigma * sigma));
}

int use_joint(float sum) {
  if (sum < MAGIC_VALUE) {
    return 1;
  }
  else {
    return 0;
  }
}
