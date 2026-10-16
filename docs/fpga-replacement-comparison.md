# Web-888 timestamp-capable FPGA replacement: source comparison

Local engineering review, 2026-10-16. No bitstream was built or deployed.
Receiver remains on its existing firmware and 24 kHz setting.

## Conclusion and target

The public Web-888 Red Pitaya adaptations provide reusable board/ADC, NCO,
filter, AXI and memory-writer examples. They are not a drop-in implementation of
stock websdr_hf.bit or websdr_vhf.bit. Build a separately identified replacement
with a matched driver, retaining the server's receive semantics and adding a
negotiated acquisition-metadata interface. Do not rename an example bitstream
and load it as the stock design.

Requirements for the user's present workload:

- Two independent physical waterfall engines, with independent tune/zoom/reset.
- At least NINE simultaneous 24 kHz hardware RX channels: seven captures for
  40/30/20/10m FT8+WSPR plus two captures hosting four software SSB listeners.
- Parameterize RX count; retain 13 as the desired target, subject to synthesis.
  Eight is a bring-up option only, not equivalent capacity. Additional onboard
  jobs consume the remaining RX slots; do not promise arbitrary simultaneous jobs.
- First qualify 61.44 MS/s ADC / 30.72 MHz Nyquist operation, with actual useful
  coverage through 10m verified separately. Internal processing clock need not
  equal ADC sample rate. Reduced ADC clock alone does not save logic resources.
- Preserve 24 kHz: total integer decimation 2560 = 256 x 10 at 61.44 MS/s.
  Existing 122.88 MS/s requires 256 x 20. Do not fold the 36 kHz issue into bring-up.

## Source identity and evidence strength

| Checkout/source | Inspected revision | Role and limitation |
|---|---|---|
| RaspSDR/red-pitaya-notes restore_web888 | 140f17d9e41d6a31a8f6e6167478378c6e3f6f94 | Local Web888-fpga checkout; alternative projects, not a stock build |
| web888 | 0d269026992284e71f24b8969d9d5a643741c159 | Earlier board adaptation |
| web888_kernel_driver | 7ecabaf70343079c1c682c7cfc4e20037b9bd0da | GPIO driver, not stock IQ DMA driver |
| web888_gnuradio_dma | aab3c19b3aecd1128bc7cbab2ae9acaa559fe13b | Alternative wide receiver ACP writer |
| web888_sdr_receiver_wide_0724 | 56637c9ad5d0f2e9e7b244b652f639d4df57897d | Alternative wide receiver layout |
| web888_sdr_receiver_ft8_0724 | 0d13d499b1866cff88e298dc655291240845db98 | Mode-specific receiver pipeline |
| web888_sdr_receiver_wspr_0724 | 41fb62ea05e91f1a8da702ebb0c193697bd5b782 | Mode-specific receiver pipeline |
| Web888-driver-reference | 194dfd9e841aba11a63247b91c67eb6954d7e08a | SteamedFish/web888-debian replacement driver, with reverse-engineered stock behavior; NOT manufacturer source |

Stock-server evidence comes directly from C:/1_HAM/Web888/zynq/ioctl.h,
zynq/peri.cpp, rx/data_pump.cpp, rx/rx_sound.cpp and rx/rx_waterfall.cpp.
The register addresses below come from the replacement-driver source and its
port-notes.md; they require independent hardware confirmation. Do not upgrade
that provenance into a claim that we possess the stock driver's source.

## Interface comparison

