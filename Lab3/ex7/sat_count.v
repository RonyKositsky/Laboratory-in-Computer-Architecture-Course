`include "../ex5/addsub.v"
module sat_count(clk, reset, branch, taken, prediction);
	parameter N=2;
	input clk, reset, branch, taken;
	output prediction;

	reg [3:0] counter;
	reg [3:0] EXPN = {N{1`b1}};
	wire [3:0] next_counter;

	addsub add0(.result(next_counter), .operand_a(counter), .operand_b(4'b1), .mode(~taken));

	always @(posedge clk)
		begin
			if (reset)
				counter <= 0;
			else 
				begin
					if (branch and next_counter >= 0 and next_counter <= EXPN)
						counter <= next_counter;
				end
		end

	assign prediction = counter[N-1];
endmodule
