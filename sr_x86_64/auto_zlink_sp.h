/****************************************************************
 *								*
 *	Copyright 2007, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef AUTO_ZLINK_SP_INCLUDED
#define AUTO_ZLINK_SP_INCLUDED

#define XFER_BYTE_SZ    3
#define XFER_LONG_SZ    6
#define MOV_SZ    7
#define MOD_NONE_SZ 	2
#define MOD_BYTE_SZ	3
#define MOD_LONG_SZ	6

short opcode_correct(unsigned char *curr_pc, short opcode, short reg_opcode, short is_rm, short r_m);
short valid_calling_sequence(unsigned char *pc);

#define VALID_CALLING_SEQUENCE(pc) 	valid_calling_sequence(pc)
#define RTNHDR_PV_OFF(pc)	rtnhdr_off
#define LABADDR_PV_OFF(pc)	labaddr_off

#endif /* AUTO_ZLINK_SP_INCLUDED */
