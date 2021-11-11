`include "../ex3/fulladder.v"
module  add4( sum, co, a, b, ci);

  input   [3:0] a, b;
  input   ci;
  output  [3:0] sum;
  output  co;
  wire [2:0] carries;

  fulladder f0(sum[0], carries[0], a[0], b[0], ci);
  fulladder f1(sum[1], carries[1], a[1], b[1], carries[0]);
  fulladder f2(sum[2], carries[2], a[2], b[2], carries[1]);
  fulladder f3(sum[3], co,a[3], b[3], carries[2]);

endmodule
