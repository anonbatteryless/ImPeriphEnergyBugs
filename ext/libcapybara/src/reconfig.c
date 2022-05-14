#include <msp430.h>

#if 0
#define LCBPRINTF printf
#else
#define LCBPRINTF(...)
#endif

#ifdef LIBCAPYBARA_VARTH_ENABLED
#include <libmcppot/mcp4xxx.h>
#endif // LIBCAPYBARA_VARTH_ENABLED

#include <libmspware/driverlib.h>

#ifndef GCC
#include <libmspbuiltins/builtins.h>
#endif //GCC

#include <libmsp/periph.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libio/console.h>
#include "reconfig.h"
#include "power.h"

/* Working config and precharged config */
#ifdef LIBCAPYBARA_VARTH_ENABLED
__nv capybara_cfg_t base_config = {0} ;
#else
__nv capybara_cfg_t base_config = {.banks = 0xFF};
#endif

/* Precharge and Burst status globals */
__nv prechg_status_t prechg_status = 0;
__nv burst_status_t burst_status = 0;
__nv swap_status_t swap_status = 0;


volatile prechg_status_t v_prechg_status;
volatile burst_status_t v_burst_status;

/* Leaving these simple for now... I can't see them ever getting too complicated, but who
 * knows? TODO turn these into macros so we don't have to pay for a function call every
 * single time they're accessed...
 */

prechg_status_t get_prechg_status(void){
    return (prechg_status);
}

int set_prechg_status(prechg_status_t in){
    prechg_status = in;
    return 0;
}

burst_status_t get_burst_status(void){
    return (burst_status);
}

int set_burst_status(burst_status_t in){
    burst_status = in;
    return 0;
}

int set_base_banks(capybara_bankmask_t in){
    base_config.banks = in;
    return 0;
}


#ifdef LIBCAPYBARA_VARTH_ENABLED
#define X(a, b, c) {.banks = b, .vth = c},
#else
#define X(a, b, c) {.banks = b},
#endif

__nv capybara_cfg_t pwr_levels[] = {
    PWR_LEVEL_TABLE
    #undef X
};

// Cycles for the latch cap to charge/discharge
#define SWITCH_TIME_CYCLES 0x1fff // charges to ~2.4v (almost full-scale); discharges to <100mV

#if defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define BANK_PORT_INNER(i) LIBCAPYBARA_BANK_PORT_ ## i ## _PORT
#define BANK_PORT(i) BANK_PORT_INNER(i)

#define BANK_PIN_INNER(i) LIBCAPYBARA_BANK_PORT_ ## i ## _PIN
#define BANK_PIN(i) BANK_PIN_INNER(i)

#define CONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i), DIR) |= BIT(BANK_PIN(i))

#define DISCONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i), DIR) &= ~BIT(BANK_PIN(i))

#elif defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)

#define BANK_PORT_INNER(i, op) LIBCAPYBARA_BANK_PORT_ ## i ## _ ## op ## _PORT
#define BANK_PORT(i, op) BANK_PORT_INNER(i, op)

#define BANK_PIN_INNER(i, op) LIBCAPYBARA_BANK_PORT_ ## i ## _ ## op ## _PIN
#define BANK_PIN(i, op) BANK_PIN_INNER(i, op)

#define CONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i, op), DIR) |= BIT(BANK_PIN(i, op))

#define DISCONNECT_LATCH(i, op) \
        GPIO(BANK_PORT(i, op), DIR) &= ~BIT(BANK_PIN(i, op))

#endif // LIBCAPYBARA_SWITCH_CONTROL

#if defined(LIBCAPYBARA_SWITCH_DESIGN__NC)

#if defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)

#define BANK_CONNECT(i) \
    GPIO(BANK_PORT(i, CLOSE), OUT) |= BIT(BANK_PIN(i, CLOSE))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i, OPEN), OUT) |= BIT(BANK_PIN(i, OPEN))

#elif defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)
#pragma message "running nc"

#define BANK_CONNECT(i) \
        GPIO(BANK_PORT(i), OUT) &= ~BIT(BANK_PIN(i))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i), OUT) |= BIT(BANK_PIN(i))

#else // LIBCAPYBARA_SWITCH_CONTROL
#error Invalid value of config option: LIBCAPYBARA_SWITCH_CONTROL
#endif // LIBCAPYBARA_SWITCH_CONTROL

#elif defined(LIBCAPYBARA_SWITCH_DESIGN__NO)
#pragma message "running no"
#if defined(LIBCAPYBARA_SWITCH_CONTROL__TWO_PIN)
#error Not implemented: switch design NO, switch control TWO PIN
#elif defined(LIBCAPYBARA_SWITCH_CONTROL__ONE_PIN)

#define BANK_CONNECT(i) \
        GPIO(BANK_PORT(i), OUT) |= BIT(BANK_PIN(i))

#define BANK_DISCONNECT(i) \
        GPIO(BANK_PORT(i), OUT) &= ~BIT(BANK_PIN(i))

#endif // LIBCAPYBARA_SWITCH_CONTROL

#else // LIBCAPYBARA_SWITCH_DESIGN
#error Invalid value of config option: LIBCAPYBARA_SWITCH_DESIGN
#endif // LIBCAPYBARA_SWITCH_DESIGN

