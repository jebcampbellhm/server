# AetherSDR firmware investigation

2026-10-16. Baseline: 4dfecc4626d206e077f6f6908bd9b97ac4607cb8.
Fork: https://github.com/jebcampbellhm/server ; upstream: RaspSDR/server.
Development/default branch: AetherSDR. Local checkout: C:/1_HAM/Web888.
No firmware deployment or receiver settings changes performed.

## Timestamp defect

- rx/data_pump.cpp:93 assigns zero to every published receive buffer tick field.
- zynq/ioctl.h RX_READ supplies address, length, and bytes-read, with no timestamp,
  sample counter, overflow generation, or PPS association in its returned structure.
- zynq/peri.cpp fpga_read_rx reads raw I/Q data. A host read-completion time is not
  a hardware first-sample timestamp; driver buffering and scheduling intervene.
- gps/gps.cpp pps_task obtains PPS_READ and calls clock_correction(t_rx,ticks).
- init/clk.cpp clock_correction stores ticks in clk.ticks, then explicitly uses
  ticks/1 second to estimate ADC frequency. This is a period measurement, not the
  original Kiwi absolute 48-bit counter assumption.
- rx/rx_sound.cpp:530-550 still subtracts clk.ticks from receive-buffer ticks as
  48-bit timestamps; filter/AGC offsets are applied at 643 and the previous buffer
  timestamp is written at 647-654. Replacing zero with an arbitrary host counter
  would not resolve this clock-domain mismatch.

Existing AetherSDR live evidence at 24 kHz: 15 seconds, 689 blocks, one startup
zero header; 672 identical timestamps among 687 comparisons, no backward steps.
Maximum step error versus 512/24000 seconds: 0.981126193 seconds. Age zero was
reported even for repeats. These are not qualified UTC anchors.

Proposed work: explicitly represent unavailable hardware sample timing; separate
frequency-correction measurements from absolute sample timing. Identify the exact
installed FPGA/driver revisions and whether a sample counter can be atomically
latched at PPS and returned alongside each RX block. Include overrun/restart
markers and counter rollover. Retain timing-unavailable status until UTC/week,
GPS timescale, FIR and AGC delay association are validated. A software-clock
fallback needs measured uncertainty and cannot silently claim GPS precision.

## HF 36 kHz defect

main.cpp:256-272 computes snd_rate=12000*(1+index). The HF path computes
rx_decim=int(61440000/snd_rate), then fpga_rx_decim=rx_decim/256. The FPGA divisor
is passed through RX_START in zynq/peri.cpp:238. Software continues to use the
unquantized rx_decim, including the advertised real rate and timestamp delays.

| Requested Hz | Software divisor | FPGA factor | Implied factor*256 | Software Hz | Implied hardware Hz |
|---|---:|---:|---:|---:|---:|
| 12000 | 5120 | 20 | 5120 | 12000 | 12000 |
| 24000 | 2560 | 10 | 2560 | 24000 | 24000 |
| 36000 | 1706 | 6 | 1536 | 36014.068 | 40000 |
| 48000 | 1280 | 5 | 1280 | 48000 | 48000 |

The 256 hardware quantum is implied by the server's own airband validation and
configuration contract; the exact installed bitstream must still be traced.
61.44 MHz cannot produce exact 36 kHz with integer factors of 256. Existing
110.592 MHz airband handling divides exactly by 12*256, but switching HF to an
airband clock is not a validated workaround (RF range, clocks, filters differ).
Changing only advertised rate leaves nominal-rate decoder and resampler consumers
inconsistent. Candidate fixes: reject unsupported HF rates, expose an honest 40k
mode consistently, or add a validated 40-to-36 kHz resampler / hardware change.
Keep 24 kHz operational while choosing the intended compatibility contract.

## Reproduction and next validation

Run python tools/reproduce_hf_rate_mismatch.py. This is an arithmetic/source-contract
reproduction, not a firmware or FPGA test. Tests compare all four HF selections
and the existing airband rates, detecting the unique HF 36k mismatch.

Next: locate/pin matching kernel driver and FPGA sources, initialize server
submodules and prepare the documented Alpine ARMv7 GCC build environment. Validate
rate changes against onboard FT8/WSPR, network IQ/PCM and filter delays, then qualify
sample-clock discontinuities, GPS loss/recovery, and restart behavior on hardware.
No production-code fix is claimed by this investigation.

## Development checkpoint — 2026-10-16

Implemented server safeguards: common exact-divisor validation for HF and airband;
unsupported rates stop startup with a specific error rather than stream at an
incorrect rate. HF 36 kHz is rejected at both 61.44 and 122.88 MHz. This does NOT
implement true 36 kHz reception. Keep 24 kHz configured when testing a future build.
No stored receiver configuration is changed by the safeguard.

Removed the invalid Kiwi sample-time calculation from the Web-888 sound path.
Untimed IQ explicitly carries solution-age 255 with zero seconds/nanoseconds;
admin redaction still zeroes all timing fields. RF/DSP sample processing and PPS
frequency correction remain independent. This prevents false timestamp claims,
but does not yet enable GPS-timed decoding.

Added a simulated FPGA sample/PPS counter prototype and proposed metadata payload;
see rx-timestamp-interface.md for the exact integration contract and source pins.
WSL is working again. GCC/ASan/UBSan and Icarus simulation passed. An Alpine ARMv7
build environment and matching production FPGA/driver source are still required;
no whole-firmware build, synthesis or hardware validation was performed. Changes
remain local; nothing deployed or pushed in this development increment.
