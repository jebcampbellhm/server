# Proposed RX sample-clock interface (not deployed)

The prototype in fpga/rx_sample_clock.v captures a continuous 64-bit ADC tick
counter and a 64-bit output-frame index. Counters are shared across the channels
in one interleaved stream. One sample_valid represents one complete I/Q frame
across those channels, not each scalar I or Q. block_start must coincide with
sample_valid. pps_edge must be synchronized into the ADC clock domain and be a
single-cycle pulse; synchronizer latency must be characterized.

PPS and first-frame snapshots use the same counter before increment on that
edge. The PPS frame index counts frames emitted strictly before that edge.
Tick zero is valid. Counter wrap is modulo 2^64. Reset must start a new driver
generation; a reset cannot be detected from tick zero alone.

## Integration required

1. Wire first-frame block_start at the actual IQ DMA block boundary, after the
   decimation pipeline. Preserve/latch exact pipeline delay; output time is not
   the ADC input/RF sample time. The prototype itself cannot compensate it.
2. Transfer the entire metadata record through a dual-clock FIFO, paired with
   the DMA descriptor/block sequence. Do not synchronize 64 bits independently,
   sample registers later from software, or drop metadata silently on overflow.
   The prototype has pulse outputs, no backpressure: its consumer must queue
   every record. Metadata overflow invalidates the stream generation.
3. Maintain per-open driver read position and preserve first-frame metadata on
   partial reads. Never split an interleaved IQ frame. Advance first_sample and
   first_ticks by returned frames and exact decimation, not requested bytes.
4. Return metadata and actual copied sample count atomically through a NEW
   negotiated ioctl. Existing RX_READ and PPS_READ numbers/layouts remain intact.
   zynq/rx_metadata.h is a draft 64-byte payload, not a registered ioctl. No call
   is made to an unknown command on the running receiver.
5. Capability negotiation checks ABI version/size and explicit counter/PPS flags.
   Legacy drivers continue untimed IQ. Reject unknown flags, overruns and restart
   markers; invalidate held decoder periods on generation change.
6. PPS sequence IDs identify individual pulse snapshots. Pair each with a verified
   GPS UTC solution; bound freshness, handle leap seconds/week rollover, and prove
   pulse-to-message pairing. PPS counter validity alone is not UTC validity.
7. Decode only after compensating FPGA/CIC, server FIR/AGC and client FIR delays.

## Source evidence and limits

Server baseline 4dfecc4626d206e077f6f6908bd9b97ac4607cb8 exposes no per-block timing
in RX_READ. The public replacement driver in SteamedFish/web888-debian at
194dfd9e841aba11a63247b91c67eb6954d7e08a (config/kernel/zynqsdr.c) implements the
same ABI and documents reverse-engineering provenance. It writes RX_START's
factor directly to a 16-bit decimation register and enforces range 5..40. PPS_READ
returns a 32-bit FIFO value, with no RX block association. It is not established
as the exact source of the installed stock driver.

https://github.com/SteamedFish/web888-debian/blob/194dfd9e841aba11a63247b91c67eb6954d7e08a/config/kernel/zynqsdr.c
https://www.rx-888.com/web/design/

Public RaspSDR/red-pitaya-notes branches were inspected, including web888,
web888_kernel_driver and restore_web888. They contain an interval PPS counter
but did not provide the production websdr_hf/vhf design or zynqsdr implementation.
C:/1_HAM/Web888-fpga is therefore reference material, not the matching production
FPGA source. Production integration requires the exact RTL/register map/build,
or a separately validated replacement FPGA design and driver. No claim is made
that this prototype can be dropped into an existing bitstream.

## Validation

Run sh tools/test-rx-clock.sh in Linux/WSL. GCC C++11 tests with ASan/UBSan cover
clock/rate compatibility, invalid bounds, metadata version/flags and timestamp
redaction. Icarus Verilog exercises 2,000 cycles with independently spaced block
and PPS events, coincident edges, malformed block_start, reset and rollover.
These are host/simulation checks, not ARM ABI, FPGA synthesis or hardware proof.

## Accepted design target: 32 MHz HF mode (2026-10-16)

The operator accepts 32 MHz operation for an 80-10m receiver. Prioritize the
61.44 MS/s ADC configuration, retaining both interactive waterfalls. In the
current server, admin.js labels narrowband as 32/64 MHz; init/clk.cpp halves
122.88 MHz for narrowband, and peri.cpp selects websdr_vhf.bit below 100 MHz.
The 32 MHz label does not mean a 32 MS/s ADC: Nyquist is 30.72 MHz at 61.44 MS/s,
with a practical analog/digital guard band to qualify above the 10m band.

