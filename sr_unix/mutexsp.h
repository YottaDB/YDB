/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEXSP_H
#define MUTEXSP_H

#ifdef MUTEX_MSEM_WAKE
#ifdef POSIX_MSEM
#include <semaphore.h>
#else
#include <sys/mman.h>
#endif
#endif

#define MUTEX_INIT_SEMVAL		LOCK_AVAILABLE
#define MUTEX_IMPOSSIBLE_SEMVAL		-1024 /* Any value other than
					       * GLOBAL_LOCK_AVAILABLE,
					       * GLOBAL_LOCK_IN_USE, and
					       * REC_UNDER_PROGRESS */

#define MUTEX_CONST_TIMEOUT_VAL		16

#define MUTEX_NUM_WAIT_BITS		19 /* max p such that 2**p <= 10**6 */

#define MUTEX_INTR_SLPCNT		2

#define WRTPND_LSBIT_MASK 		1

#define MUTEX_SOCK_DIR                  GTM_TMP_ENV
#define DEFAULT_MUTEX_SOCK_DIR          DEFAULT_GTM_TMP
#define MUTEX_SOCK_FILE_PREFIX          "gtm_mutex"
#define MAX_MUTEX_SOCKFILE_NAME_LEN	(SIZEOF(MUTEX_SOCK_FILE_PREFIX) + MAX_DIGITS_IN_INT)

#define BIN_TOGGLE(x) ((x) ? 0 : 1)

#define ONE_MILLION 			1000000

#define MICROSEC_SLEEP(x)		m_usleep(x)

typedef struct
{
	uint4	pid;
	int	mutex_wake_instance;
} mutex_wake_msg_t;

typedef enum
{
	MUTEX_LOCK_WRITE,
	MUTEX_LOCK_WRITE_IMMEDIATE
} mutex_lock_t;

/*
 * Initialize a mutex with n que entries. If crash is TRUE, then this is
 * a "crash" reinitialization;  otherwise it's a "clean" initialization.
 */
void		gtm_mutex_init(gd_region *reg, int n, bool crash);

/* mutex_lockw - write access to mutex for region reg */
enum	cdb_sc	mutex_lockw(gd_region *reg, mutex_spin_parms_ptr_t mutex_spin_parms, int crash_count);

/*
 * mutex_lockwim - write access to mutex for region reg; if cannot lock,
 *                 immediately return cdb_sc_nolock
 */
enum	cdb_sc	mutex_lockwim(gd_region *reg, mutex_spin_parms_ptr_t mutex_spin_parms, int crash_count);

/* mutex_unlockw - unlock write access to mutex for region reg */
enum	cdb_sc	mutex_unlockw(gd_region *reg, int crash_count);

/* mutex_seed_init - set the seed for pseudo random mutex sleep times */
void 		mutex_seed_init(void);

void mutex_cleanup(gd_region *reg);

#ifdef MUTEX_MSEM_WAKE
#  ifdef POSIX_MSEM
void mutex_wake_proc(sem_t *mutex_wake_msem_ptr);
#  else
void mutex_wake_proc(msemaphore *mutex_wake_msem_ptr);
#  endif
#else
void mutex_wake_proc(sm_int_ptr_t pid, int mutex_wake_instance);
#endif

void mutex_sock_init(void);
void mutex_sock_cleanup(void);
unsigned char *pid2ascx(unsigned char *pid_str, pid_t pid);

void mutex_per_process_init(void);

#ifdef MUTEX_MSEM_WAKE
#ifdef POSIX_MSEM
#  define MSEM_LOCKW(X)	 sem_wait(X)
#  define MSEM_LOCKNW(X) sem_trywait(X)
#  define MSEM_UNLOCK(X) sem_post(X)
#else
#  define MSEM_LOCKW(X)	 msem_lock(X, 0)
#  define MSEM_LOCKNW(X) msem_lock(X, MSEM_IF_NOWAIT)
#  define MSEM_UNLOCK(X) msem_unlock(X, 0)
#endif
#endif


