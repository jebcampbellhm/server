#pragma once
#include <stdint.h>

// RX_START programs an integer factor ahead of the fixed 256 decimation.
// Never advertise a divisor which differs from the one programmed in hardware.
inline bool rx_rate_divisors(uint32_t adc_hz, uint32_t rate_hz,
                             int& total, int& programmable) {
    total = programmable = 0;
    if (!adc_hz || !rate_hz || rate_hz > UINT32_MAX / 256U) return false;
    const uint32_t quantum = rate_hz * 256U;
    if (adc_hz % quantum) return false;
    const uint32_t factor = adc_hz / quantum;
    // Stock-compatible RX_START range documented by the public driver port.
    if (factor < 5U || factor > 40U) return false;
    programmable = int(factor);
    total = int(factor * 256U);
    return true;
}
