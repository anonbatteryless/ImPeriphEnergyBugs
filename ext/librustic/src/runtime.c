#include <msp430.h>
#include "checkpoint.h"
#include <libio/console.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
//Debug logging, not checkpoint logging
#define LOGGING 0
#define MAIN 1

__nv unsigned chkpt_finished = 0;
__nv unsigned atomic_depth = 0;

__nv unsigned __numBoots = 0;
__nv unsigned true_first = 1;
__nv unsigned int volatile librustic_timer_grab = 0;

__nv unsigned int * SAVED_PC = (unsigned int *) 0xFB80;
__nv unsigned int* CURR_SP = (unsigned int *) 0xFBC4;
__nv unsigned int* DEBUG_LOC = (unsigned int *) 0xFBC0;

//local helper funcs
void save_stack();
//void restore_stack();
inline void  restore_stack() __attribute__((always_inline));

extern int __stack; //should be the lowest addr in the stack;

/**
 * @brief double buffered context
 */
__nv context_t context_1 = {
			    .numRollBack = 0,
          .curr_m = Jit,
          .numReboots = 0,
          .buf = {0}
};

__nv context_t context_0 = {
			    .numRollBack = 0,
          .curr_m = Jit,
          .numReboots = 0,
          .buf = {0}
};


// LIBRUSTIC_UNSAFE indicates that we're not using llvm to do our logging for us, so we
// also don't have an external runtime library to provide curctx
#ifdef LIBRUSTIC_UNSAFE
/**
 * @brief current context pointer
 */
__nv context_t volatile *curctx = &context_1;
#else
/**
 * @brief dirtylist to save src address
 */
__nv uint8_t** data_src_base = &data_src;
/**
 * @brief dirtylist to save dst address
 */
__nv uint8_t** data_dest_base = &data_dest;
/**
 * @brief dirtylist to save size
 */
__nv unsigned* data_size_base = &data_size;

/**
 * @brief len of dirtylist
 */
__nv volatile unsigned num_dirty_gv=0;
#endif


void on_atomic_reboot()
{
  if(__numBoots == 0xFFFF) {
#ifndef LIBRUSTIC_UNSAFE
    clear_isDirty();
#endif
    __numBoots++;
  }
  // Check how many times we've rebooted on this patch of code
  if (curctx->numReboots > 3) {
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
  }
  // Not ideal that this is unbuffered but whatever
  curctx->numReboots += 1;
#ifndef LIBRUSTIC_UNSAFE
  //rollback log entries
  while(curctx->numRollBack) {
    unsigned rollback_idx =curctx->numRollBack -1;
    uint8_t* loc_data_dest = *(data_dest_base + rollback_idx);
    uint8_t* loc_data_src = *(data_src_base + rollback_idx);
    unsigned loc_data_size = *(data_size_base + rollback_idx);
    memcpy(loc_data_dest, loc_data_src, loc_data_size);
    curctx->numRollBack--;
  }
#endif

}

// Cleans out the buffer and needs to be placed *before* any PC specific
// initialization code... I'm looking at you peripherals
void pre_atomic_reboot(unsigned *buf_vals[NUM_LIBRUSTIC_ENTRIES])
{
  if (curctx->curr_m != Atomic) {
    return;
  }
  for (int i = 0; i < NUM_LIBRUSTIC_ENTRIES; i++) {
    // Write the buffered values back into the addresses we were tracking
    *(buf_vals[i])  =  curctx->buf[i];
  }
  return;
}

//End of Atomic section/start of jit (or sync?) region, switch context to refer to next section
//thereby commiting this atomic region
//watch out for atomicity bugs
void end_atomic()
{
  //must be double buffered in the case that power fails here
  //jit not yet enabled
  context_t* next_ctx;
  //get which pointer it should be, to always have a valid context
  next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
  next_ctx->curr_m = Jit;
  next_ctx->numReboots = 0;

  //atomic update of context
  curctx = next_ctx;

  COMP_VBANK(INT) |= COMP_VBANK(IE);


}

static volatile unsigned dummy = 0;


