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

	always @(posedge clk or reset)
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
					if (pop == 0 and push == 1)
						begin
							if (n < N)	// 0 <= n < N
								W_t <= (in << M * n) | W_t;
							else
								n <= n - 1; // decrease one for making n stay tha same

							n <= n + 1;
						end
					else if (pop == 1 and push == 0)
						begin
							if (n == 0)
								n <= n + 1; // increase one for making n stay tha same
							else	// 0 < n <= N
								begin
									out <= W_t & ((1 << M) - 1)	// out = W[n]
									W_t <= W_t >> M;
								end

							n <= n - 1;
						end
					else if (pop == 1 and push == 1)
						begin
							if (n == 0)
								begin
									W_t <= n;
									n <= n + 1;
								end
							else
								begin
									out <= W_t & ((1 << M) - 1)	// out = W[n]		
									W_t <= W_t >> M;
									W_t <= (in << M * (n - 1)) | W_t;
								end
						end

					if (n < N)
						full <= 0;
					else
						full <= 1;
				end
		end
endmodule