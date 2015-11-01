/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* For the various Unix/Posix flavors, extract and fill in the appropriate information */

#include "mdef.h"

#include <sys/types.h>
#ifndef __MVS__
#  include <sys/param.h>
#endif
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "gtmsiginfo.h"

/* OS/390 (R7) does not define SI_USER but for code expansions purposes, define the value it uses
   in its place */
#if !defined(SI_USER) && defined(__MVS__)
#  define SI_USER 0
#endif

#if defined(__osf__) || defined(_AIX)
void extract_signal_info(int sig, siginfo_t *info, struct sigcontext *context, gtmsiginfo_t *gtmsi)
#else
void extract_signal_info(int sig, siginfo_t *info, ucontext_t *context, gtmsiginfo_t *gtmsi)
#endif
{
	error_def(ERR_SIGILLOPC);
	error_def(ERR_SIGILLOPN);
	error_def(ERR_SIGILLADR);
	error_def(ERR_SIGILLTRP);
	error_def(ERR_SIGPRVOPC);
	error_def(ERR_SIGPRVREG);
	error_def(ERR_SIGCOPROC);
	error_def(ERR_SIGBADSTK);
	error_def(ERR_SIGADRALN);
	error_def(ERR_SIGADRERR);
	error_def(ERR_SIGOBJERR);
	error_def(ERR_SIGINTDIV);
	error_def(ERR_SIGINTOVF);
	error_def(ERR_SIGFLTDIV);
	error_def(ERR_SIGFLTOVF);
	error_def(ERR_SIGFLTUND);
	error_def(ERR_SIGFLTRES);
	error_def(ERR_SIGFLTINV);
	error_def(ERR_SIGMAPERR);
	error_def(ERR_SIGACCERR);

	memset(gtmsi, 0, sizeof(*gtmsi));
	gtmsi->signal = sig;
#if defined(__osf__) || defined (__hpux) || defined(__sparc) || defined(__MVS__)
	if (NULL != info)
	{
		switch(info->si_code)
		{
			case SI_USER:
				gtmsi->send_pid = info->si_pid;
				gtmsi->send_uid = info->si_uid;
				gtmsi->infotype |= GTMSIGINFO_USER;
				break;
			default:
#  if defined(__osf__)
				gtmsi->subcode = info->si_code;
				gtmsi->bad_vadr = info->si_addr;
				gtmsi->infotype |= GTMSIGINFO_BADR;
				if (NULL != context)
				{
					gtmsi->int_iadr = (caddr_t)context->sc_pc;
					gtmsi->infotype |= GTMSIGINFO_ILOC;
				}
				break;
#  elif defined(__hpux)
				gtmsi->subcode = info->si_code;
				gtmsi->bad_vadr = info->si_addr;
				gtmsi->infotype |= GTMSIGINFO_BADR;
				if (NULL != context)
				{
					if (0 == (context->uc_mcontext.ss_flags & SS_NARROWISINVALID))
					{
						/* Interrupt location is in narrow area */
						gtmsi->int_iadr = (caddr_t)(context->uc_mcontext.ss_narrow.ss_pcoq_head & ~3);
					} else
					{
						/* Interupt location is in wide area */
						gtmsi->int_iadr =
							(caddr_t)(context->uc_mcontext.ss_wide.ss_32.ss_pcoq_head_hi & ~3);
					}
					gtmsi->infotype |= GTMSIGINFO_ILOC;
				}
				break;
#  elif defined(__sparc)
				gtmsi->subcode = info->si_code;
				gtmsi->bad_vadr = info->si_addr;
				gtmsi->infotype |= GTMSIGINFO_BADR;
				if (NULL != context)
				{
					gtmsi->int_iadr = (caddr_t)context->uc_mcontext.gregs[REG_PC];
					gtmsi->infotype |= GTMSIGINFO_ILOC;
				}
				break;
#  else /* MVS -- not much info available */
				gtmsi->int_iadr = info->si_addr;
				gtmsi->infotype |= GTMSIGINFO_ILOC;
				break;
#  endif
		}

		/* See if additional information can be gleaned from the subcode */
#  if !defined(__MVS__)
		if (0 != gtmsi->subcode)
		{
			switch (sig)
			{
				case SIGILL :
					switch (gtmsi->subcode)
					{
						case ILL_ILLOPC : gtmsi->sig_err = ERR_SIGILLOPC;
							break;
						case ILL_ILLOPN : gtmsi->sig_err = ERR_SIGILLOPN;
							break;
						case ILL_ILLADR : gtmsi->sig_err = ERR_SIGILLADR;
							break;
						case ILL_ILLTRP : gtmsi->sig_err = ERR_SIGILLTRP;
							break;
						case ILL_PRVOPC : gtmsi->sig_err = ERR_SIGPRVOPC;
							break;
						case ILL_PRVREG : gtmsi->sig_err = ERR_SIGPRVREG;
							break;
						case ILL_COPROC : gtmsi->sig_err = ERR_SIGCOPROC;
							break;
						case ILL_BADSTK : gtmsi->sig_err = ERR_SIGBADSTK;
							break;
					}
					break;
				case SIGBUS :
					switch (gtmsi->subcode)
					{
						case BUS_ADRALN : gtmsi->sig_err = ERR_SIGADRALN;
							break;
						case BUS_ADRERR : gtmsi->sig_err = ERR_SIGADRERR;
							break;
						case BUS_OBJERR : gtmsi->sig_err = ERR_SIGOBJERR;
							break;
					}
					break;
				case SIGFPE :
					switch (gtmsi->subcode)
					{
						case FPE_INTDIV : gtmsi->sig_err = ERR_SIGINTDIV;
							break;
						case FPE_INTOVF : gtmsi->sig_err = ERR_SIGINTOVF;
							break;
						case FPE_FLTDIV : gtmsi->sig_err = ERR_SIGFLTDIV;
							break;
						case FPE_FLTOVF : gtmsi->sig_err = ERR_SIGFLTOVF;
							break;
						case FPE_FLTUND : gtmsi->sig_err = ERR_SIGFLTUND;
							break;
						case FPE_FLTRES : gtmsi->sig_err = ERR_SIGFLTRES;
							break;
						case FPE_FLTINV	: gtmsi->sig_err = ERR_SIGFLTINV;
							break;
					}
					break;
				case SIGSEGV :
					switch (gtmsi->subcode)
					{
						case SEGV_MAPERR : gtmsi->sig_err = ERR_SIGMAPERR;
							break;
						case SEGV_ACCERR : gtmsi->sig_err = ERR_SIGACCERR;
							break;
					}
					break;
			}
		}
#  endif /* Attempt to glean more info from subcode */
	}
#elif defined(_AIX)
	/* Aix only returns context information through Aix 4.3.3. Will need to look at
	   slightly different structures if running in 64 bit. */
	if (NULL != context)
	{
		gtmsi->bad_vadr = (caddr_t)context->sc_jmpbuf.jmp_context.o_vaddr;
		gtmsi->int_iadr = (caddr_t)context->sc_jmpbuf.jmp_context.iar;
		gtmsi->infotype |= GTMSIGINFO_ILOC;
		if (NULL != gtmsi->bad_vadr)
			gtmsi->infotype != GTMSIGINFO_BADR;
	}
#elif defined(__linux__)
	/* Linux does not yet (intel RedHat 6.1) support returning signal information properly so
	   we will pull what information we can out of the context blocks. */
# ifndef REG_EIP
#   define REG_EIP EIP
# endif
	if (NULL != context)
	{
		gtmsi->int_iadr = (caddr_t)context->uc_mcontext.gregs[REG_EIP];
		gtmsi->infotype = GTMSIGINFO_ILOC;
		if (NULL != (gtmsi->bad_vadr = (caddr_t)context->uc_mcontext.cr2))
			gtmsi->infotype |= GTMSIGINFO_BADR;
	}
#else
#  error "Unsupported Platform"
#endif
}