//End of JIT section/start of atomic, switch context to refer to next section
//checkpoint volatile state.
//watchout for atomicity bugs
void start_atomic(unsigned *buf_vals[NUM_LIBRUSTIC_ENTRIES])
{
  //must be double buffered in the case that power fails here
  //jit still enabled
  context_t* next_ctx;
  //get which pointer it should be, to always have a valid context
  next_ctx = (curctx == &context_0 ? &context_1 : &context_0);
  next_ctx->curr_m = Atomic;
  if (buf_vals != NULL) {
    for (int i = 0; i < NUM_LIBRUSTIC_ENTRIES; i++) {
      // Read from the addresses we're tracking into current buffer
      next_ctx->buf[i] = *(buf_vals[i]);
    }
  }
  else {
    // TODO figure out if we actually want this here, at least it'll prevent
    // reading in random values
    for (int i = 0; i < NUM_LIBRUSTIC_ENTRIES; i++) {
      next_ctx->buf[i] = 0;
    }
  }
  //atomic update of context
  curctx = next_ctx;

  // Disabling vbank interrupts here so we don't accidentally call a checkpoint
  // from the vbank interrupt handler
  COMP_VBANK(INT) &= ~COMP_VBANK(IE);
  //checkpoint the volatile state?
  //after roll back, reboot in atomic region will start from here
  //TODO what happens if we fail in the checkpoint??? The checkpoint is *not*
  //set up for double buffering
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  checkpoint();
  // Added so that on reboot we'll disable the vbank interrupt again before we
  // try to enter the atomic region
  volatile dummy = 0;
  // Added here since we've got a valid checkpoint and restore_vol will
  // overwrite the checkpoint status
  chkpt_finished = 1;
  // We reenable comp_vbank so that we *do* get the interrupt and prevent
  // coldstart
  COMP_VBANK(INT) |= COMP_VBANK(IE);
}

#ifndef LIBRUSTIC_UNSAFE
/*Add an entry to the log*/
void log_entry(uint8_t* orig_addr, uint8_t* backup_addr, size_t var_size)
{
  unsigned nrb = curctx->numRollBack;
  *(data_size_base + nrb) = var_size;
  *(data_dest_base + nrb) = orig_addr;
  *(data_src_base + nrb) = backup_addr;
  curctx->numRollBack = nrb + 1;
}
#endif

#ifdef LIBRUSTIC_USEMSP430X
void checkpoint() {
  //save the registers
  //start with the status reg
  __asm__ volatile ("MOV R2, &0x0FB88");
  // Move this one so we have it 
  __asm__ volatile ("MOVX.A R4, &0x0000FB90");

  // This is R1 (SP), but it will be R0 (PC) on restore (since we don't want to
  // resume in chckpnt obvi, but after the return)
  // Need to add 4 to checkpoint 0(R1) so we skip "capybara_shutdown" call in
  // interrupt handler when we restore, but subtract that 4 so we do call
  // capybara shutdown.
  // TODO This means we need a sacrificial instruction after every call to
  // checkpoint under different circumstances... gotta make it less janky
  __asm__ volatile ("MOVX.A 0(R1), R4"); // move old pc to r4
  __asm__ volatile ("ADDA #4, R4"); // inc
  __asm__ volatile ("MOVX.A R4, &0x0000FB80"); //Set pc for after reboot
  __asm__ volatile ("SUBA #4, R4"); // dec
  __asm__ volatile ("MOVX.A R4, 0(R1)"); // write back to stack
  __asm__ volatile ("MOVX.A R1, &0x0000FBC4"); //Curr sp
  __asm__ volatile ("ADDA #4, R1"); // inc cur stack to get rid of calla push
  __asm__ volatile ("MOVX.A R1, &0x0000FB84"); // write to mem
  __asm__ volatile ("SUBA #4, R1"); // put back to waht it should be

  #if LOGGING
  unsigned i  = 0;
  while(i < 50) {
    PRINTF("checkpoint: first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *((unsigned int*) 0x0FB80),
    *((unsigned int*)0x0FB84));
    i++;
  }
  i = 0;
#endif
  //R3 is constant generator, doesn't need to be restored

  //the rest are general purpose regs
  __asm__ volatile ("MOVX.A R5, &0x0FB94");
  __asm__ volatile ("MOVX.A R6, &0x0FB98");
  __asm__ volatile ("MOVX.A R7, &0x0fb9c");

  __asm__ volatile ("MOVX.A R8, &0x0FBA0");
  __asm__ volatile ("MOVX.A R9, &0x0FBA4");
  __asm__ volatile ("MOVX.A R10, &0x0FBA8");
  __asm__ volatile ("MOVX.A R11, &0x0fbac");

  __asm__ volatile ("MOVX.A R12, &0x0FBB0");
  __asm__ volatile ("MOVX.A R13, &0x0FBB4");
  __asm__ volatile ("MOVX.A R14, &0x0FBB8");
  __asm__ volatile ("MOVX.A R15, &0x0fbbc");

  save_stack();
  __asm__ volatile ("MOVX.A &0x0FB90, R4");
  chkpt_finished = 1;
}


