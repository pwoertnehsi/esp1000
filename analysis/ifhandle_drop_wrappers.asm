
/tmp/psp-wlan-plan-ifhandle.bin:     file format binary


Disassembly of section .data:

00000d24 <.data+0xd24>:
     d24:	27bdfff0 	addiu	sp,sp,-16
     d28:	001b1ac0 	sll	v1,k1,0xb
     d2c:	3c028041 	lui	v0,0x8041
     d30:	afb00000 	sw	s0,0(sp)
     d34:	00643824 	and	a3,v1,a0
     d38:	03608021 	move	s0,k1
     d3c:	afbf0004 	sw	ra,4(sp)
     d40:	34480003 	ori	t0,v0,0x3
     d44:	10800007 	beqz	a0,0xd64
     d48:	0060d821 	move	k1,v1
     d4c:	04e20006 	bltzl	a3,0xd68
     d50:	0200d821 	move	k1,s0
     d54:	10a00003 	beqz	a1,0xd64
     d58:	00651024 	and	v0,v1,a1
     d5c:	04410007 	bgez	v0,0xd7c
     d60:	00000000 	nop
     d64:	0200d821 	move	k1,s0
     d68:	8fbf0004 	lw	ra,4(sp)
     d6c:	8fb00000 	lw	s0,0(sp)
     d70:	01001021 	move	v0,t0
     d74:	03e00008 	jr	ra
     d78:	27bd0010 	addiu	sp,sp,16
     d7c:	0c000cac 	jal	0x32b0
     d80:	00000000 	nop
     d84:	08000359 	j	0xd64
     d88:	00404021 	move	t0,v0
     d8c:	27bdfff0 	addiu	sp,sp,-16
     d90:	afb00000 	sw	s0,0(sp)
     d94:	03608021 	move	s0,k1
     d98:	afbf0004 	sw	ra,4(sp)
     d9c:	0c000caa 	jal	0x32a8
     da0:	001bdac0 	sll	k1,k1,0xb
     da4:	0200d821 	move	k1,s0
     da8:	8fbf0004 	lw	ra,4(sp)
     dac:	8fb00000 	lw	s0,0(sp)
     db0:	03e00008 	jr	ra
     db4:	27bd0010 	addiu	sp,sp,16
     db8:	27bdfff0 	addiu	sp,sp,-16
     dbc:	afbf0000 	sw	ra,0(sp)
     dc0:	0c0003c6 	jal	0xf18
     dc4:	00000000 	nop
     dc8:	8fbf0000 	lw	ra,0(sp)
     dcc:	03e00008 	jr	ra
     dd0:	27bd0010 	addiu	sp,sp,16
