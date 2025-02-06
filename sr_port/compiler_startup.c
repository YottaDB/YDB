/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include "mdq.h"
#include "cgp.h"
#include "error.h"
#include "mmemory.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "list_file.h"
#include "source_file.h"
#include "lb_init.h"
#include "reinit_compilation_externs.h"
#include "comp_esc.h"
#include "resolve_blocks.h"
#include "hashtab_str.h"
#include "rtn_src_chksum.h"
#include "gtmmsg.h"
#include "iosp.h"	/* for SS_NORMAL */
#include "start_fetches.h"
#include "ydb_shebang.h"	/* for REPLACE_IF_SHEBANG_WITH_SEMICOLON */

#define HOPELESS_COMPILE 128

GBLREF int	source_column;

GBLREF char			cg_phase;	/* code generation phase */
GBLREF command_qualifier	cmd_qlf;
GBLREF hash_table_str		*complits_hashtab;
GBLREF int			mlmax;
GBLREF mcalloc_hdr 		*mcavailptr, *mcavailbase;
GBLREF mline			mline_root;
GBLREF spdesc			indr_stringpool, rts_stringpool, stringpool;
GBLREF src_line_struct 	src_head;
GBLREF triple			t_orig;
GBLREF unsigned char		source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF boolean_t		shebang_invocation;	/* TRUE if yottadb is invoked through the "ydbsh" soft link */

LITDEF char compile_terminated[] = "COMPILATION TERMINATED DUE TO EXCESS ERRORS";

error_def(ERR_ERRORSUMMARY);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLNOOBJECT);

