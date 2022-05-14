#include <msp430.h>
#include <stdio.h>
#include <libmspware/driverlib.h>
#include <libmspware/gpio.h>

#include <libmsp/watchdog.h>
#include <libmsp/clock.h>
#include <libmsp/gpio.h>
#include <libmsp/periph.h>
#include <libmsp/sleep.h>
#include <libmsp/mem.h>
#include <libmsp/uart.h>
//#include <libmspuartlink/uartlink.h>

#if (BOARD_MAJOR == 1 && BOARD_MINOR == 1) || BOARD_MAJOR == 2
#warning Adding libfxl
#include <libfxl/fxl6408.h>
#endif// BOARD_{MAJOR,MINOR}

//Other stuff
#include <libio/console.h>
#include "reconfig.h"
#include "power.h"
#include "capybara.h"
#include "board.h"
#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x

/** @brief Handler for capybara power-on sequence
    TODO add this to libcapybara...
*/
EUSCI_B_I2C_initMasterParam params = {
	.selectClockSource = EUSCI_B_I2C_CLOCKSOURCE_SMCLK,
  .dataRate = EUSCI_B_I2C_SET_DATA_RATE_400KBPS,
	.byteCounterThreshold = 0,
  .autoSTOPGeneration = EUSCI_B_I2C_NO_AUTO_STOP
};

void capybara_init(void) {
    msp_watchdog_disable();
    msp_gpio_unlock();
		__enable_interrupt();
// Don't wait if we're on continuous power
#ifndef LIBCAPYBARA_CONT_POWER
#pragma message ("continuous power not defined!")
capybara_wait_for_supply();
#if (BOARD_MAJOR == 1 && BOARD_MINOR == 1) || BOARD_MAJOR == 2
   capybara_wait_for_vcap();
#endif // BOARD_{MAJOR,MINOR}
#endif // LIBCAPYBARA_CONT_POWER
    capybara_config_pins();
    msp_clock_setup();
// Set up deep_discharge stop
#ifndef LIBCAPYBARA_CONT_POWER
#if BOARD_MAJOR == 1 && BOARD_MINOR == 0
    capybara_shutdown_on_deep_discharge();
#elif (BOARD_MAJOR == 1 && BOARD_MINOR == 1) || BOARD_MAJOR == 2
    if (capybara_shutdown_on_deep_discharge() == CB_ERROR_ALREADY_DEEPLY_DISCHARGED) {
      capybara_shutdown();
    }
#endif //BOARD.{MAJOR,MINOR}
#endif //LIBCAPYBARA_CONT_POWER

#if BOARD_MAJOR == 1 && BOARD_MINOR == 0
    GPIO(PORT_SENSE_SW, OUT) &= ~BIT(PIN_SENSE_SW);
    GPIO(PORT_SENSE_SW, DIR) |= BIT(PIN_SENSE_SW);

    GPIO(PORT_RADIO_SW, OUT) &= ~BIT(PIN_RADIO_SW);
    GPIO(PORT_RADIO_SW, DIR) |= BIT(PIN_RADIO_SW);

    P3OUT &= ~BIT5;
    P3DIR |= BIT5;

    P3OUT &= ~BIT0;
    P3DIR |= BIT0;
    GPIO(PORT_DEBUG, OUT) &= ~BIT(PIN_DEBUG);
    GPIO(PORT_DEBUG, DIR) |= BIT(PIN_DEBUG);
#elif (BOARD_MAJOR == 1 && BOARD_MINOR == 1)

    INIT_CONSOLE();
    __enable_interrupt();
    msp_gpio_unlock();
    LOG2("Setting up i2c\r\n");
    EUSCI_B_I2C_setup();
    LOG2("fxl init\r\n");
#ifndef LIBCAPYBARA_DISABLE_FXL
    fxl_init();
    fxl_out(BIT_PHOTO_SW);
    fxl_out(BIT_RADIO_SW);
    fxl_out(BIT_RADIO_RST);
    fxl_out(BIT_APDS_SW);
    fxl_pull_up(BIT_CCS_WAKE);
    LOG2("Done fxl!\r\n");
#else
    #pragma message "Disabled libfxl"
#endif //FXL_DISABLE

#elif BOARD_MAJOR == 2
  #warning BOARD_MAJOR is 2
//#ifndef LIBCAPYBARA_LEAN_INIT
  INIT_CONSOLE();
//#endif
  __enable_interrupt();
  msp_gpio_unlock();
  LOG2("Setting up i2c\r\n");
  
  #ifndef LIBCAPYBARA_DISABLE_FXL
  //TODO figure out if this is actually better than using i2c_setup
  //EUSCI_B_I2C_setup();
  LOG2("fxl init\r\n");
  params.i2cClk = CS_getSMCLK();
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P1,
			GPIO_PIN6 + GPIO_PIN7,
			GPIO_SECONDARY_MODULE_FUNCTION
			);
	EUSCI_B_I2C_initMaster(EUSCI_B0_BASE, &params);
	//fxl_init();
	//i2c_setup();
  fxl_init();
  LOG2("SENSE_SW\r\n");
  fxl_out(BIT_PHOTO_SW);
  fxl_out(BIT_SENSE_SW);
  fxl_out(BIT_APDS_SW);
  #ifndef LIBCAPYBARA_LEAN_INIT
  fxl_in(BIT_APDS_INT);
  fxl_in(BIT_HMC_DRDY);
  fxl_in(BIT_LSM_INT1);
  fxl_in(BIT_LSM_INT2);
  #endif
  #endif

#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know what pins to configure (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
    __enable_interrupt();
}

void fxl_reset() {
  // Reset i2c
  params.i2cClk = CS_getSMCLK();
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P1,
			GPIO_PIN6 + GPIO_PIN7,
			GPIO_SECONDARY_MODULE_FUNCTION
			);
	EUSCI_B_I2C_initMaster(EUSCI_B0_BASE, &params);
  #ifndef LIBCAPYBARA_DISABLE_FXL
    #if (BOARD_MAJOR == 2) || (BOARD_MAJOR == 1 && BOARD_MINOR == 1)
      fxl_init();
      LOG2("SENSE_SW\r\n");
      #if (BOARD_MAJOR == 2) 
        fxl_out(BIT_PHOTO_SW);
        fxl_out(BIT_SENSE_SW);
        fxl_out(BIT_APDS_SW);
        fxl_in(BIT_APDS_INT);
        fxl_in(BIT_HMC_DRDY);
        fxl_in(BIT_LSM_INT1);
        fxl_in(BIT_LSM_INT2);
      #else
        fxl_out(BIT_PHOTO_SW);
        fxl_out(BIT_RADIO_SW);
        fxl_out(BIT_RADIO_RST);
        fxl_out(BIT_APDS_SW);
        fxl_pull_up(BIT_CCS_WAKE); 
      #endif //v2.0 or v1.1
    #endif //v2.0 or v1.1
  #endif //DISABLE_FXL
  return;
}