| Interface | Stock server/driver contract | Public alternatives | Replacement action |
|---|---|---|---|
| Board/ADC | 16-bit ADC; Si5351 configured by server | cfg/ports.xdc, cfg/red_pitaya.xml, axis_red_pitaya_adc.v; current receiver clock Tcl specifies 122.88 MHz | Reuse as candidates; verify board revision, PS DDR/MIO preset, ADC coding/polarity and timing constraints |
| ADC conversion | Integer data later scaled by data_pump | ADC core registers data and complements lower ADC bits; dummy second ADC input | Prove input coding with test vectors; do not assume raw unsigned/two's-complement equivalence |
| RX count | Signature low nibble; runtime channel count; frame-major I0,Q0,I1,Q1... | sdr_receiver/rx.tcl instantiates eight complex channels (16 scalar filter paths) | Parameterized 9 minimum / 13 target, preserving ordering and equal frame cadence |
| NCO | Server calculates 48-bit phase increment, passes u64 via RX_PARAM/WF_PARAM | Receiver DDS PHASE_WIDTH=30, 32-bit config words | Implement 48-bit phase or explicitly convert in replacement driver; never copy raw words unchanged |
| RX filtering | Server rate contract expects fixed 256 followed by programmable factor 5..40 | restore receiver: six-stage programmable CIC plus decimate-by-2 FIR; mode-specific branches have different cascades | Design rate/scale/delay explicitly; preserve stock arithmetic contract or adapt server behind negotiated capability |
| RX sample format | Signed 32-bit I and signed 32-bit Q in DDR; SND's 16-bit wire IQ is a later conversion | Receiver ends in fixed-to-float conversion and AXI FIFO | Retain integer pipeline, prove scaling/clipping and sideband polarity; float words cannot be reinterpreted as stock integer IQ |
| RX transport | Driver-managed coherent DDR ring; userspace RX_READ copies bytes | Eight-channel receiver reads AXI hub FIFO; wide example has axis_ram_writer to PS S_AXI_ACP | FIFO example isn't stock DMA. Implement matched rings and completion/overrun protocol |
| Waterfalls | WF_START, WF_PARAM, WF_READ; two independently leased physical engines; server computes FFT | Inspected receiver examples do not supply the stock pair of WF engines | Add TWO separate tunable decimation/capture paths, signed 16-bit I/Q output, separate rings/status/reset |
| PPS | PPS_READ is period count; no sample association | axis_pps_counter resets every PPS and emits interval count | Keep frequency correction if desired; add free-running ADC tick, identified PPS latch and per-block sample index |
| Kernel branch | /dev/zynqsdr supports RX/WF/PPS ioctls | patches/web888_kernel.c exposes web888_sdr GPIO/attenuator control | Not a replacement acquisition driver; reuse only reviewed board-control concepts |

The alternative FT8/WSPR branches demonstrate eight-channel DSP arrangements,
not plug-in stock decoder engines or a stock dual-waterfall design. FPGA-side
FT8/WSPR-specific decimation should not replace the general 24 kHz capture
contract used by Aether's monitoring allocator.

## Register compatibility target (reverse-engineered reference)

Config base 0x40000000 (0xa0 bytes), status base 0x41000000 (0x18 bytes):

| Field | Offset / width |
|---|---|
| Reset/run bits | config +0x00, u16: RX bit0, WF bits1/2, PPS bit7 |
| RX programmable decimation | +0x02, u16 |
| RX phase increments | +0x04 + 8*channel, u64 slots |
| WF phase/decimation | +0x6c + 12*channel: u64 phase, u16 decimation at +8 |
| GPIO mask | +0x84, u32 |
| RX ring DMA address | +0x8c, u32 |
| RX start bytes | +0x90/+0x91, 0xf7/0x0f in reference driver |
| WF ring addresses | +0x92 + 6*channel, u32; start bytes at +4/+5 |
| WF continuous mask | +0x9e, u16 |
| Channel signature | status +0x00, u32; RX low byte / WF next byte in reference driver |
| RX producer position | status +0x04, u16, 128-byte units |
| WF producer positions | +0x06 + 2*channel, u16, 128-byte units |
| PPS FIFO count | +0x0a, u16 |
| Device DNA | +0x0c, u64 |

Thirteen RX slots end at config +0x6b: extending that layout to 14 would collide
with the WF registers. Keep legacy layout bounded even though server arrays
have space for 16 channels. Reserve an independently versioned metadata region;
do not append fields into existing register gaps without checking all consumers.

The stock ABI has packed user structures; retain existing ioctl numbers and
readed byte-count semantics. Add capability query + atomic RX-with-metadata read
under new negotiated commands, not a silent layout change to RX_READ. Frame-
aligned partial reads must advance BOTH data and first-sample metadata.

## Concrete traps found

1. Candidate axis_ram_writer publishes sts_data from int_addr_reg, which advances
   on address acceptance; m_axi_bvalid is not consumed for completion and there
   is no response-error input. That is NOT a proven DMA commit pointer. Before
   reuse, publish data/metadata only after corresponding writes complete, handle
   bus errors, and test AW/W/B backpressure independently. Timestamp the sample
   at acquisition, not at delayed AXI acceptance. Its FIFO writes on s_axis_tvalid;
   upstream must honor ready or explicitly signal overrun on an unstallable ADC.
2. Stock peri.cpp constructs data = wf_chan | WF_READ_CONTINUES but passes only
   wf_chan to WF_START. Reference driver notes also describe divergent stock
   continuous-mode behavior. Specify and test continuous versus one-shot behavior
   for the replacement; do not copy this ambiguity or change the running firmware
   on the strength of source inspection alone.

3. cfg/clocks.xdc creates an 8.000 ns clock (125 MHz), while the inspected
   receiver block Tcl specifies 122.88 MHz. At 61.44 MHz neither is the intended
   ADC period (about 16.276 ns). Generate consistent clock/IP constraints for the
   chosen architecture, derive input delays from actual ADC/board timing, and run
   constraint checks; do not merely change a GUI clock setting.

