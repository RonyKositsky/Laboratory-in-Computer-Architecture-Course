`include "defines.vh"

/***********************************
 * CTL module
 **********************************/
module CTL(
	   clk,
	   reset,
	   start,
	   sram_ADDR,
	   sram_DI,
	   sram_EN,
	   sram_WE,
	   sram_DO,
	   opcode,
	   alu0,
	   alu1,
	   aluout_wire
	   );

  // inputs
  input clk;
  input reset;
  input start;
  input [31:0] sram_DO;
  input [31:0] aluout_wire;

  // outputs
  output [15:0] sram_ADDR;
  output [31:0] sram_DI;
  output 	 sram_EN;
  output 	 sram_WE;
  output [31:0] alu0;
  output [31:0] alu1;
  output [4:0]  opcode;

  // registers
  reg [31:0] 	 r2;
  reg [31:0] 	 r3;
  reg [31:0] 	 r4;
  reg [31:0] 	 r5;
  reg [31:0] 	 r6;
  reg [31:0] 	 r7;
  reg [15:0] 	 pc;
  reg [31:0] 	 inst;
  reg [4:0] 	 opcode;
  reg [2:0] 	 dst;
  reg [2:0] 	 src0;
  reg [2:0] 	 src1;
  reg [31:0] 	 alu0;
  reg [31:0] 	 alu1;
  reg [31:0] 	 aluout;
  reg [31:0] 	 immediate;
  reg [31:0] 	 cycle_counter;
  reg [2:0] 	 ctl_state;

  integer 	 verilog_trace_fp, rc;

  initial
  	begin
  		verilog_trace_fp = $fopen("verilog_trace.txt", "w");
  	end

  reg [15:0] 	sram_ADDR;
  reg [31:0] 	sram_DI;
  reg 	 			sram_EN;
  reg 	 			sram_WE;
  
  reg [2:0]    DMA_STATE;
  reg 		     DMA_write;
  reg [31:0]   DMA_src;
  reg [31:0]   DMA_dst;
  reg [31:0]   DMA_length;
  reg [31:0]   DMA_data;
   
  // synchronous instructions
  always@(posedge clk)
  	begin
  		if (reset) begin
  			// registers reset
  			r2 <= 0;
	   		r3 <= 0;
	   		r4 <= 0;
	   		r5 <= 0;
	   		r6 <= 0;
	   		r7 <= 0;
	   		pc <= 0;
	   		inst <= 0;
	   		opcode <= 0;
	   		dst <= 0;
	   		src0 <= 0;
	   		src1 <= 0;
	   		alu0 <= 0;
	   		alu1 <= 0;
	   		aluout <= 0;
	   		immediate <= 0;
	   		cycle_counter <= 0;
	   		ctl_state <= 0;
	   		DMA_STATE <= 0;
       	DMA_src <= 0;
       	DMA_dst <= 0;
       	DMA_length <= 0;
       	DMA_data <= 0;
       	DMA_write <= 0;
	   	end 
	   	else begin
	   		// generate cycle trace
	   		$fdisplay(verilog_trace_fp, "cycle %0d", cycle_counter);
	   		$fdisplay(verilog_trace_fp, "r2 %08x", r2);
	   		$fdisplay(verilog_trace_fp, "r3 %08x", r3);
	   		$fdisplay(verilog_trace_fp, "r4 %08x", r4);
	   		$fdisplay(verilog_trace_fp, "r5 %08x", r5);
	   		$fdisplay(verilog_trace_fp, "r6 %08x", r6);
	   		$fdisplay(verilog_trace_fp, "r7 %08x", r7);
	   		$fdisplay(verilog_trace_fp, "pc %08x", pc);
	   		$fdisplay(verilog_trace_fp, "inst %08x", inst);
	   		$fdisplay(verilog_trace_fp, "opcode %08x", opcode);
	   		$fdisplay(verilog_trace_fp, "dst %08x", dst);
	   		$fdisplay(verilog_trace_fp, "src0 %08x", src0);
	   		$fdisplay(verilog_trace_fp, "src1 %08x", src1);
	   		$fdisplay(verilog_trace_fp, "immediate %08x", immediate);
	   		$fdisplay(verilog_trace_fp, "alu0 %08x", alu0);
	   		$fdisplay(verilog_trace_fp, "alu1 %08x", alu1);
	   		$fdisplay(verilog_trace_fp, "aluout %08x", aluout);
	   		$fdisplay(verilog_trace_fp, "cycle_counter %08x", cycle_counter);
	   		$fdisplay(verilog_trace_fp, "ctl_state %08x\n", ctl_state);

	   		cycle_counter <= cycle_counter + 1;
	   		case (ctl_state)
	   			`CTL_STATE_IDLE:
	   				begin
	   					pc <= 0;
	   					if (start)
	   						ctl_state <= `CTL_STATE_FETCH0;
	   				end
	   			`CTL_STATE_FETCH0: 
	   				begin
	   					ctl_state <= `CTL_STATE_FETCH1;
	   				end
	   			`CTL_STATE_FETCH1: 
	   				begin
	   					inst <= sram_DO;
	   					ctl_state <= `CTL_STATE_DEC0;
	   				end
	   			`CTL_STATE_DEC0: 
	   				begin
	   					opcode <= inst[29:25];
	   					dst <= inst[24:22];
							src0 <= inst[21:19];
							src1 <= inst[18:16];
							if (inst[15])
								immediate <= inst|((~(32'b0))<<16);
							else
								immediate <= inst[15:0];
								ctl_state <= `CTL_STATE_DEC1;
						end
					`CTL_STATE_DEC1: 
						begin
							if(src0==0)
								alu0 <= 0;
							if(src0==1)
								alu0 <= immediate;
							if(src0==2)
								alu0 <= r2;
							if(src0==3)
								alu0 <= r3;
							if(src0==4)
								alu0 <= r4;
							if(src0==5)
								alu0 <= r5;
							if(src0==6)
								alu0 <= r6;
							if(src0==7)
								alu0 <= r7;
							if(src1==0)
								alu1 <= 0;
							if(src1==1)
								alu1 <= immediate;
							if(src1==2)
								alu1 <= r2;
							if(src1==3)
								alu1 <= r3;
							if(src1==4)
								alu1 <= r4;
							if(src1==5)
								alu1 <= r5;
							if(src1==6)
								alu1 <= r6;
							if(src1==7)
								alu1 <= r7;
							ctl_state <= `CTL_STATE_EXEC0;
						end
					`CTL_STATE_EXEC0: 
						begin
							if (opcode!=`LD && opcode!=`ST && opcode!=`HLT)
								aluout <= aluout_wire;
							ctl_state <= `CTL_STATE_EXEC1;
						end
					`CTL_STATE_EXEC1: 
						begin
							pc <= pc+1;
							case (opcode)
								`LD: 
									begin 
										if(dst==2)
											r2 <= sram_DO;
										if(dst==3)
											r3 <= sram_DO;	
										if(dst==4)
											r4 <= sram_DO;				  			     		
										if(dst==5)
											r5 <= sram_DO;	
										if(dst==6)
											r6 <= sram_DO;	
										if(dst==7)
											r7 <= sram_DO;
									end
								`JEQ,`JLE,`JNE,`JLT,`JIN: 
									begin
										if (aluout == 1)
											begin
												r7 <= pc;
												pc <= immediate;
											end
									end
								`ADD,`SUB,`AND,`OR,`XOR,`LSF,`RSF,`LHI: 
									begin
										if(dst==2)
											r2 <= aluout_wire;
										if(dst==3)
											r3 <= aluout_wire;
										if(dst==4)
											r4 <= aluout_wire;
										if(dst==5)
											r5 <= aluout_wire;	
										if(dst==6)
											r6 <= aluout_wire;	
										if(dst==7)
											r7 <= aluout_wire;		
									end
								`CPY: 
									begin 
										if (DMA_STATE == `DMA_STATE_IDLE) begin 
											DMA_src <= alu0;
											DMA_length <= alu1;
											if(dst == 0)
												DMA_dst <= 0;
											else if(dst == 1)
												DMA_dst <= immediate;
											else if(dst == 2)
												DMA_dst <= r2;
											else if(dst == 3)
												DMA_dst <= r3;
											else if(dst == 4)
												DMA_dst <= r4;
											else if(dst == 5)
												DMA_dst <= r5;
											else if(dst == 6)
												DMA_dst <= r6;
											else if(dst == 7)
												DMA_dst <= r7;
											DMA_STATE <= `DMA_STATE_FETCH0;
										end
									end
								`ASK: 
									begin 
										if (dst==2)
											r2 <= DMA_length;
										if (dst==3)
											r3 <= DMA_length;	
										if (dst==4)
											r4 <= DMA_length;				  			     		
										if (dst==5)
											r5 <= DMA_length;	
										if (dst==6)
											r6 <= DMA_length;	
										if (dst==7)
											r7 <= DMA_length;		
									end
							endcase
							if (opcode == `HLT) 
								begin
								if (DMA_STATE != `DMA_STATE_IDLE)
									pc <= pc - 1;
								else
									begin
										ctl_state <= `CTL_STATE_IDLE;
										$fclose(verilog_trace_fp);
										$writememh("verilog_sram_out.txt", top.SP.SRAM.mem);
										$finish;
									end
								end
							else
									ctl_state <= `CTL_STATE_FETCH0;
						end
				endcase
				
				case (DMA_STATE) 
				   `DMA_STATE_FETCH0: 
							DMA_STATE <= `DMA_STATE_FETCH1;
				   `DMA_STATE_FETCH1:
							DMA_STATE <= `DMA_STATE_DEC0;
				   `DMA_STATE_DEC0: 
				    	begin
								DMA_data <= sram_DO;
								DMA_STATE <= `DMA_STATE_DEC1;
							end
				   `DMA_STATE_DEC1: 	
							DMA_STATE <= `DMA_STATE_EXEC0;
					`DMA_STATE_EXEC0: 
							DMA_STATE <= `DMA_STATE_EXEC1;
					`DMA_STATE_EXEC1: 
						begin
							if (DMA_length != 0)
								DMA_length <= DMA_length - 1;
							DMA_src <= DMA_src + 1;
							DMA_dst <= DMA_dst + 1;
							if (DMA_length > 0) 
								DMA_STATE <= `DMA_STATE_FETCH0;
							else 	
								DMA_STATE <= `DMA_STATE_IDLE;
						end
				endcase
				
			end // !reset
    end // @posedge(clk)

  always@(ctl_state or sram_ADDR or sram_DI or sram_EN or sram_WE or DMA_STATE)
		begin
			sram_ADDR = 0;
			sram_DI = 0;
			sram_EN = 0;
			sram_WE = 0;
			case (ctl_state)
				`CTL_STATE_FETCH0: 
					begin
						sram_ADDR = pc[15:0];
						sram_DI = 0;
						sram_EN = 1;
						sram_WE = 0;
					end
				`CTL_STATE_EXEC0: 
					begin
						if (opcode == `LD)
							begin
								sram_ADDR = alu1[15:0];
								sram_DI = 0;
								sram_EN = 1;
								sram_WE = 0;
							end
					end
				`CTL_STATE_EXEC1: 
					begin
						if (opcode == `ST)
							begin
								sram_ADDR = alu1[15:0];
								sram_DI = alu0;
								sram_EN = 1;
								sram_WE = 1;
							end
					end
				default: 
					begin
						sram_ADDR = 0;
						sram_DI = 0;
						sram_EN = 0;
						sram_WE = 0;
					end
			endcase
			
			if (DMA_write == 0) 
				begin
					case (DMA_STATE)
					`DMA_STATE_FETCH1: 
						begin
							sram_ADDR = DMA_src[15:0];
							sram_DI = 0;
							sram_EN = 1;
							sram_WE = 0;
							DMA_write = 1;
						end
					endcase
				end
			else 
				begin
					case (DMA_STATE)
						`DMA_STATE_DEC1: 
						begin
							sram_ADDR = DMA_dst[15:0];
							sram_DI = DMA_data;
							sram_EN = 1;
							sram_WE = 1;
							DMA_write = 0;
						end
					endcase
				end
			
			end
endmodule // CTL
