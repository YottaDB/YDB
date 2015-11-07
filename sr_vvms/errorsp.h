/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ERRORSP_H__
#define __ERRORSP_H__

#include <ssdef.h>
#include <errnodef.h>
#include <chfdef.h>

typedef	unsigned int	ch_ret_type;
GBLREF int4		 error_condition;

#define CONDITION_HANDLER(name) \
	ch_ret_type name(struct chf$signal_array *sig, struct chf$mech_array *mch)

#define WARNING		0x00000000
#define SUCCESS		0x00000001
#define ERROR		0x00000002
#define INFO		0x00000003
#define SEVERE		0x00000004
#define SEV_MSK		0x00000007

#define SIGNAL		sig->chf$l_sig_name
#define SEVERITY 	(SIGNAL & SEV_MSK)

#define FAC_MSK 	0x08010000
#define MSGCC_MASK	0x00FFFFF8
#define NOT_EXC 	4
#define FACILITY_MASK	(0x0FFF0000)
#define GTM_FACILITY	(0x08F60000)
#define RMS_FACILITY	(RMS$_FACILITY << 16)
#define SYSTEM_FACILITY	(SYSTEM$_FACILITY << 16)
#define IS_GTM_ERROR(X) 	((X & FACILITY_MASK) == GTM_FACILITY)
#define IS_RMS_ERROR(X) 	((X & FACILITY_MASK) == RMS_FACILITY)
#define IS_SYSTEM_ERROR(X)	((X & FACILITY_MASK) == SYSTEM_FACILITY)
/* Count of arguments the TPRETRY error will make available for tp_restart to use */
#define TPRESTART_ARG_CNT 10	/* see comment in INVOKE_RESTART macro in tp.h */

/* ***** If the system error was a hardware trap or a hardware fault the ***** */
/* ***** PC and PSL are necessary for message output. In all other cases ***** */
/* ***** the PC and PSL are removed from the signal argument list by     ***** */
/* ***** subtracting two from the argument count in sig->chf$l_sig_args. ***** */
/* ***** If the severity code held in the first 3 bits of the argument   ***** */
/* ***** sig->chf$l_sig_name is 5, 6, or 7 (reserved for digital) it is  ***** */
/* ***** a fault or trap.						 ***** */

#define ERROR_RTN		error_return_vms

#define TRAP 			(!(SIGNAL & FAC_MSK) && (SIGNAL & SEV_MSK) > NOT_EXC)

#define PRN_ERROR		{							\
					if (!(TRAP))					\
						sig->chf$l_sig_args -= 2;		\
					if (0 < sig->chf$l_sig_args)			\
						sys$putmsg(sig, 0);			\
				}
/* MUM_TSTART unwinds the current C-stack and restarts executing generated code from the top of the current M-stack */
#define MUM_TSTART              UNWIND(&mch->CHF_MCH_DEPTH, CODE_ADDRESS(mum_tstart))

#define ESTABLISH_NOJMP(x)	lib$establish(x)
#define ESTABLISH_NOUNWIND(x)	lib$establish(x)
#define ESTABLISH_RET(x,ret)	lib$establish(x)
#define ESTABLISH(x)		lib$establish(x)

#define REVERT			lib$revert()
#define CONTINUE		return SS$_CONTINUE
#define NEXTCH			return SS$_RESIGNAL
#define UNWIND(X, Y)		{ \
					int stat; \
					stat = sys$unwind((X), (Y)); \
					if ((SS$_NORMAL != stat) && (SS$_UNWINDING != stat)) \
						EXIT(stat); \
					return SS$_NORMAL; \
				}

