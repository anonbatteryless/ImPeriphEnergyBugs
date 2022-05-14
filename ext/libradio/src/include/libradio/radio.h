#ifndef _RADIO_H
#define _RADIO_H
#include <stdint.h>

#define LIBRADIO_BUFF_LEN 8

typedef uint8_t radio_buf_t;
extern radio_buf_t *radio_buff;

void radio_send(void);
void radio_send_one_off(void);

#endif
