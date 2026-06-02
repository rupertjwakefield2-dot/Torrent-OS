#pragma once
#include <stdint.h>

void    lapic_init(void);          /* detect, enable, set spurious vector */
void    lapic_eoi(void);           /* send end-of-interrupt */
uint32_t lapic_id(void);           /* local APIC ID of this CPU */
int     lapic_present(void);       /* 1 if LAPIC detected */
