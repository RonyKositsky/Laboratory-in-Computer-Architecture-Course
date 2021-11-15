module fulladder( sum, co, a, b, ci);

  input   a, b, ci;
  output  sum, co;
  wire and_ab;
  wire and_bci;
  wire and_aci;

  xor(sum, a, b, ci);
  and(and_ab, a, b);
  and(and_bci, b, ci);
  and(and_aci, a, ci);
  or(co, and_ab, and_bci, and_aci);
endmodule
