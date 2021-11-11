module fulladder( sum, co, a, b, ci);

  input   a, b, ci;
  output  sum, co;
  wire sum_ab;
  wire carry_ab;
  wire carry_bci;
  wire carry_aci;

  xor(sum_ab, a, b);
  xor(sum, ci, sum_ab);
  and(carry_ab, a, b);
  and(carry_bci, b, ci);
  and(carry_aci, a, ci);
  or(co, carry_ab, carry_bci, carry_aci);

endmodule
