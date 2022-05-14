#include <msp430.h>
#include <libmspware/driverlib.h>
#include <libio/console.h>
#include <libmspbuiltins/builtins.h>
#include <libpacarana/pacarana.h>

#define SCALER 100

#include "accl.h"
#include "gyro.h"
#include "lsm6ds3.h"

extern volatile __nv int accel_status;

static void write_register(uint8_t reg,uint8_t val) {
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

static uint8_t read_register(uint8_t reg) {
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

static void set_i2c_address(uint8_t addr) {
  // Set slave address //
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = addr; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable
  while (UCB0STATW & UCBBUSY); // is bus busy? then wait!
}

void accelerometer_read(uint16_t *x, uint16_t *y, uint16_t *z) {
  uint8_t temp_l, temp_h;
  uint8_t status;
  set_i2c_address(ACCL_I2C_ADDRESS);
  status = read_register(LSM6DS3_ACC_GYRO_STATUS_REG);
  while(!(status & XL_MASK)) {
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

#define LOC_SCALER 100

int accel_only_init_odr_hm(LSM6DS3_ACC_GYRO_ODR_XL_t rate, bool highperf) {
  // Set slave address //
  UCB0CTLW0 |= UCSWRST; // disable
  UCB0I2CSA = ACCL_I2C_ADDRESS; // Set slave address
  UCB0CTLW0 &= ~UCSWRST; // enable
  uint8_t temp = read_register(ACCL_ID_ADDRESS);
  if(temp != ACCL_ID_RETURN) {
    //PRINTF("Error initializing gyro!\r\n");
    return -1;
    //while(1);
  }

  if(!highperf) {
    // Change CTRL6 and CTRL7 to low power
    set_i2c_address(ACCL_I2C_ADDRESS);
    // Set bit 4 of CTRL6 to 1
    write_register(LSM6DS3_ACC_GYRO_CTRL6_C, 0x10);
  }

  uint8_t dataToWrite = 0;
  // Set up the accelerometer
  dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
  dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_8g;
  dataToWrite |= rate;

  set_i2c_address(ACCL_I2C_ADDRESS);
  write_register(LSM6DS3_ACC_GYRO_CTRL1_XL, dataToWrite);

  return 0;
}

void lsm_accel_disable(void) {
  set_i2c_address(ACCL_I2C_ADDRESS);
  write_register(LSM6DS3_ACC_GYRO_CTRL1_XL,0x0);
  STATE_CHANGE(accel,0); //TODO add back in
  return;
}

