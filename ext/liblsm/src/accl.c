#include <msp430.h>
#include <libmspware/driverlib.h>
#include <libio/console.h>
#include <libmspbuiltins/builtins.h>

#include "accl.h"
#include "lsm6ds3.h"

void write_register(uint8_t reg,uint8_t val) {
  UCB0CTLW0 |= UCTR | UCTXSTT; // transmit mode and start
  while((UCB0CTLW0 & UCTXSTT)); // wait for addr transmission to finish

  while(!(UCB0IFG & UCTXIFG)); // wait for txbuf to empty
  UCB0TXBUF = reg;

  while(!(UCB0IFG & UCTXIFG)); // wait for txbuf to empty
  UCB0TXBUF = val;

  while(!(UCB0IFG & UCTXIFG)); // wait for txbuf to empty
  UCB0CTLW0 |= UCTXSTP; // stop

  while (UCB0STATW & UCBBUSY); // wait until bus is quiet

  return;
}

uint8_t read_register(uint8_t reg) {
  while (UCB0STATW & UCBBUSY); // is bus busy? then wait!
  // Query gyro reg
  UCB0CTLW0 |= UCTR | UCTXSTT; // transmit mode and start
  while(!(UCB0IFG & UCTXIFG)); // wait until txbuf is empty

  UCB0TXBUF = reg; // fill txbuf with reg address

  while(!(UCB0IFG & UCTXIFG)); // wait until txbuf is empty

  UCB0CTLW0 &= ~UCTR; // receive mode
  UCB0CTLW0 |= UCTXSTT; // repeated start

  // wait for addr transmission to finish, data transfer to start
  while(UCB0CTLW0 & UCTXSTT);

  UCB0CTLW0 |= UCTXSTP; // stop

  while(!(UCB0IFG & UCRXIFG)); // wait until txbuf is empty
  uint8_t val = UCB0RXBUF; // read out of rx buf

  while (UCB0STATW & UCBBUSY); // hang out until bus is quiet

  return val;
}

void set_i2c_address(uint8_t addr) {
  // Set slave address //
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = addr; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable
  while (UCB0STATW & UCBBUSY); // is bus busy? then wait!
}

void accelerometer_init_data_rate(LSM6DS3_ACC_GYRO_ODR_XL_t rate) {
  __delay_cycles(48000);
  // Set slave address //
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = ACCL_I2C_ADDRESS; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable
  uint8_t temp = read_register(ACCL_ID_ADDRESS);
  if(temp != ACCL_ID_RETURN) {
    PRINTF("Error initializing gyro!\r\n");
    while(1);
  }
  uint8_t dataToWrite = 0;
  // Set up the accelerometer
  dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
  dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_8g;
  dataToWrite |= rate;
  // Change CTRL6 and CTRL7 to low power
  set_i2c_address(ACCL_I2C_ADDRESS);
  // Set bit 4 of CTRL6 to 1
  write_register(LSM6DS3_ACC_GYRO_CTRL6_C, 0x10);

  write_register(LSM6DS3_ACC_GYRO_CTRL1_XL, dataToWrite);
  return;
}

void accelerometer_init_data_rate_hm(LSM6DS3_ACC_GYRO_ODR_XL_t rate, bool
highperf) {
  __delay_cycles(48000);
  // Set slave address //
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = ACCL_I2C_ADDRESS; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable
  uint8_t temp = read_register(ACCL_ID_ADDRESS);
  if(temp != ACCL_ID_RETURN) {
    PRINTF("Error initializing gyro!\r\n");
    while(1);
  }
  uint8_t dataToWrite = 0;
  // Set up the accelerometer
  dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
  dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_8g;
  dataToWrite |= rate;
  if (!highperf) {
  // Change CTRL6 and CTRL7 to low power
  set_i2c_address(ACCL_I2C_ADDRESS);
  // Set bit 4 of CTRL6 to 1
  write_register(LSM6DS3_ACC_GYRO_CTRL6_C, 0x10);
  }

  write_register(LSM6DS3_ACC_GYRO_CTRL1_XL, dataToWrite);
  return;
}