#if defined(MUTEX_DEBUG) || defined(MUTEX_TRACE) || defined(MUTEX_TEST_RECOVERY)
#include "gtm_stdio.h"
#endif

#ifdef MUTEX_DEBUG
#define MUTEX_DPRINT1(p)		{FPRINTF(stderr, p); FFLUSH(stderr);}
#define MUTEX_DPRINT2(p, q)		{FPRINTF(stderr, p, q); FFLUSH(stderr);}
#define MUTEX_DPRINT3(p, q, r)		{FPRINTF(stderr, p, q, r); FFLUSH(stderr);}
#define MUTEX_DPRINT4(p, q, r, s)	{FPRINTF(stderr, p, q, r, s); FFLUSH(stderr);}
#define MUTEX_DPRINT5(p, q, r, s,t) 	{FPRINTF(stderr, p, q, r, s, t); FFLUSH(stderr);}
#else
#define MUTEX_DPRINT1(p)
#define MUTEX_DPRINT2(p, q)
#define MUTEX_DPRINT3(p, q, r)
#define MUTEX_DPRINT4(p, q, r, s)
#define MUTEX_DPRINT5(p, q, r, s, t)
#endif

#ifdef MUTEX_TRACE
#define DECLARE_MUTEX_TRACE_CNTRS \
			    GBLDEF int mutex_trc_lockw = 0; \
			    GBLDEF int mutex_trc_lockwim = 0; \
			    GBLDEF int mutex_trc_w_atmpts = 0; \
			    GBLREF int mutex_trc_mutex_slp_fn = 0; \
			    GBLREF int mutex_trc_mutex_slp_fn_noslp = 0; \
			    GBLDEF int mutex_trc_slp = 0; \
			    GBLDEF int mutex_trc_wt_short_slp = 0; \
			    GBLDEF int mutex_trc_wtim_short_slp = 0; \
			    GBLDEF int mutex_trc_slp_tmout = 0; \
			    GBLDEF int mutex_trc_intr_tmout = 0; \
			    GBLDEF int mutex_trc_slp_intr = 0; \
			    GBLDEF int mutex_trc_slp_wkup = 0; \
			    GBLDEF int mutex_trc_pgybckd_dlyd_wkup = 0; \
			    GBLDEF int mutex_trc_xplct_dlyd_wkup = 0; \
			    GBLDEF int mutex_trc_crit_wk = 0; \
			    GBLDEF int mutex_trc_dump_done = 0;

#define USE_MUTEX_TRACE_CNTRS \
			    GBLREF int mutex_trc_lockw; \
			    GBLREF int mutex_trc_lockwim; \
			    GBLREF int mutex_trc_w_atmpts; \
			    GBLREF int mutex_trc_mutex_slp_fn; \
			    GBLREF int mutex_trc_mutex_slp_fn_noslp; \
			    GBLREF int mutex_trc_slp; \
			    GBLREF int mutex_trc_wt_short_slp; \
			    GBLREF int mutex_trc_wtim_short_slp; \
			    GBLREF int mutex_trc_slp_tmout; \
			    GBLDEF int mutex_trc_intr_tmout; \
			    GBLDEF int mutex_trc_slp_intr; \
			    GBLREF int mutex_trc_slp_wkup; \
			    GBLREF int mutex_trc_pgybckd_dlyd_wkup; \
			    GBLREF int mutex_trc_xplct_dlyd_wkup; \
			    GBLREF int mutex_trc_crit_wk; \
			    GBLDEF int mutex_trc_dump_done;

#define MUTEX_TRACE_CNTR(x) ((x)++)

