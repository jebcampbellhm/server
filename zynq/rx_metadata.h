#pragma once
#include <stdint.h>

// Proposed ABI payload only. No ioctl number assigned and no production calls
// until the driver/FPGA can supply atomic acquisition metadata.
// Fixed-width naturally aligned layout; no userspace pointers or packed u64.
struct rx_metadata_v1 {
    uint32_t version;
    uint32_t size;
    uint64_t generation;       // driver changes on reset/restart/clock or rate change
    uint64_t first_sample;     // frame index of first returned complete I/Q frame
    uint64_t first_ticks;      // ADC tick when that output frame was produced
    uint64_t pps_ticks;        // same counter latched on identified PPS edge
    uint64_t pps_sequence;     // identifies pulse; NOT a UTC timestamp
    uint32_t frame_count;      // per channel, not total interleaved scalar values
    uint32_t ticks_per_frame;  // exact hardware decimation for this generation
    uint32_t flags;
    uint32_t reserved;
};
static_assert(sizeof(rx_metadata_v1) == 64, "metadata ABI size");

// Missing metadata must never be confused with counter value zero, which is valid.
enum { RX_META_COUNTER_VALID=1, RX_META_PPS_VALID=2, RX_META_OVERRUN=4,
       RX_META_DISCONTINUITY=8 };
inline bool rx_metadata_has_sample_clock(const rx_metadata_v1& m) {
    return m.version == 1 && m.size == sizeof(m) && m.reserved == 0
        && (m.flags & RX_META_COUNTER_VALID) && !(m.flags & ~15U)
        && !(m.flags & (RX_META_OVERRUN | RX_META_DISCONTINUITY))
        && m.frame_count != 0 && m.ticks_per_frame != 0;
}
// This validates sample-clock structure only. PPS-to-UTC association requires
// a qualified GPS message/pulse pairing, freshness and filter-delay policy.
