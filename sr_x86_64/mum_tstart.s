#################################################################
#								#
#	Copyright 2007, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.TITLE	mum_tstart.s

.include	"linkage.si"
.include	"g_msf.si"

	.sbttl	mum_tstart
	.data
.extern	frame_pointer
.extern	proc_act_type
.extern xfer_table

	.text
.extern	trans_code

ENTRY mum_tstart
	addq	$8, REG_SP	# back up over return address
	cmpw	$0, proc_act_type(REG_IP)
	je	l1
	call	trans_code
l1:	getframe
	leaq	xfer_table(REG_IP), REG_XFER_TABLE
	ret
