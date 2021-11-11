module fifo(clk, reset, in, push, pop, out, full);
   parameter N=4; // determines the maximum number of words in queue.
   parameter M=2; // determines the bit-width of each word, stored in the queue.

   input clk, reset, push, pop;
   input [M-1:0] in;
   output [M-1:0] out;
   output full;

   reg [M*N-1:0] W_t = 0;
   integer n = 0;   // means empty queue
   reg [M-1:0] temp_out = 0;
   integer temp_full = 0;

	always @(posedge clk or reset) begin
		if(reset == 1) begin
			W_t <= 0; n <= 0; temp_full <= 0; temp_out = 0;
		end
		else begin
			temp_out <=  ((1 << M) - 1) & W_t; // create M times '1' in LSB's and padded with 32-M '0' LSB's

			if (n == 0) begin
				if (push == 1) begin
					W_t <= (in<<(M*n)) | W_t ; n<=n+1;
				end
				else begin  // push == 0
					W_t <= W_t; n<=n;
				end
			end
			else begin
				if(n < N) begin
					if (pop == 1 && push == 0) begin
						W_t <= (W_t>>>M); n<=n-1;
					end else;
					if (pop == 0 && push == 1) begin
						W_t <= (in<<(M*n)) | W_t ; n<=n+1;
					end else;
					if (pop == 1 && push == 1) begin
						W_t <= (in<<(M*(n-1))) | (W_t>>>M); n<=n;
					end else;
					if (pop == 0 && push == 0) begin
						W_t <= W_t; n<=n;
					end else;
				end
				else begin // n >= N
					if(pop == 1 && push == 0) begin
						W_t <= (W_t>>M); n<=n-1;
					end else;
					if(pop == 1 && push == 1) begin
						W_t <= (in<<(M*(n-1))) | (W_t>>>M); n<=n;
					end else;
					if(pop == 0) begin
						W_t <= W_t; n<=n;
					end else;
				end
			end

			if(n < N)
				temp_full <= 0;
			else
				temp_full <= 1;
		end
    end

    assign out = temp_out;
    assign full = temp_full;

endmodule
