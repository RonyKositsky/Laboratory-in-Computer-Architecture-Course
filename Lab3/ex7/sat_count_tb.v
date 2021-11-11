module main;
   reg clk, reset, branch, taken;
   wire pred;
   reg right0;
   reg right1;
   reg right2;
   reg right3;

   sat_count count(.clk(clk), .reset(reset), .branch(branch), .taken(taken), .prediction(pred));

   always #5 clk = ~clk;

   initial
     begin
        $dumpfile("waves.vcd");
        $dumpvars;
        clk = 0;
	    right0 = 1;
	    right1 = 1;
	    right2 = 1;
	    right3 = 1;
	    reset = 1;
        branch = 0;
        taken = 0;

        #10;
	    reset = 0;
        right0 = right0 & (~pred);
        if (right0)
		$display("reset test passed!");

        #10;
        taken = 1;
        #20;
        right1 = right1 & (~pred);
        if (right1)
		$display("branch test passed!");

	    #10;
        branch = 1;
        taken = 1;
        #10;
        right2 = right2 & (~pred);
        #10;
        right2 = right2 & (pred);
        #10;
        right2 = right2 & (pred);
        #20;
        right2 = right2 & (pred);
        if (right2)
		$display("first taken test passed!");

        #10;
        branch = 1;
        taken = 0;
        #10;
        right3 = right3 & (pred);
        #10;
        right3 = right3 & (~pred);
        #10;
        right3 = right3 & (~pred);
        #20;
        right3 = right3 & (~pred);
        if (right3)
		$display("second taken test passed!");



	if (right0 & right1 & right2 & right3)
		$display("PASSED ALL TESTS!");
        $finish;
     end
endmodule
