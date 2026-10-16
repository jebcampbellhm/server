#include "init/rx_rate.h"
#include "rx/iq_timing.h"
#include "zynq/rx_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <initializer_list>

struct Header { unsigned char last_gps_solution, dummy; uint32_t gpssec, gpsnsec; };
int main() {
    int total = 99, factor = 99;
    for (uint32_t clock : {61440000U, 122880000U}) {
        for (uint32_t rate : {12000U, 24000U, 48000U}) {
            assert(rx_rate_divisors(clock, rate, total, factor));
            assert(total == factor * 256 && uint32_t(total) * rate == clock);
        }
        assert(!rx_rate_divisors(clock, 36000, total, factor));
        assert(total == 0 && factor == 0);
    }
    for (uint32_t rate : {12000U, 24000U, 36000U})
        assert(rx_rate_divisors(110592000, rate, total, factor));
    assert(rx_rate_divisors(98304000, 24000, total, factor));
    assert(!rx_rate_divisors(98304000, 36000, total, factor));
    assert(!rx_rate_divisors(0, 12000, total, factor));
    assert(!rx_rate_divisors(61440000, 0, total, factor));
    assert(!rx_rate_divisors(61440000, UINT32_MAX, total, factor));
    assert(!rx_rate_divisors(UINT32_MAX, 1, total, factor));
    rx_metadata_v1 meta{};
    assert(!rx_metadata_has_sample_clock(meta));
    meta.version=1; meta.size=sizeof(meta); meta.frame_count=512;
    meta.ticks_per_frame=2560; meta.flags=RX_META_COUNTER_VALID;
    assert(rx_metadata_has_sample_clock(meta)); // tick zero is valid
    meta.flags |= RX_META_OVERRUN;
    assert(!rx_metadata_has_sample_clock(meta));
    meta.flags=RX_META_COUNTER_VALID; meta.size--;
    assert(!rx_metadata_has_sample_clock(meta));
    meta.size=sizeof(meta); meta.version=2;
    assert(!rx_metadata_has_sample_clock(meta));
    assert(!rx_rate_divisors(61440000, 60000, total, factor)); // factor 4
    assert(!rx_rate_divisors(122880000, 6000, total, factor)); // factor 80
    Header h{0, 7, 485935, 401000843};
    for (int i = 0; i != 1000; ++i) {
        set_unavailable_iq_timing(h, true);
        assert(h.last_gps_solution == 255 && !h.dummy && !h.gpssec && !h.gpsnsec);
        set_unavailable_iq_timing(h, false);
        assert(!h.last_gps_solution && !h.dummy && !h.gpssec && !h.gpsnsec);
    }
    puts("Rate contract and unavailable/redacted IQ timing tests passed");
}
