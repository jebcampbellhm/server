#include "rx/sample_utc.h"
#include <cassert>
#include <cstdio>

int main() {
    const uint64_t rate = 61440000;
    rx_utc_pulse a{9, 42, 100, 1800000000000000000LL, 100, true};
    rx_utc_pulse b{9, 43, 100 + rate, a.utc_ns + 1000000000, 100, true};
    rx_metadata_v1 m{};
    m.version=1; m.size=sizeof(m); m.generation=9;
    m.first_ticks=100; m.first_sample=0; m.frame_count=512;
    m.ticks_per_frame=2560; m.pps_ticks=100; m.pps_sequence=42;
    m.flags=RX_META_COUNTER_VALID | RX_META_PPS_VALID;
    rx_sample_utc_mapper clock; rx_sample_utc out{};
    assert(!clock.map(m,out));
    assert(clock.qualify(a,b,1000,2000000,true));
    assert(clock.map(m,out));
    assert(out.first_ns==a.utc_ns-2000000 && out.last_ns==a.utc_ns+21291666-2000000);
    m.first_sample+=512; m.first_ticks+=512*2560;
    assert(clock.map(m,out)); assert(out.first_ns==a.utc_ns+21333333-2000000);
    ++m.first_sample; assert(!clock.map(m,out)); assert(!out.first_ns);
    --m.first_sample; assert(!clock.map(m,out)); // requires requalification
    for(int i=0;i<8;++i) {
        auto bad=b;
        switch(i) {
        case 0: bad.qualified=false; break;
        case 1: ++bad.generation; break;
        case 2: ++bad.sequence; break;
        case 3: --bad.utc_ns; break;
        case 4: bad.ticks=a.ticks; break;
        case 5: bad.ticks=a.ticks+200000000; break;
        case 6: bad.error_ns=1001; break;
        case 7: bad.utc_ns=a.utc_ns+2000000000; break; // ambiguous leap/missed pulse
        }
        assert(!clock.qualify(a,bad,1000,0,true));
    }
    assert(!clock.qualify(a,b,1000,0,false));
    assert(!clock.qualify(a,b,100000001,0,true));
    for(int i=0;i<6;++i) {
        assert(clock.qualify(a,b,1000,0,true)); auto bad=m;
        switch(i) {
        case 0: bad.flags|=RX_META_OVERRUN; break;
        case 1: ++bad.generation; break;
        case 2: ++bad.pps_sequence; break;
        case 3: ++bad.pps_ticks; break;
        case 4: bad.first_ticks=b.ticks; break; // block extends past bracket
        case 5: bad.first_ticks=a.ticks-1; break;
        }
        assert(!clock.map(bad,out));
    }
    // Counter wrap is legitimate; reset/generation changes are not wrap.
    a.ticks=UINT64_MAX-100; b.ticks=a.ticks+rate;
    m.first_ticks=a.ticks; m.pps_ticks=a.ticks; m.first_sample=0;
    assert(clock.qualify(a,b,1000,0,true)); assert(clock.map(m,out));
    m.first_ticks+=512*2560; m.first_sample=512;
    assert(clock.map(m,out)); assert(out.first_ns==a.utc_ns+21333333);
    // Updating PPS brackets must not hide dropped frames between windows.
    clock.reset(); a.ticks=100; b.ticks=100+rate;
    m.first_ticks=100; m.pps_ticks=100; m.first_sample=0;
    m.frame_count=24000; // exactly one second at 24 kHz
    assert(clock.qualify(a,b,1000,0,true)); assert(clock.map(m,out));
    rx_utc_pulse c=b; ++c.sequence; c.ticks+=rate; c.utc_ns+=1000000000;
    assert(clock.qualify(b,c,1000,0,true));
    m.first_ticks=b.ticks; m.first_sample=24000;
    m.pps_ticks=b.ticks; m.pps_sequence=b.sequence;
    assert(clock.map(m,out)); assert(out.first_ns==b.utc_ns);
    rx_utc_pulse d=c; ++d.sequence; d.ticks+=rate; d.utc_ns+=1000000000;
    assert(clock.qualify(c,d,1000,0,true));
    m.first_ticks=c.ticks; m.first_sample=48001;
    m.pps_ticks=c.ticks; m.pps_sequence=c.sequence;
    assert(!clock.map(m,out));
    puts("Bracketed sample-to-UTC mapping, bounds, gaps and wrap passed");
}
