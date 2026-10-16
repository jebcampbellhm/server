`timescale 1ns/1ps
// Standalone prototype, not wired into the production Web-888 bitstream.
// All inputs are synchronous to adc_clk. pps_edge is a ONE-cycle pulse after
// synchronization. sample_valid means one complete multi-channel IQ frame.
// block_start must coincide with the first sample_valid of a DMA block.
// Latches describe output sample time; CIC/FIR delay still needs compensation.
module rx_sample_clock (
    input wire adc_clk, input wire resetn,
    input wire sample_valid, input wire block_start, input wire pps_edge,
    output reg block_valid, output reg [63:0] block_ticks,
    output reg [63:0] block_sample_index,
    output reg pps_valid, output reg [63:0] pps_ticks,
    output reg [63:0] pps_sample_index
);
    reg [63:0] ticks;
    reg [63:0] samples;
    always @(posedge adc_clk) begin
        if (!resetn) begin
            ticks <= 0; samples <= 0;
            block_valid <= 0; block_ticks <= 0; block_sample_index <= 0;
            pps_valid <= 0; pps_ticks <= 0; pps_sample_index <= 0;
        end else begin
            ticks <= ticks + 1'b1;
            if (sample_valid) samples <= samples + 1'b1;
            block_valid <= block_start && sample_valid;
            pps_valid <= pps_edge;
            if (block_start && sample_valid) begin
                block_ticks <= ticks;
                block_sample_index <= samples;
            end
            if (pps_edge) begin
                pps_ticks <= ticks;
                // Number of frames emitted strictly before this ADC edge.
                pps_sample_index <= samples;
            end
        end
    end
endmodule