int capybara_config_banks(capybara_bankmask_t banks)
{
    // If the switches would be all on one port, we'd do this in one
    // assignment. Since on our board, they are hooked up to two
    // ports, we either have optimal code that does two assignments
    // but is not generic, or we have generic code that does four
    // assignments. We do the latter here.

    // NOTE: This is not a loop, because the pins and ports are
    // resolved at compile time. We don't want a runtime map.
#define CONFIG_BANK(i) \
    if (banks & (1 << i)) { BANK_CONNECT(i); } else { BANK_DISCONNECT(i); }
#define DO_CONNECT_LATCH(i) \
    if (banks & (1 << i)) { CONNECT_LATCH(i, CLOSE); } else { CONNECT_LATCH(i, OPEN); }
#define DO_DISCONNECT_LATCH(i) \
    if (banks & (1 << i)) { DISCONNECT_LATCH(i, CLOSE); } else { DISCONNECT_LATCH(i, OPEN); }

    CONFIG_BANK(0);
    CONFIG_BANK(1);
    CONFIG_BANK(2);
    CONFIG_BANK(3);

    // Overlap the delay for all banks

    DO_CONNECT_LATCH(0);
    DO_CONNECT_LATCH(1);
    DO_CONNECT_LATCH(2);
    DO_CONNECT_LATCH(3);

    __delay_cycles(SWITCH_TIME_CYCLES);

    DO_DISCONNECT_LATCH(0);
    DO_DISCONNECT_LATCH(1);
    DO_DISCONNECT_LATCH(2);
    DO_DISCONNECT_LATCH(3);

    return 0;
}

#ifdef LIBCAPYBARA_VARTH_ENABLED
int capybara_config_threshold(uint16_t wiper)
{
    uint16_t curr_wiper = pot_get_nv_wiper();
    if (curr_wiper != wiper) {
        pot_set_nv_wiper(wiper);
        pot_set_v_wiper(wiper); // not clear if redundant, so just in case
    } else {
        pot_set_v_wiper(wiper); // just in case
    }
    return 0;
}
#endif // LIBCAPYBARA_VARTH_ENABLED

int capybara_config(capybara_cfg_t cfg)
{
    int rc;

    rc = capybara_config_banks(cfg.banks);
    if (rc) return rc;

#ifdef LIBCAPYBARA_VARTH_ENABLED
    rc = capybara_config_threshold(cfg.vth);
    if (rc) return rc;
#endif // LIBCAPYBARA_VARTH_ENABLED

    return 0;
}

int capybara_config_max()
{
#ifdef LIBCAPYBARA_VARTH_ENABLED
    capybara_cfg_t cfg = { ~0, POT_RESOLUTION };
#else
    capybara_cfg_t cfg = { ~0 };
#endif // LIBCAPYBARA_VARTH_ENABLED
    return capybara_config(cfg);
}

void capybara_transition(int index)
{
    // need to explore exactly how we want BURST tasks to be followed --> should
    // we ever shutdown to reconfigure? Or should we always ride the burst wave
    // until we're out of energy?
#ifndef LIBCAPYBARA_CONT_POWER

    // Check previous burst state and register a finished burst
    if(burst_status){
        burst_status = 2;
    }
    capybara_task_cfg_t *cur = pwr_configs + index ;
    capybara_cfg_spec_t curpwrcfg = cur->type;
    LCBPRINTF("addr = %x, top = %u type=%x, cur cfg = %x\r\n",
            (pwr_configs + index),(pwr_configs),
            (pwr_configs + index)->type, base_config.banks);
    switch(curpwrcfg){
        case BURST:
            prechg_status = 0;
            swap_status = 0;
            capybara_config_banks(cur->precfg->banks);
            LCBPRINTF("burst! going to %x, banks = %x\r\n",
                cur->precfg->banks, base_config.banks);
            // Change to curpwrcfg->precfg ? or opcfg?
            base_config.banks = cur->precfg->banks;
            swap_status = 1;
            burst_status = 1;
            break;

        case PREBURST:
            if(!prechg_status){
                LCBPRINTF("Precharging! \r\n");
                swap_status = 0;
                base_config.banks = cur->precfg->banks;
                capybara_config_banks(base_config.banks);
                // Mark that we finished the config_banks_command
                swap_status = 1;
                prechg_status = 1;
                capybara_shutdown();
                capybara_wait_for_supply();
            }
            //intentional fall through

        case CONFIGD:
            // TODO: figure out why Capy v2.0 doesn't support the optimization
            // of leaving the banks in the same configuration. The latch caps
            // leak out extremely quickly in v2.0 hardware.
            //if((base_config.banks != cur->opcfg->banks) || (swap_status == 0)){
                LCBPRINTF("New config! going to %x from %x\r\n",
                    cur->opcfg->banks, base_config.banks);
                swap_status = 0;
                base_config.banks = cur->opcfg->banks;
                capybara_config_banks(base_config.banks);
                swap_status = 1;
                capybara_wait_for_supply();
            //}
            //Another intentional fall through

        default:
            break;
    }
#endif
   LCBPRINTF("Running task %u \r\n",curctx->task->idx);

}