Reducing clock frequency relaxes timing and reduces switching activity; it does
not by itself release LUTs, DSP blocks or BRAM. Reusing multipliers across channels
at a faster internal clock may save resources, but requires redesign and actual
Vivado utilization/timing reports. Preserve timestamp accuracy across clock domains.
The existing bitstream selection is also different, so its utilization cannot be
inferred from the clock ratio.

Manufacturer FPGA design documentation confirms the RX path's first CIC factor
256 followed by a shared programmable CIC (originally 40), plus two dedicated
waterfall paths. It reports about 80% resource usage for the illustrated design,
not a measured result for our installed image or our prototype:
https://www.rx-888.com/web/design/fpga.html

Recommended implementation order: sample/PPS metadata and DMA pairing first;
retain exact 24k operation; then evaluate exact 48k operation as a candidate wider
capture rate (61.44M / (256*5)), before adding a rational 40k-to-36k resampler.
48k arithmetic fits the documented factor range but is not hardware-qualified or
currently exposed as a supported admin option. No receiver setting is changed.
The 32 MHz operating target does not itself make 36k an integer decimation rate.

Still required for a deployable replacement: matching ADC pin/clock constraints,
PS/AXI configuration, production DMA/register integration, Vivado IP versions and
resource/timing closure. The prototype is portable source we can develop now;
the production RTL has not been located, and we cannot patch a .bit file as RTL.


## Counter-to-UTC prototype follow-up (2026-10-16)

Added rx/sample_utc.h and tests/rx_sample_utc_test.cpp. This is a standalone,
C++11 mapping primitive, not wired to RX_READ and not deployed. Its inputs are
atomic block metadata plus two independently qualified pulse/UTC associations.
Each association carries the driver generation, pulse sequence, ADC counter,
full Unix UTC nanoseconds and an uncertainty bound. The caller must establish
GPS date/UTC offset and actual pulse pairing; setting qualified=true is not proof.

Only consecutive one-second UTC brackets are accepted. Leap ambiguity, missed
pulses, unknown delay, wrong generation or PPS association, invalid metadata,
overgrown uncertainty, overruns, and samples outside the bracket are refused.
The mapping uses integer interpolation (40..160 MHz measured counter interval),
subtracts an explicitly known pipeline delay, and carries a caller-established
worst-case error bound of at most 100 ms across the whole bracket. That bound
must include pairing, quantization, interpolation and delay uncertainty. This
engineering limit is not a decoder-tolerance claim. Counter wrap is supported;
reset requires a new generation. Continuity checks survive successive brackets.

This design requires buffering until the following PPS: no extrapolation from
the last pulse and no host-arrival timestamp substitution. A production adapter
must split blocks at bracket boundaries or buffer/map both portions using their
respective brackets, preserve metadata across reads, and bound that buffering.
It must translate the full UTC association into an explicitly negotiated wire
extension; the old modulo-week SND header alone does not carry this contract.

Validation: sh tools/test-rx-clock.sh passed under Ubuntu WSL with GCC C++11,
-Wall -Wextra -Werror and ASan/UBSan. Tests cover exact 24 kHz sample offsets,
delay correction, false PPS identity, generation change, uncertainty bounds,
gaps, counter wrap and continuity across PPS updates. Existing rate/redaction
tests and the Icarus 2,000-cycle RTL test also passed. The shell test runner was
normalized to Unix line endings. No ARM compiler, qemu-arm-static or Alpine ARM
root was found by the targeted tool checks; no ARM or hardware validation is
claimed. No receiver service/configuration/firmware was changed.

Deployment remains blocked on acquisition metadata, not on UTC arithmetic:
- Matching websdr_hf/vhf RTL, board constraints and a reproducible Vivado build.
- Paired DMA/counter/PPS metadata in a compatible driver with a negotiated ABI.
- A proven GPS-message-to-PPS association and measured hardware/server delays.
- ARM Alpine build and bounded on-device capture/timing validation.

Legacy RX_READ still contains only address, length and readed. PPS_READ returns
one 32-bit value; neither supplies the block association required above. Do not
wire the mapper to fabricated counters or enable Aether timed decoding from it
until these acquisition and qualification requirements are met.


## Replacement-design comparison

The [FPGA replacement comparison](fpga-replacement-comparison.md) pins the public
branch revisions, stock server contracts, reverse-engineered register evidence,
and remaining compatibility work. Current workload requires nine hardware RX
channels plus two physical waterfalls; thirteen RX is the desired synthesis
candidate. The public eight-channel float/FIFO receiver cannot replace the stock
integer/DMA design unchanged. DMA completion visibility and inconsistent clock
constraints must be resolved before reusing the example writer and build scripts.
