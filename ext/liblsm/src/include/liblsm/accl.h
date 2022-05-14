#ifndef __ACCL_H__
#define __ACCL_H__
#include "lsm6ds3.h"
#include <stdbool.h>

void accelerometer_init();
void accelerometer_init_data_rate(LSM6DS3_ACC_GYRO_ODR_XL_t rate);
void dummy_accel_read(uint16_t *x, uint16_t *y, uint16_t *z);
void accelerometer_read(uint16_t *x, uint16_t *y, uint16_t *z);
void accelerometer_disable();
void accelerometer_read_profile();
void accelerometer_write_profile();
void accelerometer_init_data_rate_hm(LSM6DS3_ACC_GYRO_ODR_XL_t rate, bool
highperf);
float accelerometer_read_x(void);
float accelerometer_read_z(void);
int accel_only_init_odr_hm(LSM6DS3_ACC_GYRO_ODR_XL_t rate, bool highperf);
void read_xl(int16_t *x, int16_t *y, int16_t *z);

#define ACCL_I2C_ADDRESS 0x6B
#define ACCL_ID_ADDRESS 0x0F /*WhoAmI (ID) register*/
#define ACCL_ID_RETURN 0x69 /*Value of WhoAmI register*/
#define XL_MASK 0x1

#endif
