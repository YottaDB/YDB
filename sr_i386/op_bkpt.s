#################################################################
#								#
#	Copyright 2001, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_bkpt.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE "g_msf.si"

	.sbttl	opp_zstepret
#	PAGE	+
	.DATA
.extern	frame_pointer
.extern	zstep_level

	.text
.extern	gtm_fetch
.extern	op_retarg
.extern	op_zbreak
.extern	op_zst_break
.extern	op_zst_over
.extern	op_zstepret
.extern	opp_ret

# PUBLIC	opp_zstepret
ENTRY opp_zstepret
	movl	frame_pointer,%eax
	movw	msf_typ_off(%eax),%dx
	testw	$1,%dx
	je	l1
	movl	zstep_level,%edx
	cmpl	%eax, %edx
	jg	l1
	call	op_zstepret
l1:	jmp	opp_ret
# opp_zstepret ENDP

# PUBLIC	opp_zstepretarg
ENTRY opp_zstepretarg
	pushl	%eax
	pushl	%edx
	movl	frame_pointer,%eax
	movw	msf_typ_off(%eax),%dx
	testw	$1,%dx
	je	l2
	movl	zstep_level,%edx
	cmpl	%eax, %edx
	jg	l2
	call	op_zstepret
l2:	popl	%edx
	popl	%eax
	jmp	op_retarg
# opp_zstepretarg ENDP

# PUBLIC	op_zbfetch
ENTRY op_zbfetch
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
#	lea	esp, [esp][eax*4]
	leal	(%esp,%eax,4),%esp
	pushl	frame_pointer
	call	op_zbreak
	addl	$4,%esp
	getframe
	ret
# op_zbfetch ENDP

# PUBLIC	op_zbstart
ENTRY op_zbstart
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	pushl	%edx
	call	op_zbreak
	addl	$4,%esp
	getframe
	ret
# op_zbstart ENDP

# PUBLIC	op_zstepfetch
ENTRY op_zstepfetch
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
#	lea	esp, [esp][eax*4]
	leal	(%esp,%eax,4),%esp
	call	op_zst_break
	getframe
	ret
# op_zstepfetch ENDP

# PUBLIC	op_zstepstart
ENTRY op_zstepstart
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	op_zst_break
	getframe
	ret
# op_zstepstart ENDP

# PUBLIC	op_zstzbfetch
ENTRY op_zstzbfetch
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
#	lea	esp, [esp][eax*4]
	leal	(%esp,%eax,4),%esp
	pushl	frame_pointer
	call	op_zbreak
	addl	$4,%esp
	call	op_zst_break
	getframe
	ret
# op_zstzbfetch ENDP

# PUBLIC	op_zstzbstart
ENTRY op_zstzbstart
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	pushl	%edx
	call	op_zbreak
	addl	$4,%esp
	call	op_zst_break
	getframe
	ret
# op_zstzbstart ENDP

# PUBLIC	op_zstzb_fet_over
ENTRY op_zstzb_fet_over
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
#	lea	esp, [esp][eax*4]
	leal	(%esp,%eax,4),%esp
	pushl	frame_pointer
	call	op_zbreak
	addl	$4,%esp
	movl	zstep_level,%edx
	cmpl	frame_pointer,%edx
	jle	l3
	cmpl	$0,%eax
	jne	l5
	jmp	l4

l3:	call	op_zst_break
l4:	getframe
	ret

l5:	call	op_zst_over
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	ret
# op_zstzb_fet_over ENDP

# PUBLIC	op_zstzb_st_over
ENTRY op_zstzb_st_over
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	pushl	%edx
	call	op_zbreak
	addl	$4,%esp
	movl	zstep_level,%edx
	cmpl	frame_pointer,%edx
	jle	l6
	cmpl	$0,%eax
	jne	l8
	jmp	l7

l6:	call	op_zst_break
l7:	getframe
	ret

l8:	call	op_zst_over
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	ret
# op_zstzb_st_over ENDP

# PUBLIC	op_zst_fet_over
ENTRY op_zst_fet_over
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
#	lea	esp, [esp][eax*4]
	leal	(%esp,%eax,4),%esp
	movl	zstep_level,%edx
	cmpl	frame_pointer,%edx
	jg	l9
	call	op_zst_break
	getframe
	ret

l9:	call	op_zst_over
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	ret
# op_zst_fet_over ENDP

# PUBLIC	op_zst_st_over
ENTRY op_zst_st_over
	movl	frame_pointer,%eax
	popl	msf_mpc_off(%eax)
	movl	zstep_level,%edx
	cmpl	%eax,%edx
	jg	l10
	call	op_zst_break
	getframe
	ret

l10:	call	op_zst_over
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	ret
# op_zst_st_over ENDP

# PUBLIC	opp_zst_over_ret
ENTRY opp_zst_over_ret
	movl	frame_pointer,%eax
	movw	msf_typ_off(%eax),%dx
	testw	$1,%dx
	je	l11
	movl	zstep_level,%edx
	movl	msf_old_frame_off(%eax),%eax
	cmpl	%eax,%edx
	jg	l11
	call	op_zstepret
l11:	jmp	opp_ret
# opp_zst_over_ret ENDP

# PUBLIC	opp_zst_over_retarg
ENTRY opp_zst_over_retarg
	pushl	%eax
	pushl	%edx
	movl	frame_pointer,%eax
	movw	msf_typ_off(%eax),%dx
	testw	$1,%dx
	je	l12
	movl	zstep_level,%edx
	movl	msf_old_frame_off(%eax),%eax
	cmpl	%eax,%edx
	jg	l12
	call	op_zstepret
l12:	popl	%edx
	popl	%eax
	jmp	op_retarg
# opp_zst_over_retarg ENDP

# END
