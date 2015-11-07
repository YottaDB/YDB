/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#define BYPASS_MEMCPY_OVERRIDE  /* Signals gtm_string.h to not override memcpy(). When this routine is linked into gtcm_pkdisp,
				 * the assert in the routine called by memcpy macro causes the world to be pulled in. Avoid.
				 */
/* Note that since this routine is called prior to reading environment vars or pretty much any
 * other initialization, we cannot use gtm_malloc() yet so care is taken to use the real system
 * malloc.
 */
#undef malloc
#undef free

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_iconv.h"
#include "gtm_socket.h"
#include "gtm_unistd.h"
#include "gtm_limits.h"

#include <signal.h>
#include <sys/time.h>
#ifdef UNIX
# include <sys/un.h>
#endif
#ifdef VMS
# include <descrip.h>		/* Required for gtmsource.h */
# include <ssdef.h>
# include <fab.h>
# include "desblk.h"
#endif
#include "cache.h"
#include "hashtab_addr.h"
#include "hashtab_int4.h"
#include "hashtab_int8.h"
#include "hashtab_mname.h"
#include "hashtab_str.h"
#include "hashtab_objcode.h"
#include "error.h"
#include <rtnhdr.h>
#include "gdsroot.h"
#include "gdskill.h"
#include "ccp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "comline.h"
#include "compiler.h"
#include "cmd_qlf.h"
#include "io.h"
#include "iosp.h"
#include "jnl.h"
#include "lv_val.h"
#include "collseq.h"
#include "mdq.h"
#include "mprof.h"
#include "mv_stent.h"
#include "find_mvstent.h"	/* needed for zintcmd_active */
#include "stack_frame.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "zshow.h"
#include "zwrite.h"
#include "zbreak.h"
#include "fnpc.h"
#include "mmseg.h"
#ifndef VMS
# include "gtmsiginfo.h"
#endif
#include "gtmimagename.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"	/* needed for socket_pool and MAX_N_SOCKETS*/
#include "ctrlc_handler_dummy.h"
#include "unw_prof_frame_dummy.h"
#include "op.h"
#include "gtmsecshr.h"
#include "error_trap.h"
#include "patcode.h"		/* for pat_everything and sizeof_pat_everything */
#include "source_file.h"	/* for REV_TIME_BUFF_LEN */
#include "mupipbckup.h"
#include "dpgbldir.h"
#include "mmemory.h"
#include "have_crit.h"
#include "alias.h"
#include "zroutines.h"
#include "parm_pool.h"
#include "util.h"		/* for util_outbuff manipulations */
#include "nametabtyp.h"

/* FOR REPLICATION RELATED GLOBALS */
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "replgbl.h"
#include "trace_table.h"

/* FOR MERGE RELATED GLOBALS */
#include "gvname_info.h"
#include "op_merge.h"

#ifdef UNIX
# include "cli.h"
# include "invocation_mode.h"
# include "fgncal.h"
# include "parse_file.h"	/* for MAX_FBUFF */
# include "repl_sem.h"
# include "gtm_zlib.h"
# include "zro_shlibs.h"
#endif

#include "jnl_typedef.h"

#ifdef VMS
# include "gtm_logicals.h"	/* for GTM_MEMORY_NOACCESS_COUNT */
#endif

#include "gds_blk_upgrade.h"	/* for UPGRADE_IF_NEEDED flag */
#include "cws_insert.h"		/* for CWS_REORG_ARRAYSIZE */

#ifdef UNICODE_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
#endif

#ifdef GTM_CRYPT
# include "gtmcrypt.h"
# include "gdsblk.h"
# include "muextr.h"
#endif

#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
#endif

#include "gtm_threadgbl_init.h"

#define	DEFAULT_PROMPT	"GTM>"

GBLDEF void	*gtm_threadgbl;		/* Anchor for thread global for this thread */

/* Since gtm_threadgbl is type-neutral, define a structure mapping just for this routine that *does*
 * contain the types. This structure can be accessed through the debugger for easier type dumping.
 * Note we define this structure even in pro since it could be useful even in pro debugging (by gtmpcat).
 */
#define THREADGBLDEF(name,type)			type name;
#define THREADGBLFPTR(name, type, args)		type (*name)args;
#define THREADGBLAR1DEF(name, type, dim1)	type name[dim1];
#define THREADGBLAR2DEF(name, type, dim1, dim2)	type name[dim1][dim2];
typedef struct
{
#	include "gtm_threadgbl_defs.h"
} gtm_threadgbl_true_t;
#undef THREADGBLDEF
#undef THREADGBLFPTR
#undef THREADGBLAR1DEF
#undef THREADGBLAR2DEF

GBLDEF gtm_threadgbl_true_t	*gtm_threadgbl_true;

/* This routine allocates the thread global structure and for now, since GTM is not yet threaded,
 * anchors it in a global variable. This still improves access to global variables even in this
 * paradym because the 3 step global dereference only need happen once per module.
 */

error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);
error_def(ERR_GTMASSERT);

void gtm_threadgbl_init(void)
{
	void	*lcl_gtm_threadgbl;

	if (SIZEOF(gtm_threadgbl_true_t) != size_gtm_threadgbl_struct)
	{	/* Size mismatch with gtm_threadgbl_deftypes.h - no error handling yet available so do
		 * the best we can.
		 */
		FPRINTF(stderr, "GTM-F-GTMASSERT gtm_threadgbl_true_t and gtm_threadgbl_t are different sizes\n");
		exit(ERR_GTMASSERT);
	}
	if (NULL != gtm_threadgbl)
	{	/* has already been initialized - don't re-init */
		FPRINTF(stderr, "GTM-F-GTMASSERT gtm_threadgbl is already initialized\n");
		exit(ERR_GTMASSERT);
	}
	gtm_threadgbl = lcl_gtm_threadgbl = malloc(size_gtm_threadgbl_struct);
	if (NULL == gtm_threadgbl)
	{	/* Storage was not allocated for some reason - no error handling yet still */
		perror("GTM-F-MEMORY Unable to allocate startup thread structure");
		exit(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY));
	}
	memset(gtm_threadgbl, 0, size_gtm_threadgbl_struct);
	gtm_threadgbl_true = (gtm_threadgbl_true_t *)gtm_threadgbl;

	/* Add specific initializations if other than 0s here using the TREF() family of macros: */
	(TREF(director_ident)).addr = TADR(director_string);
	TREF(for_stack_ptr) = TADR(for_stack);
	(TREF(gtmprompt)).addr = TADR(prombuf);
	(TREF(gtmprompt)).len = SIZEOF(DEFAULT_PROMPT) - 1;
	TREF(lv_null_subs) = LVNULLSUBS_OK;	/* UNIX: set in gtm_env_init_sp(), VMS: set in gtm$startup() - init'd here
							 * in case alternative invocation methods bypass gtm_startup()
							 */
	MEMCPY_LIT(TADR(prombuf), DEFAULT_PROMPT);
	(TREF(replgbl)).jnl_release_timeout = DEFAULT_JNL_RELEASE_TIMEOUT;
	(TREF(window_ident)).addr = TADR(window_string);
	TREF(util_outbuff_ptr) = TADR(util_outbuff);	/* Point util_outbuff_ptr to the beginning of util_outbuff at first. */
}
