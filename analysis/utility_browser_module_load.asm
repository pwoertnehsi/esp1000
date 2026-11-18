
/tmp/psp-utility.bin:     file format binary


Disassembly of section .data:

000028f4 <.data+0x28f4>:
    28f4:	27bdffe0 	addiu	sp,sp,-32
    28f8:	afb00010 	sw	s0,16(sp)
    28fc:	00a08021 	move	s0,a1
    2900:	afbf0014 	sw	ra,20(sp)
    2904:	8ca50000 	lw	a1,0(a1)
    2908:	10a00020 	beqz	a1,0x298c
    290c:	00e04021 	move	t0,a3
    2910:	00c03821 	move	a3,a2
    2914:	0c002202 	jal	0x8808
    2918:	03a03021 	move	a2,sp
    291c:	18400012 	blez	v0,0x2968
    2920:	00403821 	move	a3,v0
    2924:	3c040000 	lui	a0,0x0
    2928:	24830754 	addiu	v1,a0,1876
    292c:	00603021 	move	a2,v1
    2930:	00002821 	move	a1,zero
    2934:	24640004 	addiu	a0,v1,4
    2938:	8c880000 	lw	t0,0(a0)
    293c:	24a50001 	addiu	a1,a1,1
    2940:	2ca30008 	sltiu	v1,a1,8
    2944:	1100000d 	beqz	t0,0x297c
    2948:	24840008 	addiu	a0,a0,8
    294c:	1460fffa 	bnez	v1,0x2938
    2950:	24c60008 	addiu	a2,a2,8
    2954:	8e050000 	lw	a1,0(s0)
    2958:	10a00004 	beqz	a1,0x296c
    295c:	8fbf0014 	lw	ra,20(sp)
    2960:	8fa60000 	lw	a2,0(sp)
    2964:	ae060000 	sw	a2,0(s0)
    2968:	8fbf0014 	lw	ra,20(sp)
    296c:	8fb00010 	lw	s0,16(sp)
    2970:	00e01021 	move	v0,a3
    2974:	03e00008 	jr	ra
    2978:	27bd0020 	addiu	sp,sp,32
    297c:	8e030000 	lw	v1,0(s0)
    2980:	acc70004 	sw	a3,4(a2)
    2984:	08000a55 	j	0x2954
    2988:	acc30000 	sw	v1,0(a2)
    298c:	00c02821 	move	a1,a2
    2990:	0c002200 	jal	0x8800
    2994:	00e03021 	move	a2,a3
    2998:	08000a47 	j	0x291c
    299c:	00000000 	nop
