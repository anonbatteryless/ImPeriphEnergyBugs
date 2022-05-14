#ifndef CAPY_BOARD_INIT_H
#define CAPY_BOARD_INIT_H

void i2c_setup();
void capybara_init();
void fxl_reset();
#if BOARD_MAJOR == 1 && BOARD_MINOR == 0
#define PORT_SENSE_SW 3
#define PIN_SENSE_SW  7


#define PORT_RADIO_SW 3
#define PIN_RADIO_SW  2

#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1

#define PORT_PHOTO_SENSE 2
#define PIN_PHOTO_SENSE 3 // GPIO extender pins
#define BIT_CCS_WAKE  (1 << 2)
#define BIT_SENSE_SW  (1 << 3)
#define BIT_PHOTO_SW  (1 << 4)
#define BIT_APDS_SW   (1 << 5)
#define BIT_RADIO_RST (1 << 6)
#define BIT_RADIO_SW  (1 << 7)

#elif BOARD_MAJOR == 2
#define PORT_RADIO_SW 4
#define PIN_RADIO_SW  0

#define PORT_RADIO_RST 4
#define PIN_RADIO_RST  1

#define HARVESTER_DISABLE \
  P2OUT |= BIT7; P2DIR |= BIT7;

#define BIT_HMC_DRDY (1 << 0)
#define BIT_LSM_INT1 (1 << 1)
#define BIT_LSM_INT2 (1 << 2)
#define BIT_LPS_INT  (1 << 3)
#define BIT_APDS_SW  (1 << 4)
#define BIT_APDS_INT (1 << 5)
#define BIT_PHOTO_SW (1 << 6)
#define BIT_SENSE_SW (1 << 7)

#endif // BOARD.{MAJOR,MINOR}

#endif//CAPY_BOARD_INIT_H
