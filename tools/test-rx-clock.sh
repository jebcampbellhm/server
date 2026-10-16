#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -I.     tests/rx_clock_contract_test.cpp -o build/rx_clock_contract_test
build/rx_clock_contract_test
${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -I. tests/rx_sample_utc_test.cpp -o build/rx_sample_utc_test
build/rx_sample_utc_test
iverilog -g2012 -Wall -s rx_sample_clock_tb -o build/rx_sample_clock_test     fpga/rx_sample_clock.v tests/rx_sample_clock_tb.v
vvp build/rx_sample_clock_test
