# Manufacturer FPGA design reference

Source: [FPGA Design — SDDC Lab](https://www.rx-888.com/web/design/fpga.html)
Saved: 2026-10-16. Source license: CC-BY-SA 4.0.
Local HTML snapshot: [manufacturer-fpga-design.html](manufacturer-fpga-design.html).
The snapshot retains original asset links; images are not bundled offline.
This note paraphrases the source; the HTML preserves the retrieved page.

## Design details

- Zynq-7010, Vivado block design; reported approximately 80% utilization at 122.88 MHz.
- Thirteen complex RX channels: DDS/multipliers, then three-stage CIC decimation
  by 256. The 26 I/Q paths share a five-stage multichannel CIC, decimating by 40
  for the illustrated 12 kHz configuration.
- RX data transfers through DMA using the general-purpose CPU interface.
- Two independent waterfall paths use high-performance ports and separate DMA;
  software multiplexes them across receiver channels. Each uses a five-stage CIC
  and 16-bit output, compared with 32-bit RX I/Q.
- The PPS counter measures clock cycles between pulses for frequency accuracy.

## Interpretation limits

The overview calculates RX bandwidth with 16-bit components, while its detailed
RX section describes 32-bit components. Preserve this discrepancy when estimating
traffic. These diagrams document a design, not the exact installed bitstream,
a complete RTL release, or proof of sample-to-UTC timestamp support.

See [our replacement comparison](../fpga-replacement-comparison.md) for source-level
compatibility findings and the proposed timestamp interface.
