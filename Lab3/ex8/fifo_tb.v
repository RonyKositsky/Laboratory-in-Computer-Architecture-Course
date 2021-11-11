module main;
   reg clk, reset, push, pop;
   reg [2:0] in;
   wire [2:0] out;
   wire full;

   fifo uut #(4,2) (clk, reset, in, push, pop, out, full);

   always #5 clk = ~clk;

   initial begin
$dumpfile("waves.vcd");
$dumpvars;
clk = 1; fail = 0;
#10
reset = 1; in = 0; push = 0; pop = 0; //reset params
#10 if (full != 0 || out != 0) fail <= 1;

#10 reset = 0; in = 2; push = 1; pop = 0; // #1 push

#10 if (full != 0 || out != 0) fail <= 2;

#10 reset = 0; in = 0; push = 0; pop = 1; // #1 pop

#10 if (full != 0 || out != 2) fail <= 3;

#10 reset = 0; in = 3; push = 1; pop = 1; // #1 push (nothing to pop)

#10 if (full != 0 || out != 0) fail <= 4;

#10 reset = 0; in = 2; push = 1; pop = 0; // #2 push.

#10 if (full != 0 || out != 3) fail <= 5;

#10 reset = 0; in = 1; push = 1; pop = 0; // #3 push.

#10 if (full != 0 || out != 3) fail <= 6;

#10 reset = 0; in = 2; push = 1; pop = 0; // #4 push.

#10 if (full == 0 || out != 3) fail <= 7;

#10 reset = 0; in = 3; push = 1; pop = 0; // #4 push when full , not really pushed

#10 if (full != 1 || out != 3) fail <= 8;

#10 reset = 0; in = 0; push = 0; pop = 1; // #1 pop

#10 if (full != 1 || out != 3) fail <= 9;

#10 reset = 0; in = 0; push = 0; pop = 1; // #2 pop

#10 if (full != 0 || out != 2) fail <= 10;

#10 reset = 1; in = 0; push = 0; pop = 0; //reset params

#10 if (full != 0 || out != 0) fail <= 11;

#10
if(fail == 0)
	$display("PASSED ALL TESTS");
else
	$display("Failed at test %d:", fail);

$finish;
end

endmodule
