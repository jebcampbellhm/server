#pragma once
#include "zynq/rx_metadata.h"
#include <stdint.h>
#include <limits>

// An external GPS adapter must prove pulse/message association and convert the
// full date to Unix UTC. Neither NMEA arrival time nor PPS frequency is enough.
struct rx_utc_pulse {
    uint64_t generation, sequence, ticks;
    int64_t utc_ns;
    uint32_t error_ns;
    bool qualified; // includes known UTC offset and unambiguous leap handling
};
struct rx_sample_utc {
    int64_t first_ns, last_ns;
    uint32_t error_ns;
};

// Prototype mapping primitive, NOT connected to legacy RX_READ. Bracketed
// interpolation only: hold a block until the following qualified PPS arrives.
// No receipt-time anchors, extrapolation, allocations or floating point.
class rx_sample_utc_mapper {
public:
    void reset() { valid_ = false; continuous_ = false; }
    bool qualify(const rx_utc_pulse& before, const rx_utc_pulse& after,
                 uint32_t mapping_error_ns, uint32_t delay_ns, bool delay_known) {
        // Refresh only with the immediately following bracket. Preserve sample
        // continuity across PPS windows; a new generation needs explicit reset.
        const bool continuing = valid_;
        const bool adjacent = !continuing ||
            (before.generation == after_.generation && before.sequence == after_.sequence
             && before.ticks == after_.ticks && before.utc_ns == after_.utc_ns
             && delay_ns == delay_);
        valid_ = false;
        const uint64_t span = after.ticks - before.ticks; // modulo counter wrap
        if (!adjacent || !before.qualified || !after.qualified || !delay_known
            || before.generation != after.generation
            || after.sequence - before.sequence != 1
            || before.utc_ns < 0 || before.utc_ns > INT64_MAX - 1000000000LL
            || after.utc_ns != before.utc_ns + 1000000000LL
            || span < 40000000 || span > 160000000
            || mapping_error_ns > 100000000 || before.error_ns > mapping_error_ns
            || after.error_ns > mapping_error_ns) { reset(); return false; }
        // mapping_error_ns must bound pulse pairing, interpolation, counter
        // quantization and pipeline-delay uncertainty, not just GPS accuracy.
        if (!continuing) continuous_ = false;
        before_ = before; after_ = after; span_ = span;
        error_ = mapping_error_ns; delay_ = delay_ns; valid_ = true;
        return true;
    }
    bool map(const rx_metadata_v1& m, rx_sample_utc& result) {
        result = {};
        const uint64_t offset = m.first_ticks - before_.ticks;
        const uint64_t extent = uint64_t(m.frame_count ? m.frame_count - 1 : 0)
                              * m.ticks_per_frame;
        const bool matching_pps = (m.pps_sequence == before_.sequence && m.pps_ticks == before_.ticks)
                              || (m.pps_sequence == after_.sequence && m.pps_ticks == after_.ticks);
        if (!valid_ || !rx_metadata_has_sample_clock(m) || !(m.flags & RX_META_PPS_VALID)
            || m.generation != before_.generation || !matching_pps
            || offset > span_ || extent > span_ - offset
            || (continuous_ && (m.first_sample != next_sample_ || m.first_ticks != next_ticks_
                                || m.ticks_per_frame != decimation_))) {
            reset(); return false;
        }
        // span <= 160M ensures the integer multiplication cannot overflow u64.
        const int64_t start = before_.utc_ns + int64_t(offset * 1000000000ULL / span_);
        const int64_t end = before_.utc_ns + int64_t((offset + extent) * 1000000000ULL / span_);
        if (start < delay_) { reset(); return false; }
        result = {start - delay_, end - delay_, error_};
        next_sample_ = m.first_sample + m.frame_count;
        next_ticks_ = m.first_ticks + uint64_t(m.frame_count) * m.ticks_per_frame;
        decimation_ = m.ticks_per_frame; continuous_ = true;
        return true;
    }
private:
    rx_utc_pulse before_{}, after_{};
    uint64_t span_{}, next_sample_{}, next_ticks_{};
    uint32_t error_{}, delay_{}, decimation_{};
    bool valid_{false}, continuous_{false};
};