#define START_CH		error_def(ERR_TPRETRY);								\
				DCL_THREADGBL_ACCESS;								\
														\
				SETUP_THREADGBL_ACCESS;								\
				if (SIGNAL != ERR_TPRETRY)							\
				{										\
					if (SIGNAL == SS$_UNWIND)						\
						return SS$_NORMAL;						\
					if (SIGNAL == SS$_DEBUG)						\
						return SS$_RESIGNAL;						\
					if ((SIGNAL & MSGCC_MASK) == (C$_SIGUSR1 & MSGCC_MASK))			\
					{	/* Ignore signal if no handler. Call handler as AST */		\
						/* because deferred_events() needs it that way. */  		\
						/* Ignore errors 'cause we can't handle them now anyway */	\
						if (NULL != RFPTR(gtm_sigusr1_handler))	     	 		\
							if (lib$ast_in_prog())					\
								IVFPTR(gtm_sigusr1_handler);			\
							else							\
								sys$dclast(RFPTR(gtm_sigusr1_handler), 0, 0);	\
						return SS$_CONTINUE;				       	  	\
					}									\
					error_condition = SIGNAL;						\
				}

#define MDB_START		{									\
					ESTABLISH(terminate_ch);					\
					if (cs_addrs  &&  cs_addrs->hdr  &&  cs_addrs->hdr->clustered)	\
						sys$cantim(&cs_addrs->hdr->clustered, 0);		\
				}

#define TERMINATE		{					\
					lib$sig_to_stop(sig, mch);	\
					return SS$_RESIGNAL;		\
				}

#define SUPPRESS_DUMP		(FALSE)
#define DUMP_CORE		TERMINATE

#define MUMPS_EXIT		{					\
					mumps_status = SIGNAL;		\
					GOLEVEL(0, FALSE);		\
					MUM_TSTART;			\
				}
#define PROCDIE(x)		EXIT(x)
#define EXIT(x)			sys$exit(x)

#define DUMP			(   SIGNAL == SS$_ACCVIO			\
				 || SIGNAL == SS$_ASTFLT			\
				 || SIGNAL == SS$_OPCCUS			\
				 || SIGNAL == SS$_OPCDEC			\
				 || SIGNAL == SS$_PAGRDERR			\
				 || SIGNAL == SS$_RADRMOD			\
				 || SIGNAL == SS$_ROPRAND			\
				 || SIGNAL == ERR_ASSERT			\
				 || SIGNAL == ERR_GTMASSERT			\
				 || SIGNAL == ERR_GTMASSERT2			\
				 || SIGNAL == ERR_GTMCHECK	/* BYPASSOK */	\
                                 || SIGNAL == ERR_VMSMEMORY 			\
				 || SIGNAL == ERR_STACKOFLOW )

/* true if one of above or SEVERE and GTM error or SYSTEM error */
#define DUMPABLE		( DUMP || (SEVERITY == SEVERE &&  \
					   ( IS_GTM_ERROR(SIGNAL))))

unsigned char *set_zstatus(mstr *src, struct chf$signal_array *sig, unsigned char **ctxtp, boolean_t need_rtsloc);

#define SET_ZSTATUS(ctxt)	set_zstatus(&src_line_d, sig, ctxt, TRUE)
#define MSG_OUTPUT		!(SIGNAL & 0xF0000000)


#define EXIT_HANDLER(x)		sys$dclexh(x)

#define SEND_CALLERID(callee) send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_CALLERID, 3, LEN_AND_STR((callee)), caller_id());
#define PRINT_CALLERID util_out_print(" -- generated from 0x!XL.", NOFLUSH, caller_id());
#define MAKE_MSG_WARNING(x)     ((x) & ~SEV_MSK | WARNING)
#define MAKE_MSG_SUCCESS(x)     ((x) & ~SEV_MSK | SUCCESS)
#define MAKE_MSG_ERROR(x)       ((x) & ~SEV_MSK | ERROR)
#define MAKE_MSG_INFO(x)        ((x) & ~SEV_MSK | INFO)
#define MAKE_MSG_SEVERE(x)      ((x) & ~SEV_MSK | SEVERE)
#define MAX_MSG_SIZE 		256

CONDITION_HANDLER(ccp_ch);
CONDITION_HANDLER(ccp_exi_ch);
CONDITION_HANDLER(ojch);

#endif
