`timescale 1ns/1ps
module rx_sample_clock_tb;
    reg clk=0, resetn=0, sample_valid=0, block_start=0, pps_edge=0;
    wire bv,pv;
    wire [63:0] bt,bi,pt,pi;
    rx_sample_clock dut(clk,resetn,sample_valid,block_start,pps_edge,bv,bt,bi,pv,pt,pi);
    always #5 clk=~clk;
    integer i;
    reg [63:0] expected_ticks=0, expected_samples=0;
    initial begin
        repeat(2) @(negedge clk);
        resetn=1;
        for(i=0;i<2000;i=i+1) begin
            sample_valid=(i%6==0); block_start=(i%24==0); pps_edge=(i%31==0);
            @(posedge clk); #1;
            if(bv !== (block_start && sample_valid) || pv !== pps_edge) $fatal(1,"valid pulse mismatch");
            if(bv && (bt !== expected_ticks || bi !== expected_samples)) $fatal(1,"block latch mismatch");
            if(pv && (pt !== expected_ticks || pi !== expected_samples)) $fatal(1,"PPS latch mismatch");
            expected_ticks=expected_ticks+1;
            if(sample_valid) expected_samples=expected_samples+1;
            @(negedge clk);
        end
        // A malformed block_start without data must not invent a sample.
        sample_valid=0; block_start=1; pps_edge=0;
        @(posedge clk); #1; if(bv) $fatal(1,"phantom block");
        @(negedge clk); resetn=0;
        @(posedge clk); #1;
        if(bv || pv || bt || bi || pt || pi) $fatal(1,"reset retained metadata");
        // Exercise carry and modulo rollover without simulating 2**64 clocks.
        @(negedge clk); resetn=1; sample_valid=1; block_start=1; pps_edge=1;
        dut.ticks=64'hffffffffffffffff; dut.samples=64'hffffffffffffffff;
        @(posedge clk); #1;
        if(bt !== 64'hffffffffffffffff || pt !== bt || bi !== bt || pi !== bi) $fatal(1,"rollover latch");
        @(negedge clk);
        @(posedge clk); #1;
        if(bt || bi || pt || pi) $fatal(1,"rollover increment");
        $display("RX sample/PPS capture, coincident edges, reset and rollover passed");
        $finish;
    end
endmodule
