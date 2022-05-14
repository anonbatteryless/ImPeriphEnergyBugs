#ifndef __PACK_FUNCS_H_
#define __PACK_FUNCS_H_

// This is the knob parameter for update_window_start
#define WINDOW_SIZE 16 /*number of samples in a window*/

#define NUM_WINDOWS 4
#define WINDOW_DIV_SHIFT 2 /* 2^WINDOW_DIV_SHIFT = NUM_WINDOWS */
#define WINDOWS_SIZE 16 /* NUM_WINDOWS * WINDOW_SIZE (libchain needs a literal number) */

// This is our knob parameter for pack_funcs!
//#define PKT_NUM_WINDOWS 2
#define PKT_NUM_WINDOWS 16


typedef struct _samp_t{
  int temp;
  int mx;
  int my;
  int mz;
  int ax;
  int ay;
  int az;
#ifdef ENABLE_GYRO
  int gx;
  int gy;
  int gz;
#endif // ENABLE_GYRO
} samp_t;

typedef struct __attribute__((packed)) {
    int temp:8;
    int mx:4;
    int my:4;
    int mz:4;
    int ax:4;
    int ay:4;
    int az:4;
#ifdef ENABLE_GYRO
    int gx:4;
    int gy:4;
    int gz:4;
#endif // ENABLE_GYRO
} pkt_win_t;

typedef struct __attribute__((packed)) {
    pkt_win_t windows[PKT_NUM_WINDOWS];
} pkt_t;

extern pkt_t pkt;
extern samp_t windows[PKT_NUM_WINDOWS];
extern samp_t samples[WINDOW_SIZE];
extern samp_t avg;
#endif
