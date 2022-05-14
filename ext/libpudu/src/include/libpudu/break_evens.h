//TODO make different versions for different clock freqs
#ifndef LIBPACARANA_BREAK_EVENS
#define LIBPACARANA_BREAK_EVENS

#ifdef LIBPACARANA_FAST_CLK
uint16_t __paca_break_evens[5][4] = {
/*accel*/ {875, 837, 0, 0},
/*gyro*/  {1187, 10675, 0, 0},
/*color*/ {1175,0,0,0},
/*prox*/  {100,0,0,0},
/*photo*/ {1000,0,0,0}
};
#else
uint16_t __paca_break_evens[5][4] = {
/*accel*/ {30, 32, 0, 0},
/*gyro*/  {43, 385, 0, 0},
/*color*/ {42,0,0,0},
/*prox*/  {4,0,0,0},
///*apds*/  {43,0,0,0},
/*photo*/ {1000,0,0,0}
};
#endif



#endif
