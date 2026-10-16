# FPGA source starting points (2026-10-16)

## Checked-out sources

- `C:\1_HAM\red-pitaya-upstream-reference`: https://github.com/pavel-demin/red-pitaya-notes at `71c3402a015df6136bdb751e8b1531388b8e4f92`. MIT license. Includes Verilog cores, Vivado Tcl project construction, constraints, filter definitions and C receiver servers. This is the clean upstream reference, not Web-888 production RTL.
- `C:\1_HAM\Web888-fpga`: https://github.com/RaspSDR/red-pitaya-notes, restore_web888 at `140f17d9e41d6a31a8f6e6167478378c6e3f6f94`. The sdr_receiver project explicitly uses a 122.88 MHz clock, one 16-bit ADC input and a dummy second input. Its rx.tcl constructs eight receiver paths. This is a useful board-port lead; its name does not establish compatibility with installed Web-888 firmware.
- `C:\1_HAM\Web888-driver-reference`: pinned replacement zynqsdr driver described in the timestamp interface investigation. Not confirmed stock driver source.

## Files to start from

1. RaspSDR `projects/sdr_receiver/block_design.tcl`: ADC and processing-system integration.
2. RaspSDR `cfg/ports.xdc`, `cfg/clocks.xdc`, `cfg/red_pitaya.xml`: candidate pin, clock and PS/DDR configuration. Must verify against the physical Web-888 revision before loading anything.
3. Upstream `projects/sdr_receiver_122_88/rx.tcl`: receiver DSP construction; compare with the older RaspSDR variant.
4. Upstream `cores/` and receiver server directories: reusable streaming blocks and software interface examples.
5. Our `fpga/rx_sample_clock.v`: simulated sample/PPS capture prototype, still requiring atomic DMA metadata and driver integration.

## Proposed first implementation milestone

Build a separate receive-only experimental design around one DDC and sample/PPS metadata at the accepted 61.44 MS/s ADC target (the UI's 32 MHz mode). Verify board pins, ADC capture timing, PS/DDR configuration and software register ABI first. Start with an exactly representable 24 kHz output. Measure synthesis utilization and timing before increasing capture count or integrating two waterfall chains. This milestone is a development scaffold, not a proposed reduction of the final receiver features.

The current 13-RX/two-waterfall production design has not been located as a complete reproducible source tree. None of these reference bitstreams is a drop-in replacement for it. Vivado IP dependencies still apply despite the source repository's MIT license. No synthesis, place-and-route, hardware loading or resource-fit claim was made during this source search.

## Resource claims: evidence boundaries

Manufacturer documentation describes per-channel DDCs followed by CIC decimation and a shared multichannel second CIC stage: https://www.rx-888.com/web/design/fpga.html . It does not establish a 4096-point FPGA FFT filter bank. CIC filters do not require general multipliers in their integrator/comb arithmetic.

A primary issue reports DSP48 exhaustion when adapting a 7020-targeted HPSDR receiver FIR design to a 7010 TRX-DUO: https://github.com/pavel-demin/red-pitaya-notes/issues/1119 . This supports architecture-dependent resource limits, but does not verify the claimed 4096-point FCFB/eight-channel relationship. The generic Groups.io topics index is insufficient evidence for that claim.
