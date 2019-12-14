		.text

		addiu	$a0,$0,2131
		sll	$a1,$a0,5
		srl	$a2,$a1,3
		addiu	$t0,$0,-32767
		addu  	$a1,$t0,$t0
		addiu	$t4,$t0,-32767
		and	$a0,$0,2131
		or	$t5,$a0,$t4
		slt	$a2,$a1,$a0
		andi	$t0,$0,32767
		ori 	$a1,$t0,10000
		lui	$t4,32767
		lui	$t6,0x0040
		addiu	$t6,$t6,0x1000
		sw 	$a1, 0($t6)
		lw	$s0, 0($t6)
		lui	$t7,0x0040
		addiu	$t7,$t7,0x3ff8
		addiu	$s1,$s1,-10000
		sw	$s1, 4($t7)
		lw	$s2, 4($t7)
		bne	$0,$0,Label
		beq	$0,$0,Label
		or	$t5,$a0,$t4
		or	$t5,$a0,$t4
		or	$t5,$a0,$t4
		Label:
		blez $0,Label
