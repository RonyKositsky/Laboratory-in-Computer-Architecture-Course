module main;
	reg a,b,ci;
	wire sum, co;
	fulladder fulladder1(sum, co, a, b, ci);

	always@(sum or co)
		begin
			$display("time=%d: %b + %b (+%b) = %b, carry = %b\n", $time, a, b, ci, sum, co);
		end
	
	initial
		begin
			$dumpfile("waves.vcd");
			$dumpvars;
			a = 0; b = 0; ci=0;
			#5
			a = 0; b = 0; ci=1;
			#5
			a = 0; b = 1; ci=0;
			#5
			a = 0; b = 1; ci=1;
			#5
			a = 1; b = 0; ci=0;
			#5
			a = 1; b = 0; ci=1;
			#5
			a = 1; b = 1; ci=0;
			#5
			a = 1; b = 1; ci=1;
			#5
			$finish;
		end
endmodule
