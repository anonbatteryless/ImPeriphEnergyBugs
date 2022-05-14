#include <msp430.h>
#include "pacarana.h"
#include "stdio.h"
#include <stdarg.h>
#include <libfxl/fxl6408.h>
#include "break_evens.h"

#include <librustic/checkpoint.h>

#include "uses_vector_init.h"

volatile __nv pacarana_active_vect_t __paca_active_uses = 0;//TODO only works for 16 uses
volatile __nv uint8_t __paca_toggles[NUM_STORED_PERIPHS];
volatile __nv uint16_t __paca_global_ovfl = 0; // Need to carry over power failures
volatile __nv uint16_t __paca_last_ovfl = 0;

void possible_states_inner(const char *periph,...) {
  return;
}


int __paca_dummy_func(int a, int b,...) {
  if (a > -2) {return 1;}
  printf("%x\r\n",a+b); 
  va_list test;
  va_start(test,a);
  int total = 0;
  for (int i = 0; i < a; i++) {
    total += va_arg(test,int);
  }
  va_end(test);
  return total;
}

int __paca_read_table(int func, int *state, int periph) {
	if (*state > 0) {
		return *(__paca_all_tables[periph - 1]+NUM_FUNCS*(*state-1) + func);
	}
	else {
		return 0;
	}
}


void __paca_test_log_func(int x) {
    printf("Outputing from pacarana-pass: %u\r\n",x);
}

void paca_update_pfs(void) {
  // check if there were active timers at failure
  // Intentionally approximate-- double incrementing here is fine, if we're not
  // making it all the way through this, then the energy buffer isn't big enough
  // to support peripheral intensive apps
  if (__paca_active_uses) {
    for (int i = 0; i < (sizeof(pacarana_active_vect_t) << 3); i++) {
      if (__paca_active_uses & (0x1 << i)) {
        __paca_periph_uses[i].pf++;
#ifdef LIBPACARANA_SEPARATE_PF
        if (__paca_periph_uses[i].pf > 2) {
          uint8_t periph_num = __paca_periph_uses[i].periph;
          // Change the toggle bit
          __paca_toggles[periph_num] = 1;
          // Change state so we don't reboot with dev on
          *__paca_status_ptrs[periph_num] = 0;
        }
#endif
      }
    }
    // Copy overflows, again, we allow the possibility of a WAR conflict, but we
    // do want to guarantee that these are updated atomically, so we'll disable
    // interrupts so we don't trigger jit checkpointing. 
    // It's fine that we clobber __paca_global_ovfl if we have a power failure
    // before a timer stop since if a timer has experienced 2 reboots, that's a
    // point in the "do toggle" column
    __disable_interrupt();
    __paca_last_ovfl =  __paca_global_ovfl;
    __paca_global_ovfl = 0;
    __enable_interrupt();
    // Trigger clock again 
    TA0CCR0 = 0xffff;
    TA0CTL = TASSEL__ACLK | MC__UP | ID_3 | TAIE_1;
    TA0CCTL0 |= CCIE;
    TA0R = 0; //TODO check if we need
  }
}

#if 0
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

#endif


#ifdef LIBPACARANA_FULLY_DYNAMIC
#warning "FULLY DYNAMIC!"
//volatile __nv unsigned int __paca_index_extended = 0;

__nv pacarana_active_vect_t __paca_periph_maps[NUM_STORED_PERIPHS] = {0}; //supports 4 periphs
uint16_t paca_insert(uint16_t pc) {
  uint16_t index = pc >> 2; // Shear off bottom 2 bits
  index &= 0xf; //TODO make this general for all periph_uses vector sizes
  uint16_t start = index;
  while(__paca_periph_uses[index].pc != 0) {
    index++;
    if (index > 15){
      index = 0;
    }
    if (index == start) {
      printf("Error: table full!\r\n");
      while(1); }
  }
  // Update periph uses and claim spot
  __paca_periph_uses[index].pc = pc;
  return index;
}
#endif

