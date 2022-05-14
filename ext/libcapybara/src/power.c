#include <msp430.h>

#include <libmsp/periph.h>
#include <libmsp/sleep.h>
#include <libfxl/fxl6408.h>
#include "power.h"
#include "reconfig.h"
#include "board.h"

void capybara_wait_for_supply()
{
    // wait for BOOST_OK: supply voltage stablized
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IES) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IE) |= BIT(LIBCAPYBARA_PIN_VBOOST_OK);

    __disable_interrupt(); // classic lock-check-sleep pattern
    while ((GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IN) & BIT(LIBCAPYBARA_PIN_VBOOST_OK)) !=
                BIT(LIBCAPYBARA_PIN_VBOOST_OK)) {
				__bis_SR_register(LPM4_bits + GIE);
        __disable_interrupt();
    }
    __enable_interrupt();
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IE) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
    GPIO(LIBCAPYBARA_PORT_VBOOST_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBOOST_OK);
}

void capybara_wait_for_banks()
{
    // wait for VBANK_OK: cap voltage is high enough
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IES) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IE) |= BIT(LIBCAPYBARA_PIN_VBANK_OK);

    __disable_interrupt(); // classic lock-check-sleep pattern
    while ((GPIO(LIBCAPYBARA_PORT_VBANK_OK, IN) & BIT(LIBCAPYBARA_PIN_VBANK_OK)) !=
                BIT(LIBCAPYBARA_PIN_VBANK_OK)) {
        __bis_SR_register(LPM4_bits + GIE);
        __disable_interrupt();
    }
    __enable_interrupt();

    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IE) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
    GPIO(LIBCAPYBARA_PORT_VBANK_OK, IFG) &= ~BIT(LIBCAPYBARA_PIN_VBANK_OK);
}

int capybara_report_vbank_ok()
{   int vbank_ok_val = 0;
    vbank_ok_val = GPIO(LIBCAPYBARA_PORT_VBANK_OK,IN) & BIT(LIBCAPYBARA_PIN_VBANK_OK);
    return vbank_ok_val;
}

void capybara_wait_for_vcap()
{
    // Wait for Vcap to recover
    // NOTE: this is the crudest implementation: sleep for a fixed interval
    // Alternative implemenation: use Vcap supervisor, comparator, or ADC
    msp_sleep(PERIOD_LIBCAPYBARA_VCAP_RECOVERY_TIME);
}

void capybara_shutdown()
{
    // Sleep, while we wait for supply voltage to drop
    __disable_interrupt();
    // Disable booster
    GPIO(LIBCAPYBARA_PORT_BOOST_SW, OUT) |= BIT(LIBCAPYBARA_PIN_BOOST_SW);

    while (1) {
        __bis_SR_register(LPM4_bits);
    }
}

cb_rc_t capybara_shutdown_on_deep_discharge()
{

#if defined(__MSP430FR5949__) || defined(__MSP430FR5994__)
    GPIO(LIBCAPYBARA_VBANK_COMP_PIN_PORT, SEL0) |= BIT(LIBCAPYBARA_VBANK_COMP_PIN_PIN);
    GPIO(LIBCAPYBARA_VBANK_COMP_PIN_PORT, SEL1) |= BIT(LIBCAPYBARA_VBANK_COMP_PIN_PIN);
#else // device
#error Comparator pin SEL config not implemented for device.
#endif // device

    // Configure comparator to interrupt when Vcap drops below a threshold
    COMP_VBANK(CTL3) |= COMP2_VBANK(PD, LIBCAPYBARA_VBANK_COMP_CHAN);
    COMP_VBANK(CTL0) = COMP_VBANK(IMEN) | COMP2_VBANK(IMSEL_, LIBCAPYBARA_VBANK_COMP_CHAN);
    // REF applied to resistor ladder, ladder tap applied to V+ terminal
    COMP_VBANK(CTL2) = COMP_VBANK(RS_2) | COMP2_VBANK(REFL_, LIBCAPYBARA_DEEP_DISCHARGE_REF) |
                       COMP2_VBANK(REF0_, LIBCAPYBARA_DEEP_DISCHARGE_DOWN) |
                       COMP2_VBANK(REF1_, LIBCAPYBARA_DEEP_DISCHARGE_UP);
    // Turn comparator on in ultra-low power mode
    COMP_VBANK(CTL1) |= COMP_VBANK(PWRMD_2) | COMP_VBANK(ON);

    // Let the comparator output settle before checking or setting up interrupt
    msp_sleep(PERIOD_LIBCAPYBARA_VBANK_COMP_SETTLE);

    if (COMP_VBANK(CTL1) & COMP_VBANK(OUT)) {
        // Vcap already below threshold
        return CB_ERROR_ALREADY_DEEPLY_DISCHARGED;
    }

    // Clear int flag and enable int
    COMP_VBANK(INT) &= ~(COMP_VBANK(IFG) | COMP_VBANK(IIFG));
    COMP_VBANK(INT) |= COMP_VBANK(IE);
    return CB_SUCCESS;
}

#ifndef LIBCAPYBARA_JIT
#ifndef LIBCAPYBARA_CONT_POWER
#pragma message "Adding comp_vbank_isr"
// Own the ISR for now, if need be can make a function, to let main own the ISR
__attribute__ ((interrupt(COMP_VECTOR(LIBCAPYBARA_VBANK_COMP_TYPE))))
void COMP_VBANK_ISR (void)
{
    switch (__even_in_range(COMP_VBANK(IV), 0x4)) {
        case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IIFG):
						break;
        case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IFG):
            COMP_VBANK(INT) &= ~COMP_VBANK(IE);
            COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
            capybara_shutdown();
            break;
    }
}
// ONLY NECESSARY IF LLVM VERSION IS OLDER THAN V7
#ifdef LIBCAPYBARA_CLANGISR
#pragma warning "Adding in vector!"
__attribute__((section("__interrupt_vector_comp_e"),aligned(2)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;
#endif //CLANGISR
#endif //CONT_POWER
#endif //JIT


