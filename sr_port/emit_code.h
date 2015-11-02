/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
int	gtm_reg(int vax_reg);

#ifdef __x86_64__
#define NUM_BUFFERRED_INSTRUCTIONS 100
#define CODE_TYPE char
#else /* ! __x86_64__ */
#define NUM_BUFFERRED_INSTRUCTIONS 25
#ifdef __ia64
#define CODE_TYPE ia64_bundle
#else
#define CODE_TYPE uint4
#endif /* __ia64 */
#endif /* __x86_64__ */
#define ASM_OUT_BUFF 	256
#define PUSH_LIST_SIZE	500

#if defined(__vms) || defined(_AIX) || defined(__hpux) || (defined(__linux__) && defined(__ia64))
#  define TRUTH_IN_REG
#elif defined(__osf__) || (defined(__linux__) && defined(__x86_64__))
#  undef TRUTH_IN_REG
#else
# error UNSUPPORTED PLATFORM
#endif

#endif