void accelerometer_init() {
  // Set slave address //
  uint8_t temp;
  set_i2c_address(ACCL_I2C_ADDRESS);
  /*UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = ACCL_I2C_ADDRESS; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable*/
  temp = read_register(ACCL_ID_ADDRESS);
  //PRINTF("read\r\n");
  if(temp != ACCL_ID_RETURN) {
    PRINTF("Error initializing gyro!\r\n");
    while(1);
  }
  uint8_t dataToWrite = 0;
  PRINTF("Running setup\r\n");
  // Set up the accelerometer
  dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
  dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_8g;
  dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_104Hz;
    
  // Change CTRL6 and CTRL7 to low power
  set_i2c_address(ACCL_I2C_ADDRESS);
  // Set bit 4 of CTRL6 to 1
  write_register(LSM6DS3_ACC_GYRO_CTRL6_C, 0x10);

  write_register(LSM6DS3_ACC_GYRO_CTRL1_XL, dataToWrite);  
  //__delay_cycles(160000);
}


void dummy_accel_read(uint16_t *x, uint16_t *y, uint16_t *z) {
  uint8_t temp_l, temp_h;
  uint8_t status;
  /*
  set_i2c_address(ACCL_I2C_ADDRESS);
  status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  while(!(status & XL_MASK)) {
    __delay_cycles(500);
    set_i2c_address(ACCL_I2C_ADDRESS);
    status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  }*/

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTX_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTX_H_XL);

  *x = (temp_h << 8) + temp_l;

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTY_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTY_H_XL);

  *y = (temp_h << 8) + temp_l;

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTZ_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTZ_H_XL);

  *z = (temp_h << 8) + temp_l;

  return;
}

#define SCALER 100
float accelerometer_read_x() {
	int16_t x;
  uint8_t temp_l, temp_h;
  uint8_t status;
  set_i2c_address(ACCL_I2C_ADDRESS);
  status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  while(!(status & XL_MASK)) {
    __delay_cycles(100);
    set_i2c_address(ACCL_I2C_ADDRESS);
    status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  }

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTX_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTX_H_XL);

  x = (temp_h << 8) + temp_l;
	return (float)((float)x/SCALER);
}



void accelerometer_disable() {
  set_i2c_address(ACCL_I2C_ADDRESS);
  write_reg(LSM6DS3_ACC_GYRO_CTRL1_XL,0x0);
  write_reg(LSM6DS3_ACC_GYRO_CTRL2_G,0x0);
  return;
}

void accelerometer_read_profile() {
  set_i2c_address(ACCL_I2C_ADDRESS);
  uint8_t temp = read_register(ACCL_ID_ADDRESS);
  return;
}

void accelerometer_write_profile() {
  set_i2c_address(ACCL_I2C_ADDRESS);
  write_reg(LSM6DS3_ACC_GYRO_CTRL1_XL,0x0);
  return;
}

float accelerometer_read_z() {
	int16_t x;
  uint8_t temp_l, temp_h;
  uint8_t status;
  set_i2c_address(ACCL_I2C_ADDRESS);
  status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  while(!(status & XL_MASK)) {
    __delay_cycles(100);
    set_i2c_address(ACCL_I2C_ADDRESS);
    status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  }

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTZ_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTZ_H_XL);

  x = (temp_h << 8) + temp_l;
	return (float)((float)x/SCALER);
}

void read_xl(int16_t *x, int16_t *y, int16_t *z) {
  uint8_t temp_l, temp_h;
  uint8_t status;
  set_i2c_address(ACCL_I2C_ADDRESS);
  status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  while(!(status & 0x1)) { //Check XL data ready bit in status reg
    __delay_cycles(100);
    set_i2c_address(ACCL_I2C_ADDRESS);
    status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
    //printf("test_ready");
  }

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTX_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTX_H_XL);

  *x = (temp_h << 8) + temp_l;

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTY_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTY_H_XL);

  *y = (temp_h << 8) + temp_l;

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_l = read_register(LSM6DS3_ACC_GYRO_OUTZ_L_XL);

  set_i2c_address(ACCL_I2C_ADDRESS);
  temp_h = read_register(LSM6DS3_ACC_GYRO_OUTZ_H_XL);

  *z = (temp_h << 8) + temp_l;

  return;
}