// Starts the timer between peripheral uses
// vars we need to double buffer for running as part of a task:
// paca_periph_uses.pc (accessed in insert), __paca_index_extended (if
// GRAB_START_TIME ever appears *before* start, use->active, paca_active_uses
int __paca_timer_start(unsigned int pc, int periph, int periph_status) {
  // Check if timer for current usage is active (it shouldn't be)
#ifndef LIBPACARANA_FULLY_DYNAMIC
  pacarana_periph_usage_t *use = &__paca_periph_uses[pc];
#else
  uint16_t index,flag=0;
  // Get vector spots corresponding to this periph
  if(__paca_periph_maps[periph]) {
    //Check if this is already mapped
    for (int i = 0; i < sizeof(pacarana_active_vect_t) << 3; i++) {
      if (__paca_periph_maps[periph] & (0x1 << i)) {
        //Check this spot in vector
        if (__paca_periph_uses[i].pc == pc) {
          index = i;
          flag = 1;
          break;
        }
      }
    }
  }
  if (!flag) {
    index = paca_insert(pc);
    __paca_periph_maps[periph] |= 1 << index;
    //printf("inserting into: %u\r\n",index);
  }
  __paca_index_extended = index;
  pacarana_periph_usage_t *use = &__paca_periph_uses[index];
  //printf("Start on %i\r\n",index);
  //printf("Start: Looking at use: %x, %x %x. Actives: %x\r\n", index, use->periph,
  //use->periph_status,__paca_active_uses); 
#endif
  if( use->active) {
    // Error!
    printf("Peripheral is already active! :( :( %u\r\n",periph);
    while(1);
  }
  // We're ok with these updates being interrupted as long as they're restarted
  // from the same spot, init only looks at __paca_active_uses
  use->active = 1;
  use->periph_status = periph_status;
  use->periph = periph;
  //printf("periph: %x periph_status: %x\r\n",use->periph, use->periph_status);
  __paca_active_uses |= (1 << index);
  // Check if timer is running for a different peripheral
  if( !(TA0CTL & (0x0010))) { // checks if we're in MC_UP
    __disable_interrupt();
    TA0CCR0 = 0xffff;
    TA0CTL = TASSEL__ACLK | MC__UP | ID_3 | TAIE_1;
    TA0CCTL0 |= CCIE;
    TA0R = 0; //TODO check if we need
    __enable_interrupt();
  }
  // Update timer **outside** this function so we can do it _after_ the toggle
#ifndef LIBPACARANA_MULTI_CNT
  if (use->cntr >= 2) {
    return 1;
  }
  else {
    return 0;
  }
#else
  uint16_t mask = 0b11 << ((periph_status -1) << 1);
  uint16_t val = (use->cntr & mask) >> ((periph_status - 1) << 1);
  if ( val >= 2) {
    return 1;
  }
  else {
    return 0;
  }
#endif
}