boolean_t compiler_startup(void)
{
#ifdef DEBUG
	void			dumpall();
#endif
	boolean_t		compile_w_err, need_source_lines, use_src_queue, creating_list_file;
	unsigned char		err_buf[45];
	unsigned char 		*cp, *cp2;
	int			errknt;
	int4			n;
	uint4			line_count, total_source_len;
	mlabel			*null_lab;
	src_line_struct		*sl;
	mident			null_mident;
	gtm_rtn_src_chksum_ctx	checksum_ctx;
	mstr			str;
	size_t			mcallocated, alloc;
	size_t			stlen;
	mcalloc_hdr		*lastmca = NULL, *nextmca;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Although we have an invocation of compiler cleanups at the end of this module, there exist ways to avoid this
	 * cleanup by working in direct mode, getting certain types of errors combined with an argumentless ZGOTO that unwinds
	 * pretty much everything that can bypass that cleanup. So do a quick check if it is needed and if so, git-r-done
	 * (test is part of the macro invocation). Note this is the easiest place to make this check rather than complicating
	 * error handling to provide a similar effect.
	 */
	/* The stringpool switching pattern is to copy the stringpool structure to the holding structure for the currently active
	 * pool and then copy the other pool structure into the stringpool structure. The initialization pattern is to stp_init
	 * the stringpool structure and then copy it to the holding structure for the pool we're initializing. The indr_stringpool
	 * might more appropriately be called cmplr_stringpool, because we use it for all compiles not just those for indirection
	 */
	if (rts_stringpool.base == stringpool.base)
	{
		rts_stringpool = stringpool;
		if (!indr_stringpool.base)
		{
			stp_init(STP_INITSIZE);
			indr_stringpool = stringpool;
		} else
			stringpool = indr_stringpool;
	}
	run_time = FALSE;
	TREF(compile_time) = TRUE;
	TREF(transform) = FALSE;
	TREF(dollar_zcstatus) = SS_NORMAL;
	reinit_compilation_externs();
	memset(&null_mident, 0, SIZEOF(null_mident));
	ESTABLISH_RET(compiler_ch, FALSE);
	/* If last compile errored out, it may have left stuff - find out how much space we have in mcalloc blocks */
	for (mcallocated = 0, nextmca = mcavailptr; nextmca; nextmca = nextmca->link)
		mcallocated += nextmca->size;
	if (0 == mcallocated)
		mcallocated = MC_DSBLKSIZE - MCALLOC_HDR_SZ;	/* Min size is one default block size */
	COMPILE_HASHTAB_CLEANUP;
	cg_phase = CGP_NOSTATE;
	TREF(source_error_found) = errknt = 0;
	open_source_file();
	rtn_src_chksum_init(&checksum_ctx);
	cg_phase = CGP_PARSE;
	creating_list_file = (cmd_qlf.qlf & CQ_LIST) || (cmd_qlf.qlf & CQ_CROSS_REFERENCE);
	need_source_lines = (cmd_qlf.qlf & CQ_EMBED_SOURCE) || creating_list_file;
	use_src_queue = (cmd_qlf.qlf & CQ_EMBED_SOURCE) || (creating_list_file && (cmd_qlf.qlf & CQ_MACHINE_CODE));
	dqinit(&src_head, que);
	if (creating_list_file)
		open_list_file();
	if (cmd_qlf.qlf & CQ_CE_PREPROCESS)
		open_ceprep_file();
	tripinit();
	null_lab = get_mladdr(&null_mident);
	null_lab->ml = &mline_root;
	mlmax++;
	(TREF(fetch_control)).curr_fetch_trip = (TREF(fetch_control)).curr_fetch_opr = newtriple(OC_LINEFETCH);
	(TREF(fetch_control)).curr_fetch_count = 0;
	TREF(code_generated) = FALSE;
	line_count = 1;
	total_source_len = 0;
	for (TREF(source_line) = 1;  errknt <= HOPELESS_COMPILE;  (TREF(source_line))++)
	{
		if (-1 == (n = read_source_file()))
			break;
		/* Whether or not, we come here through "ydbsh" or "yottadb", allow for shebang lines in the first line
		 * and do not issue a compile error. Hence the below macro is invoked even if "shebang_invocation" is FALSE.
		 */
		if (1 == TREF(source_line))
			REPLACE_IF_SHEBANG_WITH_SEMICOLON(TREF(source_buffer));
		rtn_src_chksum_line(&checksum_ctx, (TREF(source_buffer)).addr, n);
		/* Save the source lines; a check later determines whether to include them in the object file */
		/* Accumulate list of M source lines */
		sl = (src_line_struct *)mcalloc(SIZEOF(src_line_struct));
		dqrins(&src_head, que, sl);
		stlen = n + 1;
		sl->str.addr = mcalloc(stlen);		/* == n+1 for zero termination */
		sl->str.len = n;
		sl->line = TREF(source_line);
		assert(NULL != sl->str.addr);
		memcpy(sl->str.addr, (TREF(source_buffer)).addr, stlen);
		total_source_len += n;
		cp = (unsigned char *)((TREF(source_buffer)).addr + n - 1);
		NEWLINE_TO_NULL(*cp); /* avoid SPOREOL errors due to trailing newlines */
		if (need_source_lines && creating_list_file && !(cmd_qlf.qlf & CQ_MACHINE_CODE))
		{	/* list now. for machine_code we intersperse machine code and M code, thus can't list M code yet */
			list_line_number();
			list_line((TREF(source_buffer)).addr);
		}
		TREF(source_error_found) = 0;
		lb_init();
		if (cmd_qlf.qlf & CQ_CE_PREPROCESS)
			put_ceprep_line();
		if (!line(&line_count))
		{
			assert(TREF(source_error_found));
			errknt++;
		}
	}
	rtn_src_chksum_digest(&checksum_ctx);
	close_source_file();
	if (cmd_qlf.qlf & CQ_CE_PREPROCESS)
		close_ceprep_file();
	cg_phase = CGP_RESOLVE;
	if (t_orig.exorder.fl == &t_orig)	/* if no lines in routine, set up line 0 */
		newtriple(OC_LINESTART);
	newtriple(OC_RET);			/* always provide a default QUIT */
	mline_root.externalentry = t_orig.exorder.fl;
	start_fetches(OC_NOOP);
	resolve_blocks();
	errknt = resolve_ref(errknt);
	gvname2naked_optimize(&t_orig);
	assert(indr_stringpool.base == stringpool.base);
	INVOKE_STP_GCOL(0);
	/* The above invocation of stp_gcol with a parameter of 0 is a critical part of compilation
	 * (both routine compilations and indirect dynamic compilations). This collapses the indirect
	 * (compilation) stringpool so that only the literals are left. This stringpool is then written
	 * out to the compiled object as the literal pool for that compilation. Temporary stringpool
	 * use for conversions or whatever are eliminated. Note the path is different in stp_gcol for
	 * the indirect stringpool which is only used during compilations.
	 */
	if (cmd_qlf.qlf & CQ_EMBED_SOURCE)
	{	/* Append source text to text pool */
		ENSURE_STP_FREE_SPACE(total_source_len);
		DBG_MARK_STRINGPOOL_UNEXPANDABLE;
		TREF(routine_source_offset) = (uint4)(stringpool.free - stringpool.base);
		dqloop(&src_head, que, sl)
		{
			str = sl->str;
			s2pool(&str); /* changes str.addr, points it into stringpool */
		}
		DBG_MARK_STRINGPOOL_EXPANDABLE;
	}
	/* At this point, the stringpool (indr_stringpool) contains the M literals (and embedded source text if CQ_EMBED_SOURCE).
	 * There should be no more activity on this stringpool as this is exactly what will get written out later to the
	 * compiled object file as the literal pool. Therefore mark this unusability using the below macro call. Until the
	 * stringpool is switched back to rts_stringpool (at the end of this function), we will keep it unusable.
	 */
	DBG_MARK_STRINGPOOL_UNUSABLE;
	compile_w_err = (errknt <= HOPELESS_COMPILE && (cmd_qlf.qlf & CQ_IGNORE));
	if (cmd_qlf.qlf & CQ_LIST || cmd_qlf.qlf & CQ_CROSS_REFERENCE)
	{
		list_line("");
		if (errknt)
			cp = i2asc(err_buf, errknt);
		else
		{
			cp = err_buf;
			*cp++ = 'n';
			*cp++ = 'o';
		}
		memcpy(cp, " error", SIZEOF(" error"));
		cp += SIZEOF(" error") - 1;
		if (1 != errknt)
			*cp++ = 's';
		*cp = 0;
		list_line((char *)err_buf);
		if (errknt > HOPELESS_COMPILE)
			list_line((char *)compile_terminated);
	}
	if ((!errknt || compile_w_err) && ((cmd_qlf.qlf & CQ_OBJECT) || (cmd_qlf.qlf & CQ_MACHINE_CODE)))
	{
		obj_code(line_count, &checksum_ctx);
		cg_phase = CGP_FINI;
	} else if (!compile_w_err)
	{
		TREF(dollar_zcstatus) = -ERR_ERRORSUMMARY;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_ZLINKFILE, 2, source_name_len, source_file_name, ERR_ZLNOOBJECT);
	}
	if (cmd_qlf.qlf & CQ_LIST || cmd_qlf.qlf & CQ_CROSS_REFERENCE)
		close_list_file();
	COMPILE_HASHTAB_CLEANUP;
	reinit_compilation_externs();
	/* Determine if need to remove any added mc blocks. Min value of mcallocated ensures we leave at least one block alone. */
	for (alloc = 0, nextmca = mcavailptr;
			nextmca && (alloc < mcallocated);
			alloc += nextmca->size, lastmca = nextmca, nextmca = nextmca->link)
		;
	if (nextmca)
	{	/* Start freeing at the nextmca node since these are added blocks */
		assert(lastmca);
		lastmca->link = NULL;	/* Sever link to further blocks here */
		/* Release any remaining blocks if any */
		for (lastmca = nextmca; lastmca; lastmca = nextmca)
		{
			nextmca = lastmca->link;
			free(lastmca);
		}
	}
	assert ((FALSE == run_time) && (TRUE == TREF(compile_time)));
	run_time = TRUE;
	TREF(compile_time) = FALSE;
	TREF(transform) = TRUE;
	assert(indr_stringpool.base == stringpool.base);
	DBG_MARK_STRINGPOOL_USABLE;	/* Now that we are about to switch stringpool back to rts_stringpool, make it usable */
	indr_stringpool = stringpool;
	stringpool = rts_stringpool;
	REVERT;
	return errknt ? TRUE : FALSE;
}
