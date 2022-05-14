#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libmspbuiltins/builtins.h>

typedef struct {
	float photo;
} photo_pf_buf;

typedef struct {
	float x;
	float y;
	float z;
} accel_pf_buf;

void print_float(float f);

void print_long(uint32_t l);

//uint16_t my_rand();
unsigned my_rand();

float my_sqrt(float x);

float calc_cos(accel_pf_buf *in);

float get_random_pos();

float gaussian(float mu, float sigma, float x);

float get_random_gauss(float mu, float sigma);

float read_field(float *field, const float pos);

void calc_weights_single(int n, float w[], const float photo, float pos[]);

void calc_weights_joint(int n, float w[], const float photo,
  const float accel, float pos[]);

void resample(int n, float w[], float sum, float prev_ps[], float cur_ps[]);

void move(int param, float *pos);

int use_joint(float sum);

extern float photo_field[];
extern float accel_cos[];


#define T_SENSE 1.0 // every 1 (second?)
#define VEL 2.0
#define SENSE_ERR 5.0
#define MOVE_ERR 1.0

#define SIGMA1 2
//#define SIGMA1 250
#define SIGMA2 0.1
#define MAGIC_VALUE 0.70
//#define MAGIC_VALUE 0.5

