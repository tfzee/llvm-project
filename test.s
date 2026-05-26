//-- Begin function ._Z9putNumberRi
._Z9putNumberRi
.LBB0_0
	sub SP, SP, 2
	mov r2, r1
	bsl r16, SP, 2
	add r1, r16, 4
	add r1, r1, 0
	bsr r1, r1, 2
	STR r1, r2
	LOD r1, r1
	bsr r2, r1, 2
	LOD r1, r2
	bsl r1, r1, 5
	STR r2, r1
	add SP, SP, 2
	ret
	.text
                                        // -- End function
//-- Begin function .main
.main
.LBB1_0
	sub SP, SP, 4
	bsl r16, SP, 2
	add r1, r16, 12
	add r1, r1, 0
	bsr r2, r1, 2
	mov r1, r0
	STR r2, r1

	bsl r16, SP, 2
	add r1, r16, 8
	add r1, r1, 0
	bsr r3, r1, 2

	bsl r16, SP, 2
	add r16, r16, 4
	LSTR r16, 0, r3

	mov r2, 32
	STR r3, r2
	cal ._Z9putNumberRi
	bsl r16, SP, 2
	add r1, r16, 4
	LLOD r1, r1, 0
	LOD r1, r1
	add SP, SP, 4
	ret
                                        // -- End function