#define DUMP_MUTEX_TRACE_CNTRS \
if (!mutex_trc_dump_done) \
{ \
  FPRINTF(stderr, "mutex_lockw : %d\n", mutex_trc_lockw);\
  FPRINTF(stderr, "mutex_lockwim : %d\n", mutex_trc_lockwim);\
  FPRINTF(stderr, "Lock write attempts : %d\n", mutex_trc_w_atmpts);\
  FPRINTF(stderr, "Calls to mutex_sleep : %d\n", mutex_trc_mutex_slp_fn);\
  FPRINTF(stderr, "Mutex_sleep no sleep : %d\n", mutex_trc_mutex_slp_fn_noslp);\
  FPRINTF(stderr, "Write short sleep count : %d\n", mutex_trc_wt_short_slp);\
  FPRINTF(stderr, "Wrt Imm short slp count : %d\n", mutex_trc_wtim_short_slp);\
  FPRINTF(stderr, "Sleep timeout : %d\n", mutex_trc_slp_tmout);\
  FPRINTF(stderr, "Sleep timeout caused by intr: %d\n", mutex_trc_intr_tmout);\
  FPRINTF(stderr, "Sleep interrupted : %d\n", mutex_trc_slp_intr);\
  FPRINTF(stderr, "Sleep woken up : %d\n", mutex_trc_slp_wkup);\
  FPRINTF(stderr, "Dlyd pgybckd slp wkup : %d\n", mutex_trc_pgybckd_dlyd_wkup);\
  FPRINTF(stderr, "Dlyd xplct slp wkup : %d\n", mutex_trc_xplct_dlyd_wkup);\
  FPRINTF(stderr, "Crit wake others : %d\n", mutex_trc_crit_wk);\
  mutex_trc_dump_done = 1;\
}

#else

#define DECLARE_MUTEX_TRACE_CNTRS
#define USE_MUTEX_TRACE_CNTRS
#define MUTEX_TRACE_CNTR(x)
#define DUMP_MUTEX_TRACE_CNTRS

#endif /* MUTEX_TRACE */

#ifdef MUTEX_TEST_RECOVERY

#include <signal.h>
#define DECLARE_MUTEX_TEST_SIGNAL_FLAG	GBLDEF int mutex_test_signalled = FALSE;
#define USE_MUTEX_TEST_SIGNAL_FLAG	GBLREF int mutex_test_signalled;
#define MUTEX_TEST_SIGNAL_HERE(x, y) 					     \
					if ((y) && !mutex_test_signalled)    \
					{ 				     \
						FPRINTF(stderr, x);          \
						FFLUSH(stderr); 	     \
						mutex_test_signalled = TRUE; \
						exi_rundown(SIGQUIT);	change this before use     \
					} 				     \
					else ;
#define MUTEX_TEST_PRINT1(p)		{FPRINTF(stderr, p); FFLUSH(stderr);}
#define MUTEX_TEST_PRINT2(p, q)		{FPRINTF(stderr, p, q); FFLUSH(stderr);}
#define MUTEX_TEST_PRINT3(p, q, r)	{FPRINTF(stderr, p, q, r); FFLUSH(stderr);}
#define MUTEX_TEST_PRINT4(p, q, r, s)	{FPRINTF(stderr, p, q, r, s); FFLUSH(stderr);}
#define MUTEX_TEST_PRINT5(p, q, r, s,t) {FPRINTF(stderr, p, q, r, s, t); FFLUSH(stderr);}

#else

#define DECLARE_MUTEX_TEST_SIGNAL_FLAG
#define USE_MUTEX_TEST_SIGNAL_FLAG
#define MUTEX_TEST_SIGNAL_HERE(x, y)
#define MUTEX_TEST_PRINT1(p)
#define MUTEX_TEST_PRINT2(p, q)
#define MUTEX_TEST_PRINT3(p, q, r)
#define MUTEX_TEST_PRINT4(p, q, r, s)
#define MUTEX_TEST_PRINT5(p, q, r, s, t)

#endif /* MUTEX_TEST */

#endif /* MUTEXSP_H */
