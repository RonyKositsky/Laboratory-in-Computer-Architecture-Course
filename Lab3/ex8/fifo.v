module fifo(clk, reset, in, push, pop, out, full);
   parameter N=4; // determines the maximum number of words in queue.
   parameter M=2; // determines the bit-width of each word, stored in the queue.

   input clk, reset, push, pop;
   input [M-1:0] in;
   output [M-1:0] out;
   output full;

   reg [(M * N - 1):0] W_t = 0;
   integer n = 0;   					// init empty queue
   reg [M-1:0] out;
   reg full;

	always @(posedge clk or posedge reset)
		begin
			if (reset == 1) 
				begin
					W_t <= 0;
					n <= 0;
					full <= 0;
					out <= 0;
				end
			else 
				begin
					if (pop == 0 && push == 1)
						begin
							if (n < N)	// 0 <= n < N
								begin
									W_t <= (in << M * n) | W_t;
									n <= n + 1;
								end
						end
					else if (pop == 1 && push == 0)
						begin
							if (n != 0)
								begin
									W_t <= (W_t >> M);
									n <= n - 1;
								end
						end
					else if (pop == 1 && push == 1)
						begin
							if (n == 0)
								begin
									W_t <= in;
									n <= n + 1;
								end
							else
								begin
									W_t <= (W_t >> M);						// pop on item
									W_t <= (in << M * (n - 1)) | W_t;	// push the new item to (n-1)
								end
						end
					
					out <= n == 0 ? 0 : W_t & ((1 << M) - 1);	// out = W[n]
					full <= n < N ? 0 : 1;
				end
		end
endmodule