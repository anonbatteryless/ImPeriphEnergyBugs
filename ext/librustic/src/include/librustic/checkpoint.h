#ifndef _LIBRUSTIC_CHECKPOINT_H_
#define _LIBRUSTIC_CHECKPOINT_H_
#include <stdint.h>
#include <stddef.h>
#include <libmsp/mem.h>
#include <libmsp/periph.h>
#include <libcapybara/power.h>

//things other pieces might need to see
extern unsigned chkpt_finished;
extern unsigned __numBoots;
extern unsigned chkpt_first_taken;
extern unsigned atomic_depth;
extern __nv unsigned int volatile librustic_timer_grab;
typedef enum { Atomic, Jit, Sync} Mode;
#pragma warning "Adding librustic timer"
#define NUM_LIBRUSTIC_ENTRIES 1

/* Checkpoint context */
typedef struct _context_t {
  /* Execution mode: atomic, jit, sync(?) */
  Mode curr_m;
  /* Track number of entries to roll back*/
  unsigned numRollBack;
  /* Track number of reboots for an atomic block */
  unsigned numReboots;
  unsigned buf[NUM_LIBRUSTIC_ENTRIES];
} context_t;

#ifndef LIBRUSTIC_UNSAFE
extern uint8_t* data_src[];
extern uint8_t* data_dest[];
extern unsigned data_size[];
extern uint8_t** data_src_base;
extern uint8_t** data_dest_base;
extern unsigned* data_size_base;

extern context_t * volatile curctx;
//for double buffering
extern context_t context_0;
extern context_t context_1;


/** @brief LLVM generated function that clears all isDirty_ array */
extern void clear_isDirty();
#endif

/*Entry function of the rust app*/
extern void _entry();
void entry();

/*Function Declarations*/
void log_entry(uint8_t *data_src, uint8_t *data_dest, size_t var_size);

void commit(void);
void checkpoint(void);
void pre_atomic_reboot(unsigned *buf_vals[NUM_LIBRUSTIC_ENTRIES]);
void on_atomic_reboot(void);
void end_atomic(void);
void start_atomic(unsigned *buf_vals[NUM_LIBRUSTIC_ENTRIES]);
void restore_vol(void);
void entry(void);
void COMP_VBANK_ISR(void);

extern unsigned *librustic_buff_vals[NUM_LIBRUSTIC_ENTRIES];

// Programmers declare this and use = {&mem_1, &mem_2, ... } to set it
#define SEMI_SAFE_ATOMIC_BUFFER_SET \
  __nv unsigned *librustic_buff_vals[NUM_LIBRUSTIC_ENTRIES]

#define SEMI_SAFE_ATOMIC_BUFFER \
  librustic_buff_vals

#define UNSAFE_ATOMIC_START \
  do { \
    volatile int _dummy;\
    start_atomic(librustic_buff_vals); \
    _dummy = 0;\
  } while(0);

#define  UNSAFE_ATOMIC_END \
  do { \
    end_atomic(); \
  } while(0);

#endif
