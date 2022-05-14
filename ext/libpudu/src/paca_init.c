#include <msp430.h>
#include "pacarana.h"
#include "stdio.h"
#include <stdarg.h>
#include <libfxl/fxl6408.h>

#include <librustic/checkpoint.h>

#ifdef LIBPACARANA_FULLY_DYNAMIC
volatile __nv unsigned int __paca_index_extended = 0;
#endif

// Note: this runs before restore, so effectively it runs without support for
// intermittent execution
void paca_init(unsigned cfg) {
#warning "in paca_init!"
  // run fxl initialization
  fxl_set(inits[cfg].fxl_byte);
  // Walk through the init functions (needs to run all the way through every
  // time, so i is volatile)
  for(int i = 0; i < inits[cfg].num_funcs; i++) {
    inits[cfg].pointertable[i]();
  }
#ifndef LIBPACARANA_SEPARATE_PF
    paca_update_pfs();
#endif
  //TODO add check if we made it all the way through successfully
}
