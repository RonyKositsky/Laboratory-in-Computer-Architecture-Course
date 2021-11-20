module main;
   reg clk, reset, push, pop;
   reg [1:0] in;
   wire [1:0] out;
   wire full;
   integer fail;

   fifo #(4,2) uut (clk, reset, in, push, pop, out, full);

   always #5 clk = ~clk;

   initial 
      begin
         $dumpfile("waves.vcd");
         $dumpvars;
         clk = 0; fail = 0;
         // reset test
         reset = 1; in = 0; push = 0; pop = 0; 
         #10
         $display("full - %d, out - %d", full, out);
         if (full == 0 && out == 0)
            $display("reset passed!");
         else fail = 1;

         // push (n = 1)
         reset = 0; in = 2; push = 1; pop = 0; 
         #10
         if (full == 0 && out == 0)
            $display("push (n = 1) passed!");
         else fail = fail | 2;

         // pop (n = 0)
         reset = 0; in = 0; push = 0; pop = 1; 
         #10
         if (full == 0 && out == 2)
            $display("pop (n = 0) passed!");
         else fail = fail | 4;

         // push && pop (n = 1)
         reset = 0; in = 3; push = 1; pop = 1;
         #10
         if (full == 0 && out == 0)
            $display("push && pop (n = 1) passed!");
         else fail = fail | 8;

         // push (n = 2)
         reset = 0; in = 2; push = 1; pop = 0;
         #10
         if (full == 0 && out == 3)
            $display("push (n = 2) passed!");
         else fail = fail | 16;

         // push (n = 3)
         reset = 0; in = 1; push = 1; pop = 0; 
         #10
         if (full == 0 && out == 3)
            $display("push (n = 3) passed!");
         else fail = fail | 32;

         // push (n = 4)
         reset = 0; in = 2; push = 1; pop = 0;
         #10
         if (full == 0 && out == 3)
            $display("push (n = 4) passed!");
         else fail = fail | 64;

         // push (n = 4) queue is full.
         reset = 0; in = 3; push = 1; pop = 0;
         #10
         if (full == 1 && out == 3)
            $display("push (n = 4) queue is full passed!");
         else fail = fail | 128;

         // pop (n = 3)
         reset = 0; in = 0; push = 0; pop = 1;
         #10
         if (full == 1 && out == 3)
            $display("pop (n = 3) passed!");
         else fail = fail | 256;

         // pop (n = 2)
         reset = 0; in = 0; push = 0; pop = 1;
         #10
         if (full == 0 && out == 2)
            $display("pop (n = 2) passed!");
         else fail = fail | 512;

         // push && pop (n = 2)
         reset = 0; in = 3; push = 1; pop = 1;
         #10
         if (full == 0 && out == 1)
            $display("push && pop (n = 2) passed!");
         else fail = fail | 1024;

         // reset queue
         reset = 1; in = 0; push = 0; pop = 0; //reset params
         #10
         if (full == 0 && out == 0)
            $display("reset queue passed!");
         else fail = fail | 2048;

         if(fail == 0)
         	$display("PASSED ALL TESTS");
         else
         	$display("Failed at test %d:", fail);

         $finish;
      end
endmodule
