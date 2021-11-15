module main;
   reg clk, reset, branch, taken;
   wire prediction;
   reg test_result_0;
   reg test_result_1;
   reg test_result_2;
   reg test_result_3;

   sat_count count(.clk(clk), .reset(reset), .branch(branch), .taken(taken), .prediction(prediction));

   always #5 clk = ~clk;

   initial
      begin
         $dumpfile("waves.vcd");
         $dumpvars;
         clk = 0;
         test_result_0 = 1;
         test_result_1 = 1;
         test_result_2 = 1;
         test_result_3 = 1;
         reset = 1;
         branch = 0;
         taken = 0;

         #10;
         reset = 0;
         test_result_0 = test_result_0 & (~prediction);
         if (test_result_0)
            $display("reset test passed!");

         #10;
         taken = 1;
         #20;
         test_result_1 = test_result_1 & (~prediction);
         if (test_result_1)
            $display("branch test passed!");

         #10;
         branch = 1;
         taken = 1;
         #10;
         test_result_2 = test_result_2 & (~prediction);
         #10;
         test_result_2 = test_result_2 & (prediction);
         #10;
         test_result_2 = test_result_2 & (prediction);
         #20;
         test_result_2 = test_result_2 & (prediction);
         if (test_result_2)
            $display("first taken test passed!");
         #10;
         branch = 1;
         taken = 0;
         #10;
         test_result_3 = test_result_3 & (prediction);
         #10;
         test_result_3 = test_result_3 & (~prediction);
         #10;
         test_result_3 = test_result_3 & (~prediction);
         #20;
         test_result_3 = test_result_3 & (~prediction);
         if (test_result_3)
            $display("second taken test passed!");

         if (test_result_0 & test_result_1 & test_result_2 & test_result_3)
            $display("PASSED ALL TESTS!");
         $finish;
      end
endmodule
