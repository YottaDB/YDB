#################################################################
#								#
#	Copyright 2007, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_tstart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_tstart
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_tstart

# PUBLIC	opp_tstart
ENTRY opp_tstart        # /* PROC */
        putframe
        addq    $8, REG_SP # burn the return pc
        enter   $8, $0     # pull a small stack, used only for saving the incoming $rsp value.
                           # But make sure that the $rsp is 16 byte aligned
        movl    REG32_ARG2,REG32_ACCUM
        cmpl    $0,REG32_ACCUM
        je      no_arg
        cmpl    $1,REG32_ACCUM
        je      arg_1
        cmpl    $2,REG32_ACCUM
        je      arg_2
        subl    $3,REG32_ACCUM          #3 arguments are already in register
        cltq
        leaq    (REG_FRAME_POINTER,REG64_ACCUM,8),REG64_SCRATCH1
again:  pushq   (REG64_SCRATCH1)
        subq    $8,REG64_SCRATCH1
        subq    $1,REG64_ACCUM
        cmpq    $0,REG64_ACCUM
        jg      again
        pushq   REG64_ARG5
arg_2:  movq    REG64_ARG4,REG64_ARG5
arg_1:  movq    REG64_ARG3,REG64_ARG4
no_arg: movq    REG64_ARG2,REG64_ARG3
        movq    REG64_ARG1,REG64_ARG2
        movq    REG64_ARG0,REG64_ARG1
        movl    $0,REG32_ARG0		# arg0: NOT an implicit op_tstart() call
        movb    $0,REG8_ACCUM           # variable length argument
        call    op_tstart
        leave   # restore $rsp
        getframe
        ret
# opp_tstart ENDP

# END

