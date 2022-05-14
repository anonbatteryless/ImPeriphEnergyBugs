__attribute__((section("__interrupt_vector_compe_e"),aligned(2)))
void(*__vector_compe_e)(void) = COMP_VBANK_ISR;

__attribute__((section("__interrupt_vector_timer0_b0"),aligned(2)))
void(*__vector_timer0_b0)(void) = TIMER_ISR(LIBMSP_SLEEP_TIMER_TYPE, LIBMSP_SLEEP_TIMER_IDX, LIBMSP_SLEEP_TIMER_CC);


