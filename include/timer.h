#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Timer functions
void timer_init(uint32_t frequency);
void timer_wait(uint32_t ticks);
uint64_t timer_get_ticks(void);
void sleep_ms(uint32_t ms);

#endif