void restore_vol() {
  //restore the registers

  if (!chkpt_finished) {
    // 3 flips is an error code...
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    return;
    }
  unsigned i = 0;
  chkpt_finished = 0;
  __numBoots +=1;

  __asm__ volatile ("MOVX.A R1, &0x0000FBC0");
  __asm__ volatile ("MOVX.A &0x0FB84, R1");
  restore_stack();
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;


#if LOGGING
  while(i < 50) {
    PRINTF("sp done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.A &0x0FB90, R4");
  __asm__ volatile ("MOVX.A &0x0FB94, R5");
  __asm__ volatile ("MOVX.A &0x0FB98, R6");
  __asm__ volatile ("MOVX.A &0x0fb9c, R7");

  #if LOGGING
  while(i < 50) {
    PRINTF("first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *(DEBUG_LOC), *((unsigned int*)0xFB84));
    i++;
  }
  i = 0;
  #endif
  __asm__ volatile ("MOVX.A &0x0FBB0, R12");
  __asm__ volatile ("MOVX.A &0x0FBB4, R13");
  __asm__ volatile ("MOVX.A &0x0FBB8, R14");
  __asm__ volatile ("MOVX.A &0x0fbbc, R15");

#if LOGGING
  while(i < 50) {
    PRINTF("third batch done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.A &0x0FBA0, R8");
  __asm__ volatile ("MOVX.A &0x0FBA4, R9");
  __asm__ volatile ("MOVX.A &0x0FBA8, R10");
  __asm__ volatile ("MOVX.A &0x0fbac, R11");

  //last but not least, move regs 2 and 0
  __asm__ volatile ("MOVX.A &0x0FB84, R1");
  __asm__ volatile ("MOVX.A &0x0FB88, R2");
  __asm__ volatile ("MOVX.A &0x0FB80, R0");
  //pc has been changed, can't do anything here!!
}
#else //TODO confirm this still works
void checkpoint() {
  //save the registers
  //start with the status reg
  __asm__ volatile ("MOV R2, &0xFB88");

  // This is R1 (SP), but it will be R0 (PC) on restore (since we don't want to
  // resume in chckpnt obvi, but after the return)
  // Need to add 4 to checkpoint 0(R1) so we skip "capybara_shutdown" call in
  // interrupt handler when we restore, but subtract that 4 so we do call
  // capybara shutdown.
  // TODO This means we need a sacrificial instruction after every call to
  // checkpoint under different circumstances... gotta make it less janky
  __asm__ volatile ("ADD #4, 0(R1)");
  __asm__ volatile ("MOVX.W 0(R1), &0xFB80");
  __asm__ volatile ("SUB #4, 0(R1)");

  __asm__ volatile ("MOVX.W R1, &0xFBC4");
  //what we want R1 to be on restoration
  // Using 2 instead of 4 because reasons...
  __asm__ volatile ("ADD #2, R1");
  __asm__ volatile ("MOVX.W R1, &0xFB84");
  __asm__ volatile ("SUB #2, R1");

  #if LOGGING
  unsigned i  = 0;
  while(i < 50) {
    PRINTF("checkpoint: first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *((unsigned int*) 0xFB80), *((unsigned int*)0xFB84));
    i++;
  }
  i = 0;
#endif


  //R3 is constant generator, doesn't need to be restored

  //the rest are general purpose regs
  __asm__ volatile ("MOVX.W R4, &0xFB90");
  __asm__ volatile ("MOVX.W R5, &0xFB94");
  __asm__ volatile ("MOVX.W R6, &0xFB98");
  __asm__ volatile ("MOVX.W R7, &0xfb9c");

  __asm__ volatile ("MOVX.W R8, &0xFBA0");
  __asm__ volatile ("MOVX.W R9, &0xFBA4");
  __asm__ volatile ("MOVX.W R10, &0xFBA8");
  __asm__ volatile ("MOVX.W R11, &0xfbac");

  __asm__ volatile ("MOVX.W R12, &0xFBB0");
  __asm__ volatile ("MOVX.W R13, &0xFBB4");
  __asm__ volatile ("MOVX.W R14, &0xFBB8");
  __asm__ volatile ("MOVX.W R15, &0xfbbc");

  save_stack();

  chkpt_finished = 1;
}

void restore_vol() {
  //restore the registers

  if (!chkpt_finished) {
    // 3 flips is an error code...
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    return;
    }
  unsigned i = 0;
  chkpt_finished = 0;
  __numBoots +=1;

  __asm__ volatile ("MOVX.W R1, &0xFBC0");
  __asm__ volatile ("MOVX.W &0xFB84, R1");

  restore_stack();


#if LOGGING
  while(i < 50) {
    PRINTF("sp done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.W &0xFB90, R4");
  __asm__ volatile ("MOVX.W &0xFB94, R5");
  __asm__ volatile ("MOVX.W &0xFB98, R6");
  __asm__ volatile ("MOVX.W &0xfb9c, R7");

  #if LOGGING
  while(i < 50) {
    PRINTF("first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *(DEBUG_LOC), *((unsigned int*)0xFB84));
    i++;
  }
  i = 0;
  #endif
  __asm__ volatile ("MOVX.W &0xFBB0, R12");
  __asm__ volatile ("MOVX.W &0xFBB4, R13");
  __asm__ volatile ("MOVX.W &0xFBB8, R14");
  __asm__ volatile ("MOVX.W &0xfbbc, R15");

#if LOGGING
  while(i < 50) {
    PRINTF("third batch done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.W &0xFBA0, R8");
  __asm__ volatile ("MOVX.W &0xFBA4, R9");
  __asm__ volatile ("MOVX.W &0xFBA8, R10");
  __asm__ volatile ("MOVX.W &0xfbac, R11");

  //last but not least, move regs 2 and 0
  __asm__ volatile ("MOVX.W &0xFB84, R1");
  __asm__ volatile ("MOVX.W &0xFB88, R2");
  __asm__ volatile ("MOVX.W &0xFB80, R0");
  //pc has been changed, can't do anything here!!
}
#endif


/*Function that copies the stack to nvmem*/
void save_stack()
{
  uint16_t *stack_start = (uint16_t*)(&__stack);
#if LOGGING
  PRINTF("save: stack is from %u to %u\r\n", stack_start, *CURR_SP);
#endif
  //PRINTF("ss %u sp %u\r\n", stack_start, *CURR_SP);
  unsigned int* save_point = 0xFBC8;
  // Had to reverse direction because MSP430 stack grows down
  if ((unsigned int)stack_start - (unsigned int)(*(CURR_SP)) > 0x400) {
    PRINTF("Stack overflow!\r\n");
  }
  while( (unsigned int)stack_start > *CURR_SP) {
    *save_point = *stack_start;
    save_point++;
    stack_start--;
    //PRINTF("%u = %u\r\n",*stack_start, *save_point);
  }
 // PRINTF("save pt %u\r\n", save_point);
}


/*Function that restores the stack from nvmem
  Note: this NEEDS to be inlined so that the return address pushed to the stack
  doesn't get clobbered. I.e. if the function is inlined there is no return
  address to worry about.*/
void inline restore_stack()
{
  uint16_t *stack_start = (uint16_t*)(&__stack);
  //PRINTF("restore: stack is from %u to %u\r\n", stack_start, *((unsigned int*)CURR_SP));
#if LOGGING
  PRINTF("restore: stack is from %u to %u\r\n", stack_start, *CURR_SP);
#endif
  unsigned int* save_point = 0xFBC8;
  while( (unsigned)stack_start > *CURR_SP) {
    *stack_start = *save_point;
    save_point++;
    stack_start--;
    //PRINTF("%u = %u\r\n",*stack_start, *save_point);
  }
}

//entry after reboot
// call init and appropriate rb function for current Mode.
//#if MAIN

void entry() {
  P1OUT |= BIT4;
  P1DIR |= BIT4;
  P1OUT &= ~BIT4;

  if(true_first) {
    //in the case that the first checkpoint in the program is not reached.
    //need to make sure true_first is accurate.
#if LOGGING
    PRINTF("TRUE FIRST %u\r\n",&__stack);
#endif
    checkpoint();
    // We need the dummy set to follow checkpoint() so that we don't skip the
    // "true_first" set
    dummy = 0;
    true_first = 0;
    return;
    //dead region
  }

  switch(curctx->curr_m) {
  case Atomic:
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    on_atomic_reboot();
    //fall through
  default:
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    P1OUT |= BIT4;
    P1DIR |= BIT4;
    P1OUT &= ~BIT4;
    restore_vol();
    //deadregion
  }
  return;
}
//#endif

__attribute__ ((interrupt(COMP_VECTOR(LIBCAPYBARA_VBANK_COMP_TYPE))))
void COMP_VBANK_ISR (void)
{
    switch (__even_in_range(COMP_VBANK(IV), 0x4)) {
      case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IIFG):
          break;
      case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IFG):
          COMP_VBANK(INT) &= ~COMP_VBANK(IE);
          COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
          if (curctx->curr_m == Atomic) {
            P1OUT |= BIT3;
            P1DIR |= BIT3;
            P1OUT &= ~BIT3;
            P1OUT |= BIT3;
            P1DIR |= BIT3;
            P1OUT &= ~BIT3;
            librustic_timer_grab = TA0R;
#ifdef LIBRUSTIC_TIMERA
            librustic_timer_grab = TA0R;
#endif
            capybara_shutdown();
          }
          else {
            // Checkpoint needs to be **immediately** followed by capybara
            // shutdown for this to work
            P1OUT |= BIT3;
            P1DIR |= BIT3;
            P1OUT &= ~BIT3;
            librustic_timer_grab = TA0R;
#ifdef LIBRUSTIC_TIMERA
#pragma warning "adding librustic timer!"
            librustic_timer_grab = TA0R;
#endif
            checkpoint();
            capybara_shutdown();
          }
          break;
  }
}


