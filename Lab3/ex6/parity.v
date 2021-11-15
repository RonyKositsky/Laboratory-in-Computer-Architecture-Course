module parity(clk, in, reset, out);

  input clk, in, reset;
  output out;

  reg 	out;
  reg   state;

  localparam zero=0, one=1;

  always @(posedge clk)
    begin
      if (reset)
        state <= zero;
      else
        case (state)
          0: state <= in;
          1: state <= ~in;
          default: state <= 1;
        endcase
      end

  always @(state)
    begin
      case (state)
        0: out <= 0;
        1: out <= 1;
        default: out <= 1;
      endcase
    end

endmodule
