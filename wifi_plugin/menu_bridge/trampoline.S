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
