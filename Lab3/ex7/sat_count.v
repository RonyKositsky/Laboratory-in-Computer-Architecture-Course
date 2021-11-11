`include "../ex5/addsub.v"
module sat_count(clk, reset, branch, taken, prediction);
   parameter N=2;
   input clk, reset, branch, taken;
   output prediction;
   reg [3:0] counter;
   wire [3:0] counter_plus1;

   addsub add0(.result(counter_plus1), .operand_a(counter), .operand_b(4'b1), .mode(~taken));

    always @(posedge clk)
      begin
	   if (reset)
	      counter <= 0;
	   else begin
          if (branch)
		      if (taken)
			      if (EXPN>counter)
				      counter <= counter_plus1;
			      else
				      counter <= EXPN;
		      else
			      if (0==counter)
				      counter <= 0;
			      else
				      counter <= counter_plus1;
          else
	  	      counter <= counter;
         end
     end

    assign prediction = counter[N-1];

endmodule