Additionally, cfg/ports.xdc comments label antenna_o[6] PGA and [7] Dith, whereas
server GPIO semantics assign dither bit6/PGA bit7. Verify the full top-level wiring
and physical schematic before adopting pin mapping; comments alone are not proof.

## Proposed architecture and timing boundary

ADC -> shared free-running tick domain -> N independently tunable RX DDCs ->
resource-shared decimation -> integer interleaved frame packer -> RX DMA ring.
ADC also feeds two independently tuned WF decimators -> two separate WF rings.
PPS synchronization -> pulse ID + same-domain tick snapshot -> metadata FIFO.
First RX frame of each acquisition block -> frame index + tick + generation ->
metadata FIFO paired with the exact data block descriptor.

The metadata queue crosses clock domains atomically, never by separately
synchronizing 64-bit registers. Overflow, rate/reset change or loss of pairing
revokes timing and starts a new generation. Blocks and timestamps are exposed
only after DMA completion; a monotonic produced/consumed sequence detects lapping.
Preserve independent WF activity when RX timing is invalid and vice versa.

The existing rx_sample_clock.v prototype and rx_metadata_v1 draft are useful
building blocks, not a complete AXI/driver design. rx/sample_utc.h additionally
requires qualified full-date UTC-to-PPS pairing and known delay. Its bracketed
mapping may wait up to a PPS interval; continuously drain DMA into a bounded
software buffer rather than leaving data in the hardware ring for that wait.

At 24 kHz, RX DDR payload is N * 8 * 24000 bytes/s: 1.728 MB/s for nine channels,
2.496 MB/s for thirteen. A 0x7fc00-byte ring holds only about 303 / 210 ms,
respectively. These are DDR payload figures, not Ethernet traffic. Two hypothetical
continuous full-rate 61.44 MS/s, 16-bit-I/Q waterfall outputs would add 491.52 MB/s;
actual bursts/decimation must be profiled. Keep independent HP-port paths where
appropriate and measure contention; do not infer throughput from nominal AXI width.

## Build and acceptance sequence

1. Freeze source SHAs, board revision/pins, PS preset and supported Vivado/IP version.
   Makefile targets xc7z010clg400-1; build Tcl imports cfg/red_pitaya.xml. Audit these
   together rather than trusting a Red Pitaya filename. Vivado was not found on the
   checked WSL PATH; no synthesis/utilization/timing report exists for this proposal.
2. Create an isolated replacement project. First simulate one RX plus both WF
   controls and the data/metadata writer. Never introduce TX/DAC outputs.
3. Scale to 9 RX + 2 WF, then evaluate 13 RX + 2 WF. Generate post-route LUT/DSP/
   BRAM/slack reports; retain channel count only when timing/resource checks pass.
   Clock reduction or known eight-channel examples do not establish that 13 fits.
4. Simulate register widths, 48-bit tuning, rates, IQ ordering, rounding, independent
   WF resets, partial reads, descriptor wrap, metadata overflow and AXI stalls/errors.
5. Build matched ARM Alpine driver/server. Negotiate metadata; keep old clients
   usable as untimed streams. Prove continuous/one-shot WF behavior explicitly.
6. Prepare a separately identified recovery-safe SD image and rollback procedure.
   Only then seek deployment approval for that concrete artifact. Qualify device
   identity, both waterfalls and all RX channels BEFORE enabling timing consumers.
7. With GPS/known reference, measure PPS pairing, group delay and clock errors;
   verify timestamp progression and uncertainty through retune/reset/gaps. Feed
   qualified anchors into Aether; only then enable timed decoders and known-message
   tests. No reporting is enabled as part of this project.

This comparison is source-level evidence. No claim of a buildable replacement,
hardware timing qualification or preserved production resource usage is made.


## Reproduce this review

Use git show <SHA>:<path> in C:/1_HAM/Web888-fpga for the pinned branch snapshots.
Primary alternative paths: projects/sdr_receiver/{block_design,rx}.tcl,
projects/sdr_receiver_wide/block_design.tcl, projects/sdr_transceiver_ft8/rx.tcl,
projects/sdr_transceiver_wspr/rx.tcl, cores/{axis_ram_writer,axis_pps_counter,
axis_red_pitaya_adc,axi_hub}.v, cfg/{ports,clocks}.xdc and cfg/red_pitaya.xml.
For web888_kernel_driver inspect patches/web888_kernel.c. No working-tree branch
was changed during this review; extra public branch objects were fetched only.
Comparison and arithmetic were reviewed against these sources; no simulation or
synthesis was performed for the proposed combined architecture.


## Saved manufacturer reference

[Manufacturer FPGA design notes and local HTML snapshot](references/manufacturer-fpga-design.md)
were saved on 2026-10-16 from https://www.rx-888.com/web/design/fpga.html.