// Stops the timer before another peripheral use
// Note: this is done on a peripheral by peripheral basis, so need to pass in
// peripheral we're looking at
// Vars to double buffer for tasks: use->ts, use->ovfl, use->active, use->cntr
void __paca_timer_stop(int periph) {
  __disable_interrupt();
  uint16_t stop_time = TA0R;
  uint16_t stop_time_ovfl = __paca_global_ovfl;
  __enable_interrupt();
  // Note: if we fail after this, that's fine, we already grabbed the stop_time
  uint16_t time = 0, ovfl = 0;
  // Look at periph mask, figure out which ones are active
  // TODO rework this loop-- it's gross
  for (int i = 0; i < (sizeof(pacarana_active_vect_t) << 3); i++) {
    // Check spots associated with this periph
    if( __paca_periph_maps[periph] & (0x1 << i)) {
      if (!__paca_periph_uses[i].active) {
        continue; }
    }
    else {
      // We need to keep on keeping on if bit isn't active
      continue;
    }
    int cur_index;
    cur_index = i;
    // Find an active use for the given periph
    pacarana_periph_usage_t *use = &__paca_periph_uses[i];
    //printf("Stop:Looking at use: %x, %x %x. Actives: %x map: %x\r\n",cur_index, use->periph,
    //use->periph_status,__paca_active_uses,__paca_periph_maps[periph]); 
    // No borrow from ovfl
    // Just set time to infinity if we've rebooted twice with this timer still
    // active
    //TODO make this smarter...
    if ( use->pf > 1 ) {
      time = 0xffff;
      ovfl = 0xff;
      use->pf = 0;
    }
    else if ( use->pf == 1 ) {
      if (use->ts < librustic_timer_grab) {
        time = librustic_timer_grab - use->ts + stop_time;
        ovfl = __paca_last_ovfl - use->ovfl + stop_time_ovfl;
      }
      else { //Handles borrow from overflow
        time = 0xffff - use->ts;
        time += librustic_timer_grab;
        time += stop_time;
        ovfl = __paca_last_ovfl - use->ovfl + stop_time_ovfl ;
        ovfl--;
      }
      use->pf = 0;
    }
    else {
      if (use->ts < stop_time) {
        time = stop_time - use->ts;
        ovfl = stop_time_ovfl - use->ovfl;
      }
      else { //Handles borrow from overflow
        time = 0xffff - use->ts;
        time += stop_time;
        ovfl = stop_time_ovfl - use->ovfl;
        ovfl--;
      }
    }
    // Deactivate usage and update counter
    use->active = 0;
    use->ovfl = 0;
    use->ts = 0;
    __paca_active_uses &= ~(1 << i); // clear bit in active vector


    uint32_t expanded_time = (uint32_t)((uint32_t)ovfl << 16);
    expanded_time += time;
    uint32_t be = (uint32_t)__paca_break_evens[use->periph][use->periph_status-1];

#ifndef LIBPACARANA_MULTI_CNT
    if ( expanded_time > be ) {
      if ( use->cntr < 3 ) {
        use->cntr++;
      }
    }
    else if (use->cntr > 0) {
        use->cntr--;
    }
#ifdef LIBPACARANA_PRINTF
    printf( "%x %x, time: %x, cnt: %x, global: %x\r\n",ovfl, time,stop_time,
    use->cntr,__paca_global_ovfl);
#endif
#else
    // Implement counter per peripheral state using masking
    uint16_t mask = 0b11 << ((use->periph_status -1) << 1);//<<1 b/c *2 may fail
    uint16_t val = (use->cntr & mask) >> ((use->periph_status - 1) << 1 );
    // Clear counter
    use->cntr &= ~(mask);
    if ( expanded_time > be ) {
#ifdef LIBPACARANA_PRINTF
      printf("Greater! %u\r\n",val);
#endif
      if ( val  < 3 ) {
        val++;
      }
    }
    else if ( val > 0 ) {
      val--;
    }
#ifdef LIBPACARANA_PRINTF
      //printf("be: %x [%x][%x]\r\n",be,use->periph, use->periph_status -1 );
#endif
    use->cntr |= val << ((use->periph_status - 1) << 1);
#ifdef LIBPACARANA_PRINTF
    printf( "Time:%x %x, %x|\r\n",ovfl, time,(uint16_t)be);
    printf("index: %x val: %u\r\n",cur_index,val);
#endif
#endif
    break; // we add this here because there should only ever be one active
  }

  // Turn off timer if no peripherals are active
  if (!__paca_active_uses) {
    TA0CTL = MC__STOP;
    __paca_global_ovfl = 0;
    //printf("Disabling clock!\r\n");
  }
  return;
}


void __attribute ((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR(void)
{ //Handles overflows
  __disable_interrupt();
  __paca_global_ovfl++;
  TA0R = 0;
  __enable_interrupt();// A little paranoia over comp_e getting thrown
  //Note: shutdown interrupt in librustic will handle grabbing final TA0R,
  //because COMP_E has high priority than TimerA, TimerA will never preempt
  //COMP_E and put TA0R and __paca_global_ovfl out of whack
}
