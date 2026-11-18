    .set noreorder
    .text
    .align 4
    .globl wifiMenuOriginal
    .ent wifiMenuOriginal
wifiMenuOriginal:
    jr $ra
    addiu $v0,$zero,-1
    nop
    nop
    .end wifiMenuOriginal

    # PSP uses the extended argument registers: a0..a3,t0..t3.
    # (gp,fn,old,next,event,error,arg) -> (old,next,event,error,arg).
    # Sony APCTL switches to the handler module's GP around every call.
    .align 4
    .globl wifiNativeInvoke
    .ent wifiNativeInvoke
wifiNativeInvoke:
    addiu $sp,$sp,-32
    sw $ra,28($sp)
    sw $gp,24($sp)
    move $gp,$a0
    move $t9,$a1
    move $a0,$a2
    move $a1,$a3
    move $a2,$t0
    move $a3,$t1
    jalr $t9
    move $t0,$t2
    lw $gp,24($sp)
    lw $ra,28($sp)
    jr $ra
    addiu $sp,$sp,32
    .end wifiNativeInvoke
