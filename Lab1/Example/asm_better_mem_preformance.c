	/*
	* Beter memory preformence code.
	*/
	asm_cmd(ADD, 2, 1, 0, 15);	// 0: 	R2 = 15
	asm_cmd(ADD, 3, 1, 0, 1); 	// 1: 	R3 = 1
	asm_cmd(ADD, 4, 1, 0, 8); 	// 2: 	R4 = 8
	asm_cmd(LD,  6, 0, 2, 0); 	// 3:	R6 = mem[R2]
	asm_cmd(JEQ, 0, 3, 4, 12);	// 4: 	if (R3 = R4) goto (pc = 12)
	asm_cmd(ADD, 5, 0, 6, 0); 	// 5: 	R5 = R6
	asm_cmd(ADD, 2, 2, 1, 1); 	// 6: 	R2++
	asm_cmd(LD,  6, 0, 2, 0); 	// 7:	R6 = mem[R2]
	asm_cmd(ADD, 6, 6, 5, 0); 	// 8:	R6 += R5
	asm_cmd(ST,  0, 6, 2, 0); 	// 9: 	mem[R2] = R6
	asm_cmd(ADD, 3, 3, 1, 1); 	// 10: 	R3++
	asm_cmd(JEQ, 0, 0, 0, 4); 	// 11: 	jump to (pc = 4)
	asm_cmd(HLT, 0, 0, 0, 0); 	// 12: 	halt

	