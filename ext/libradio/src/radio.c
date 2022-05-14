#include <libio/console.h>
#include <libmsp/mem.h>
#include <libmsp/periph.h>
#include <libmsp/clock.h>
#include <libmsp/watchdog.h>
#include <libmsp/gpio.h>
#include <libmsp/sleep.h>
#include <libfxl/fxl6408.h>
#include <libmspuartlink/uartlink.h>
#include <libcapybara/board.h>
#include <libradio/radio.h>

#if BOARD_MAJOR != 2
#warning "problem! error defining board"
#endif



radio_buf_t radio_buff_internal[LIBRADIO_BUFF_LEN + 1];
radio_buf_t *radio_buff = (radio_buf_t *) (radio_buff_internal + 1);

static inline void radio_on()
{
#if (BOARD_MAJOR == 1 && BOARD_MINOR == 0) || \
  (BOARD_MAJOR == 2 && BOARD_MINOR == 0)

#if PORT_RADIO_SW != PORT_RADIO_RST // we assume this below
#error Unexpected pin config: RAD_SW and RAD_RST not on same port
#endif // PORT_RADIO_SW != PORT_RADIO_RST
  GPIO(PORT_RADIO_SW, OUT) |= BIT(PIN_RADIO_SW) | BIT(PIN_RADIO_RST);
  GPIO(PORT_RADIO_SW, DIR) |= BIT(PIN_RADIO_SW) | BIT(PIN_RADIO_RST);
  GPIO(PORT_RADIO_RST, OUT) &= ~BIT(PIN_RADIO_RST);

#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1
  fxl_set(BIT_RADIO_SW | BIT_RADIO_RST);
  fxl_clear(BIT_RADIO_RST);

#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know how to turn off radio (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
}

static inline void radio_off()
{
#if (BOARD_MAJOR == 1 && BOARD_MINOR == 0) || \
  (BOARD_MAJOR == 2 && BOARD_MINOR == 0)
#if (PORT_RADIO_RST == 4 && PIN_RADIO_RST == 1 && PORT_RADIO_SW == 4 && \
  PIN_RADIO_SW == 0)
  #warning "Correct rst sw config!\r\n"
#endif
  GPIO(PORT_RADIO_RST, OUT) |= BIT(PIN_RADIO_RST); // reset for clean(er) shutdown
  msp_sleep(1);
  GPIO(PORT_RADIO_SW, OUT) &= ~BIT(PIN_RADIO_SW);
#elif BOARD_MAJOR == 1 && BOARD_MINOR == 1
  fxl_clear(BIT_RADIO_SW);
#else // BOARD_{MAJOR,MINOR}
#error Unsupported board: do not know how to turn on radio (see BOARD var)
#endif // BOARD_{MAJOR,MINOR}
}


/*
 * @brief: activates the radio on the CAPYBARA platform
 */
#if (BOARD_MAJOR == 2 && BOARD_MINOR == 0) 
void radio_send() { 
  printf("Sending!\r\n");
  radio_on();
  __delay_cycles(120000);
  uartlink_open_tx();
  uartlink_send(radio_buff_internal, LIBRADIO_BUFF_LEN);
  uartlink_close();
  // TODO: wait until radio is finished; for now, wait for 0.25sec
  __delay_cycles(120000);
  radio_off();
  return;
}
void radio_send_one_off() { 
  //printf("Sending!\r\n");
  radio_on();
  __delay_cycles(90000);
  uartlink_open_tx();
  uartlink_send(radio_buff_internal, LIBRADIO_BUFF_LEN);
  uartlink_close();
  __delay_cycles(80000);
  // To prevent the long stall, we'll just skip turning off the radio
  radio_off();
  return;
}
#endif
