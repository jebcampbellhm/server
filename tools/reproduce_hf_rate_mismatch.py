"""Reproduce the baseline HF rate arithmetic; does not contact hardware."""
from pathlib import Path
from fractions import Fraction
import subprocess

source = subprocess.check_output(["git", "show", "4dfecc4626d206e077f6f6908bd9b97ac4607cb8:main.cpp"], cwd=Path(__file__).resolve().parents[1], text=True)
assert "rx_decim = (int)(ADC_CLOCK_TYP / snd_rate);" in source
assert "fpga_rx_decim = rx_decim / 256;" in source
mismatches = []
for requested in (12000, 24000, 36000, 48000):
    software_divisor = 61440000 // requested
    hardware_divisor = (software_divisor // 256) * 256
    software_rate = Fraction(61440000, software_divisor)
    hardware_rate = Fraction(61440000, hardware_divisor)
    if software_divisor != hardware_divisor:
        mismatches.append(requested)
    print(f"requested={requested} software_divisor={software_divisor} "
          f"hardware_divisor={hardware_divisor} software_hz={float(software_rate):.6f} "
          f"implied_hardware_hz={float(hardware_rate):.6f}")
assert mismatches == [36000]
assert Fraction(61440000, 1536) == 40000
assert 61440000 % (36000 * 256) != 0
for clock, rates in ((98304000, (12000, 24000)), (110592000, (12000, 24000, 36000))):
    for rate in rates:
        assert clock % (rate * 256) == 0
print("Confirmed: baseline HF 36k divisor mismatch; other listed rate contracts agree.")
