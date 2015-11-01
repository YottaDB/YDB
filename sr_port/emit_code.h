/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef EMIT_CODE_INCLUDED
#define EMIT_CODE_INCLUDED

#include "emit_code_sp.h"

#ifdef DEBUG
void	emit_asmlist(triple *ct);
void	emit_eoi(void);
#endif
void    trip_gen(triple *ct);
short	*emit_vax_inst(short *inst, oprtype **fst_opr, oprtype **lst_opr);
void	emit_jmp(uint4 branchop, short **instp, int reg);
void	emit_pcrel(void);
void	emit_trip(oprtype *opr, boolean_t val_output, uint4 alpha_inst, int ra_reg);
void	emit_push(int reg);
void	emit_pop(int count);
void	add_to_vax_push_list(int pushes_seen);
int	next_vax_push_list(void);
void	push_list_init(void);
void    reset_push_list_ptr(void);
void	emit_call_xfer(int xfer);

int	get_arg_reg(void);
int     get_bytes(void);
int	gtm_reg(int vax_reg);

#define NUM_BUFFERRED_INSTRUCTIONS 25

#if defined(__vms) || defined(_AIX)
#  define TRUTH_IN_REG
#elif defined(__osf__)
#  undef TRUTH_IN_REG
#else
# error UNSUPPORTED PLATFORM
#endif

#endif
