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

/* Since we are about to create gtm_threadgbl_deftypes.h, signal gtm_threadgbl.h to avoid including it */
#define NO_THREADGBL_DEFTYPES
#include "mdef.h"

#include <stddef.h>
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
#include "patcode.h"	/* for pat_everything and sizeof_pat_everything */
#include "source_file.h"	/* for REV_TIME_BUFF_LEN */
#include "mupipbckup.h"
#include "dpgbldir.h"
#include "mmemory.h"
#include "have_crit.h"
#include "alias.h"
#include "zroutines.h"
#include "trace_table.h"
#include "parm_pool.h"
#include "util.h"		/* for util_outbuff manipulations */
#include "nametabtyp.h"

/* FOR REPLICATION RELATED GLOBALS */
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "replgbl.h"

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

/* This module's purpose is to generate gtm_threadgbl_deftypes.h for a given platform. This header file
 * contains all the type and offset informatin needed for the TREF macro in gtm_threadgbl.h to access
 * any field in the global structure without having to have all the types known in every module. Only the
 * types used need be known.
 *
 * This is acomplished by creating the structure in this module with all types and offsets known and outputting
 * those values in the form of #define statements that can be used by subsequent compiles.
 */

/* First step, create the structure */
#define THREADGBLDEF(name, type)		type name;
#define THREADGBLFPTR(name, type, args)		type (*name)args;
#define THREADGBLAR1DEF(name, type, dim1)	type name[dim1];
#define THREADGBLAR2DEF(name, type, dim1, dim2)	type name[dim1][dim2];
typedef struct
{
#include "gtm_threadgbl_defs.h"
} gtm_threadgbl_def_t;
#undef THREADGBLDEF
#undef THREADGBLFPTR
#undef THREADGBLAR1DEF
#undef THREADGBLAR2DEF

/* Note this module uses regular (lower case) printf because using PRINTF calls gtm_printf which is inappropriate
 * since this module is not part of the GTM runtime but a standalone text generator.
 */

/* Define macros that will generate the type and offset #defines */
#define PRINT_TYPE_OFFSET(name, type)								\
	printf("# define ggo_%s %d\n", #name, (int)OFFSETOF(gtm_threadgbl_def_t, name));	\
	printf("# define ggt_%s %s\n", #name, #type);

/* For function pointers, we need the offset and type (which is a return type in this case since the actual type of
 * the item is "function pointer") but also need the argument declarations for the function declaration to be complete
 * Lastly, we need a function pointer typedef to make invocations work correctly.
 */
#define PRINT_TYPE_OFFSET_FPTR(name, type, args) 						\
	printf("# define ggo_%s %d\n", #name, (int)OFFSETOF(gtm_threadgbl_def_t, name));	\
	printf("# define ggt_%s %s\n", #name, #type);	/* In this case, return type */		\
	printf("# define gga_%s %s\n", #name, #args);						\
	printf("typedef %s (*ggf_%s)%s;\n", #type, #name, #args);

/* For single dimension arrays, include the length of the entire array as it is likely needed, especially
 * for character types.
 */
#define PRINT_TYPE_OFFSET_ARY1(name, type, dim1)						\
	printf("# define ggo_%s %d\n", #name, (int)OFFSETOF(gtm_threadgbl_def_t, name));	\
	printf("# define ggt_%s %s\n", #name, #type);						\
	printf("# define ggl_%s %d\n", #name, (int)SIZEOF(gtd.name));

/* For two dimensional arrays, we need to record the 2nd dimension as it is needed in the address computations */
#define PRINT_TYPE_OFFSET_ARY2(name, type, dim1, dim2)						\
	printf("# define ggo_%s %d\n", #name, (int)OFFSETOF(gtm_threadgbl_def_t, name));	\
	printf("# define ggt_%s %s\n", #name, #type);						\
	printf("# define ggl_%s %d\n", #name, (int)SIZEOF(gtm_threadgbl_def_t.name));		\
	printf("# define ggd_%s %d\n", #name, (int)dim2);

int main()
{
	gtm_threadgbl_def_t gtd;

	/* Now run through each var in the structure generating defines for the type and offset within the structure */
#	define THREADGBLDEF(name, type)			PRINT_TYPE_OFFSET(name, type)
#	define THREADGBLFPTR(name, type, args)		PRINT_TYPE_OFFSET_FPTR(name, type, args)
#	define THREADGBLAR1DEF(name, type, dim1)	PRINT_TYPE_OFFSET_ARY1(name, type, dim1)
#	define THREADGBLAR2DEF(name, type, dim1, dim2)	PRINT_TYPE_OFFSET_ARY1(name, type, dim1, dim2)
#	include "gtm_threadgbl_defs.h"
#	undef THREADGBLDEF
#	undef THREADGBLFPTR
#	undef THREADGBLAR1DEF
#	undef THREADGBLAR2DEF
	printf("# define size_gtm_threadgbl_struct %d\n", (int)SIZEOF(gtm_threadgbl_def_t));
	return 0;
}
