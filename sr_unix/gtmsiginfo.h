/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_SIGNAL_INFO_H
#define GTM_SIGNAL_INFO_H

/* Define signal information structures and routines */

#ifdef __sparc
#  include <sys/siginfo.h>
#  include <ucontext.h>
#endif
#ifdef __linux__
#  include <ucontext.h>
#endif

typedef struct
{
	caddr_t	int_iadr;		/* Interrupted instruction address */
	caddr_t	bad_vadr;		/* Failing virtual address */
	int	sig_err;		/* Error message with signal reason clarification */
	pid_t	send_pid;		/* Process that sent signal */
	uid_t	send_uid;		/* Userid that sent signal */
	int	subcode;		/* Subcode used to create message */
	int	infotype;		/* mask of what types of information are available */
	int	signal;			/* Actual signal number */
} gtmsiginfo_t;

/* Info types that are available (mask) */
#define GTMSIGINFO_NONE		0	/* No further information is available about the signals */
#define GTMSIGINFO_ILOC		0x1	/* Interrupt location information is available */
#define GTMSIGINFO_BADR		0x2	/* Bad virtual address information is available */
#define GTMSIGINFO_USER		0x4	/* User information is available */

#if defined(__osf__)
typedef	struct sigcontext	gtm_sigcontext_t;
#elif defined(__CYGWIN__)
typedef	struct ucontext		gtm_sigcontext_t;
#elif defined(_AIX)
typedef struct sigcontext64	gtm_sigcontext_t;
#else
typedef	ucontext_t		gtm_sigcontext_t;
#endif

void extract_signal_info(int sig, siginfo_t *info, gtm_sigcontext_t *context, gtmsiginfo_t *gtmsi);

#define NO_SUSPEND		0	/* Suspend not pending */
#define DEFER_SUSPEND		1	/* Suspend deferred */
#define NOW_SUSPEND		2	/* Suspend now; will indicate that we "suspended" ourselves when woken up */

/* States of exit_state
 *
 * Normal state is no exit is pending. When we receive a signal, we are either going
 * to go into a pending state (wait until out of crit) or an immediate state (going
 * to exit now). There are two different pending states. In the first, we can tolerate
 * another (2nd) signal which will bump us to final pending state. If a 3rd signal
 * comes in, we will go to the immediate exit state [tolerant pending state added at the
 * request of Roger 4/2000]. If one of the terminal signals is received (i.e. not
 * sent by another user, we will go to the immediate exit state.
 */

#define EXIT_NOTPENDING		0
#define EXIT_PENDING_TOLERANT	1
#define EXIT_PENDING		2
#define EXIT_IMMED		3

#endif /* ifndef GTM_SIGNAL_INFO_H */
