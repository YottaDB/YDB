/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define _POSIX_EXIT	/* Needed for VMS system() call */ /* BYPASSOK: system() used insode the comment, no SYSTEM() needed */
#include "mdef.h"

#ifdef VMS
#include <descrip.h>
#include <rms.h>
#include <ssdef.h>
#endif

#include <errno.h>
#include "sys/wait.h"
#include "gtm_stat.h"
#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"

#include "gtmio.h"
#include "copy.h"
#include "iosp.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblk.h"
#include "patcode.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "gtmmsg.h"
#include "eintr_wrappers.h"
#include "min_max.h"
#include "error.h"
#include "jnl.h"
#include "trans_log_name.h"
#include "dbcertify.h"

#define FILETAB		"File  	"
#define REGIONTAB 	"Region	"

error_def(ERR_DEVOPENFAIL);
error_def(ERR_SYSCALL);
error_def(ERR_DBRDONLY);
error_def(ERR_DBOPNERR);
error_def(ERR_DBCCMDFAIL);
error_def(ERR_DBCINTEGERR);
error_def(ERR_PREMATEOF);
error_def(ERR_TEXT);
error_def(ERR_TRNLOGFAIL);
error_def(ERR_NOREGION);
error_def(ERR_DBNOTGDS);
error_def(ERR_BADDBVER);
error_def(ERR_DBMINRESBYTES);
error_def(ERR_DBMAXREC2BIG);
error_def(ERR_DBCKILLIP);
error_def(ERR_DBCNOTSAMEDB);
error_def(ERR_LOGTOOLONG);
error_def(ERR_GTMDISTUNDEF);

/* Open the temporary file that hold the command(s) we are going to execute */
void dbc_open_command_file(phase_static_area *psa)
{
	char_ptr_t	errmsg;
	int		rc, save_errno;
	int4		status;
	char		*dist_ptr;
	mstr		gtm_dist_m, gtm_dist_path;
	char		gtm_dist_path_buff[MAX_FN_LEN + 1];

	assert(NULL != psa && NULL == psa->tcfp);
	if (!psa->tmp_file_names_gend)
		dbc_gen_temp_file_names(psa);
	gtm_dist_m.addr = UNIX_ONLY("$")GTM_DIST;
	gtm_dist_m.len = SIZEOF(UNIX_ONLY("$")GTM_DIST) - 1;
	status = TRANS_LOG_NAME(&gtm_dist_m, &gtm_dist_path, gtm_dist_path_buff, SIZEOF(gtm_dist_path_buff),
					dont_sendmsg_on_log2long);
#ifdef UNIX
	if (SS_LOG2LONG == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3,
				gtm_dist_m.len, gtm_dist_m.addr, SIZEOF(gtm_dist_path_buff) - 1);
	else
#endif
	if (SS_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMDISTUNDEF);
	assert(0 < gtm_dist_path.len);
	VMS_ONLY(dbc_remove_command_file(psa));	/* If we don't do this, the command files versions pile up fast */
	psa->tcfp = Fopen((char_ptr_t)psa->tmpcmdfile, "w");
	if (NULL == psa->tcfp)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DEVOPENFAIL, 2, psa->tmpcmdfile_len, psa->tmpcmdfile,
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
	UNIX_ONLY(dbc_write_command_file(psa, SHELL_START));
	MEMCPY_LIT(psa->util_cmd_buff, SETDISTLOGENV);
	memcpy(psa->util_cmd_buff + SIZEOF(SETDISTLOGENV) - 1, gtm_dist_path.addr, gtm_dist_path.len);
	psa->util_cmd_buff[SIZEOF(SETDISTLOGENV) - 1 + gtm_dist_path.len] = 0;	/* Null temrinator */
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
}

/* Write a record to temporary command file */
void dbc_write_command_file(phase_static_area *psa, char_ptr_t cmd)
{
	char_ptr_t	errmsg;
	int		rc, save_errno;

	assert(NULL != psa && NULL != psa->tcfp);
	assert(psa->tmp_file_names_gend);
	rc = FPRINTF(psa->tcfp, "%s\n", cmd);
	if (-1 == rc)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fprintf()"), CALLFROM, /* BYPASSOK */
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
	rc = CHMOD((char_ptr_t)psa->tmpcmdfile, S_IRUSR + S_IWUSR + S_IXUSR + S_IRGRP + S_IROTH); /* Change to 744 */
	if (-1 == rc)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(15) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("chmod()"), CALLFROM,
			  ERR_TEXT, 2, psa->tmpcmdfile_len, psa->tmpcmdfile,
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
}

/* Close the temporary command file */
void dbc_close_command_file(phase_static_area *psa)
{
	assert(NULL != psa && NULL != psa->tcfp);
	assert(psa->tmp_file_names_gend);
	fclose(psa->tcfp);
	psa->tcfp = NULL;
}

/* Execute the temporary command file */
void dbc_run_command_file(phase_static_area *psa, char_ptr_t cmdname, char_ptr_t cmdargs, boolean_t piped_result)
{
	int		rc, cmd_len;
	unsigned char	cmdbuf1[MAX_FN_LEN + 256], cmdbuf2[MAX_FN_LEN + 1], *cp;

	assert(NULL != psa && NULL == psa->tcfp);
	assert(psa->tmp_file_names_gend);
	MEMCPY_LIT(cmdbuf1, RUN_CMD);
	cp = cmdbuf1 + SIZEOF(RUN_CMD) - 1;
	memcpy(cp, psa->tmpcmdfile, psa->tmpcmdfile_len);
	cp += psa->tmpcmdfile_len;
	cmd_len = (int)(cp - cmdbuf1);
	*cp = '\0';

	rc = dbc_syscmd((char_ptr_t)cmdbuf1);
	if (0 != rc)
	{	/* If piped_result, they can't see what went wrong (error messages likely in result file */
		if (piped_result)
		{	/* Output result file so they can see what happened -- this may or may not work but it is
			   the best we can do at this point */
			MEMCPY_LIT(cmdbuf2, DUMPRSLTFILE);
			cp = cmdbuf2 + SIZEOF(DUMPRSLTFILE) - 1;
			memcpy(cp, psa->tmprsltfile, psa->tmprsltfile_len);
			cp += psa->tmprsltfile_len;
			*cp = '\0';
			dbc_syscmd((char_ptr_t)cmdbuf2);
		}
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(13) ERR_DBCCMDFAIL, 7, rc, cmd_len, cmdbuf1, RTS_ERROR_TEXT(cmdname),
			  RTS_ERROR_TEXT(cmdargs), ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Note that the "UNIX_ONLY("environment variable $")VMS_ONLY("logical ")GTM_DIST
					    " must point to the current GT.M V4 installation"));
	}
}

/* Remove/dalete command file - normally only needed at cleanup since open with "W" will delete any existing file. */
void dbc_remove_command_file(phase_static_area *psa)
{
	char_ptr_t	errmsg;
	int		rc, save_errno;

	assert(NULL != psa && NULL == psa->tcfp);		/* Must be closed */
	if (!psa->tmp_file_names_gend)
		dbc_gen_temp_file_names(psa);
	rc = remove((char_ptr_t)psa->tmpcmdfile);
	if (-1 == rc && ENOENT != errno)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(15) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("remove()"), CALLFROM,
			  ERR_TEXT, 2, psa->tmpcmdfile_len, psa->tmpcmdfile,
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
}

/* Open result file */
void dbc_open_result_file(phase_static_area *psa)
{
	char_ptr_t	errmsg;
	int		rc, save_errno;

	assert(NULL != psa && NULL == psa->trfp);
	assert(psa->tmp_file_names_gend);
	psa->trfp = Fopen((char_ptr_t)psa->tmprsltfile, "r");
	if (0 == psa->trfp)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DEVOPENFAIL, 2, psa->tmprsltfile_len, psa->tmprsltfile,
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
}

/* Read a record from temporary result file */
uchar_ptr_t dbc_read_result_file(phase_static_area *psa, int rderrmsg, uchar_ptr_t arg)
{
	int    		save_errno;
	char_ptr_t	errmsg;
	char_ptr_t	fgs;
	char		emsg[MAX_FN_LEN + 256];

	assert(NULL != psa && NULL != psa->trfp);
	FGETS((char_ptr_t)psa->rslt_buff, MAX_ZWR_KEY_SZ, psa->trfp, fgs);
	if (NULL == fgs)
	{
		if (!feof(psa->trfp))
		{	/* Non-EOF message */
			save_errno = errno;
			errmsg = STRERROR(save_errno);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fgets()"), CALLFROM,
				  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
		} else
		{	/* We have EOF */
			if (0 != rderrmsg)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) rderrmsg, 2, RTS_ERROR_TEXT((char_ptr_t)arg));
			else
			{
				strcpy(emsg, "Temporary results file (");
				strcat(emsg, (char_ptr_t)psa->tmprsltfile);
				strcat(emsg, " had unexpected values");
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_PREMATEOF, 0, ERR_TEXT, 2,
					  RTS_ERROR_TEXT(emsg));
			}
		}
		exit(EXIT_FAILURE);	/* We shouldn't come here but in case... */
	}
	return (uchar_ptr_t)fgs;
}

/* Close the temporary command file */
void dbc_close_result_file(phase_static_area *psa)
{
	assert(NULL != psa && NULL != psa->trfp);
	fclose(psa->trfp);
	psa->trfp = NULL;
}

/* Remove/dalete result file */
void dbc_remove_result_file(phase_static_area *psa)
{
	int		rc, save_errno;
	char_ptr_t	errmsg;

	assert(NULL != psa && NULL == psa->trfp);		/* Must be closed */
	if (!psa->tmp_file_names_gend)
		dbc_gen_temp_file_names(psa);
	rc = remove((char_ptr_t)psa->tmprsltfile);
	if (-1 == rc && ENOENT != errno)
	{
		save_errno = errno;
		errmsg = STRERROR(save_errno);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(15) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("remove()"), CALLFROM,
			  ERR_TEXT, 2, psa->tmprsltfile_len, psa->tmprsltfile,
			  ERR_TEXT, 2, RTS_ERROR_TEXT(errmsg));
	}
}

/* Create the temporary file names we need. This is a hardcoded prefix following by the region name
   of the file we are processing. This should create a unique temporary command script and command
   result file for a given invocation and allows multiple copies of DBCERTIFY to run against databases
   in the same directory.
*/
void dbc_gen_temp_file_names(phase_static_area *psa)
{
	unsigned char	*cp, *regname_p;
	int		len, dir_len;

	assert(NULL != psa && !psa->tmp_file_names_gend);
	assert(0 == psa->tmprsltfile[0]);
	assert(0 == psa->tmpcmdfile[0]);
	/* See if we have region name information yet. Where this region name information is
	   kept depends on the phase we are in. Scan phase it is in psa->regname (is an argument
	   to the phase). In certify phase the region name is in the scan phase outfile file header.
	*/
	if (psa->phase_one)
		/* Scan phase, region name in regname */
		regname_p = psa->regname;
	else
		regname_p = (uchar_ptr_t)psa->ofhdr.regname;
	assert(0 != *regname_p);	/* We should have a regname of substance */

	/* Temp command file name: <tempfiledir>TMPFILEPFX_<regionname>.TMPFILESFX. Note that
	   tempfiledir has no default if not specified therefore defaults to the current directory.
	*/
	cp = psa->tmpcmdfile;
	if (0 != *psa->tmpfiledir)
	{	/* A temporary file directory was specified .. use it */
		len = STRLEN((char_ptr_t)psa->tmpfiledir);
		memcpy(cp, psa->tmpfiledir, len);
		cp += len;
#ifdef VMS
		if (']' != *(cp - 1) && '>' != *(cp - 1) && ':' != *(cp - 1))
			*cp++ = ':';
#else
		if ('/' != *(cp - 1))
			*cp++ = '/';
#endif
		dir_len = (int)(cp - psa->tmpcmdfile);
	} else
		dir_len = 0;
	MEMCPY_LIT(cp, TMPFILEPFX);
	cp = psa->tmpcmdfile + SIZEOF(TMPFILEPFX) - 1;
	*cp++ = '_';
	len = STRLEN((char_ptr_t)regname_p);
	memcpy(cp, regname_p, len);
	cp += len;
	MEMCPY_LIT(cp, TMPCMDFILSFX);
	cp += SIZEOF(TMPCMDFILSFX) - 1;
	psa->tmpcmdfile_len = (int)(cp - psa->tmpcmdfile);
	*cp = '\0';	/* Null terminate */

	/* Now same thing for temporary results file */
	cp = psa->tmprsltfile;
	if (0 != dir_len)
	{	/* Dir will be same as for command file so use that.. */
		memcpy(cp, psa->tmpcmdfile, dir_len);
		cp += dir_len;
	}
	MEMCPY_LIT(cp, TMPFILEPFX);
	cp = psa->tmprsltfile + SIZEOF(TMPFILEPFX) - 1;
	*cp++ = '_';
	len = STRLEN((char_ptr_t)regname_p);
	memcpy(cp, regname_p, len);
	cp += len;
	MEMCPY_LIT(cp, TMPRSLTFILSFX);
	cp += SIZEOF(TMPRSLTFILSFX) - 1;
	psa->tmprsltfile_len = (int)(cp - psa->tmprsltfile);
	*cp = '\0';	/* Null terminate */


	psa->tmp_file_names_gend = TRUE;
}

/* Execute the given system command. Note even in VMS we are getting a POSIX style return code, not the
   VMS style return code due to the _POSIX_EXIT macro defined at the top of this module. See the C library
   reference manual for details on VMS.
 */
int dbc_syscmd(char_ptr_t cmdparm)
{
	int		rc;
#ifdef _BSD
	union wait	wait_stat;
#else
	int4		wait_stat;
#endif

#ifdef VMS
	/* Verify system() is supported */ /* BYPASSOK: system() used insode the comment, no SYSTEM() needed */
	if (0 == SYSTEM(NULL))
		GTMASSERT;
#endif
	rc = SYSTEM(cmdparm);
	if (-1 == rc)
		rc = errno;
	else
	{
#ifdef _BSD
		wait_stat.w_status = rc;
#else
		wait_stat = rc;
#endif
		rc = WEXITSTATUS(wait_stat);
	}
	return rc;
}

/* Find the region name associated with the given database.

   Parse the output from "DSE /REG" to determine.
*/
void dbc_find_database_filename(phase_static_area *psa, uchar_ptr_t regname, uchar_ptr_t dbfn)
{
	int		len;
	uchar_ptr_t	rptr;

	dbc_open_command_file(psa);
#ifdef VMS
	strcpy((char_ptr_t)psa->util_cmd_buff, RESULT_ASGN);
	strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->tmprsltfile);
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
	dbc_write_command_file(psa, DSE_START);
#else
	strcpy((char_ptr_t)psa->util_cmd_buff, DSE_START_PIPE_RSLT1);
	strcat((char_ptr_t)psa->util_cmd_buff, (char_ptr_t)psa->tmprsltfile);
	strcat((char_ptr_t)psa->util_cmd_buff, DSE_START_PIPE_RSLT2);
	dbc_write_command_file(psa, (char_ptr_t)psa->util_cmd_buff);
#endif
	dbc_write_command_file(psa, DSE_FIND_REG_ALL);
	dbc_write_command_file(psa, DSE_QUIT);
	UNIX_ONLY(dbc_write_command_file(psa, "EOF"));
	dbc_close_command_file(psa);
	dbc_remove_result_file(psa);
	dbc_run_command_file(psa, "DSE", DSE_FIND_REG_ALL, TRUE);

	dbc_open_result_file(psa);
	/* There should be 7 lines of uninteresting stuff before we get to the file/region pairs but since other errors may
	   pop up at the head of this list (notorious example -- waiting for ftok semaphore from DSE) we will just eat lines
	   until we get to the first DSE prompt at which point our command should be outputing. If we run out of lines (hit
	   EOF), the read routine will return an appropriate error.
	*/
	do
	{
		rptr = dbc_read_result_file(psa, 0, NULL);
	} while (0 != MEMCMP_LIT(rptr, DSE_REG_LIST_START));
	/* And two more lines after that are also bogus */
	dbc_read_result_file(psa, 0, NULL);
	dbc_read_result_file(psa, 0, NULL);

	/* Now we get to the triplets of Filename, Regionname, and a blank line for each region in GLD */
	while(1)
	{
		rptr = dbc_read_result_file(psa, ERR_NOREGION, regname);	/* Should have filename */
		if (0 != MEMCMP_LIT(rptr, FILETAB))
			GTMASSERT;
		len = STRLEN(((char_ptr_t)rptr + SIZEOF(FILETAB) - 1)) - 1;
		assert(MAX_FN_LEN >= len);
		assert(len);
		memcpy(dbfn, (rptr + SIZEOF(FILETAB) - 1), len);
		dbfn[len] = 0;
		rptr = dbc_read_result_file(psa, ERR_NOREGION, regname);
		if (0 != MEMCMP_LIT(rptr, REGIONTAB))
			GTMASSERT;
		len = STRLEN((char_ptr_t)rptr + SIZEOF(REGIONTAB) - 1) - 1;
		assert(len);
		*(rptr + SIZEOF(REGIONTAB) - 1 + len) = 0;	/* Get rid of trailing \n */
		if (0 == strcmp(((char_ptr_t)rptr + SIZEOF(REGIONTAB) - 1), (char_ptr_t)regname))
			break;				/* Found match */
		rptr = dbc_read_result_file(psa, ERR_NOREGION, regname);	/* Ingored blank line */
		len = STRLEN((char_ptr_t)rptr);
		if ('\n' == rptr[len - 1]) --len;	/* Note last record of output file does not have '\n' so test
							   before adjusting 'len' */
		if ('\r' == rptr[len - 1]) --len;	/* Same for carriage return (mostly for VMS) */
		if (0 != len && ((SIZEOF("DSE> ") - 1) != len || 0 != memcmp(rptr, "DSE> ", len)))
			GTMASSERT;
	}
	dbc_close_result_file(psa);
	return;
}

/* Read a given database block into the next block_set (or return existing one) */
int dbc_read_dbblk(phase_static_area *psa, int blk_num, enum gdsblk_type blk_type)
{
	uchar_ptr_t	tucp, src_blk_p;
	int		blk_index;
	block_info	*blk_set_p, *blk_set_new_p;

	assert(0 <= blk_num);
	if (NULL == psa->blk_set)
	{	/* Need a few of these.. */
		psa->blk_set = malloc(SIZEOF(block_info) * MAX_BLOCK_INFO_DEPTH);
		memset(psa->blk_set, 0, SIZEOF(block_info) * MAX_BLOCK_INFO_DEPTH);
		assert(-1 == psa->block_depth);
	}

	DBC_DEBUG(("DBC_DEBUG: Requesting database block 0x%x (type = %d)\n", blk_num, blk_type));
	/* Scan the blocks we have thus far to make sure this block is not amongst them.
	   Block duplication is possible in two instances: (1) We have reached the target
	   block or (2) this is a bitmap block we are reading in to modify. These conditions
	   are also "asserted".
	*/
	assert(blk_num < psa->dbc_cs_data->trans_hist.total_blks);
	for (blk_index = 0, blk_set_p = &psa->blk_set[0]; blk_index <= psa->block_depth; ++blk_index, ++blk_set_p)
	{
		if (blk_set_p->blk_num == blk_num)
		{	/* This block already exists in our cache */
			assert(0xff == ((v15_blk_hdr_ptr_t)blk_set_p->old_buff)->levl || 0 == blk_index);
			if (gdsblk_gvtroot == blk_type)
			{
				assert(gdsblk_gvtindex == blk_set_p->blk_type);
				/* Override preexisting type to say this is the GVT root block which we
				   did not know before when we read it in.
				*/
				blk_set_p->blk_type = gdsblk_gvtroot;
			}
			DBC_DEBUG(("DBC_DEBUG: Found block in active cache - blk_index %d\n", blk_index));
			blk_set_p->found_in_cache = TRUE;
			return blk_index;
		}
	}

	++psa->block_depth;			/* Going to another level of block usage */
	if (MAX_BLOCK_INFO_DEPTH <= psa->block_depth)
		GTMASSERT;
	blk_set_new_p = &psa->blk_set[psa->block_depth];

	/* See if this block already occupies this or another slot in the "inactive cache" which is any block
	   that was read into a slot >= block_depth but <= block_depth_hwm (our high water mark).
	*/
	for (blk_index = psa->block_depth, blk_set_p = &psa->blk_set[psa->block_depth];
	     blk_index <= psa->block_depth_hwm;
	     blk_index++, blk_set_p++)
	{	/* Note the blk_set_p->blk_num != 0 represents a minor inefficiency in that it forces us to always
		   reload block 0 the first time it is used in a transaction but does not cause any correctness
		   issues. After this initial load, it will be found on subsequent searches by the loop above. The
		   check against 0 prevents us from picking up a block that was not in use before. An alternative
		   method to this check would be to initialize the block numbers of the entire cache to some value
		   (preferrably other than -1 as that value has another meaning [created block]). This is something
		   that could be done in the future if this processing turns out to be burdensome which we fully
		   do not expect to be the case with v5cbsu.m handling the bulk of the ocnversion workload. SE 5/2005.
		*/
		if (blk_num == blk_set_p->blk_num && 0 != blk_set_p->blk_num)
		{	/* Block already exists in this slot so we can avoid I/O. If this is the slot we were going
			   to put the new block in (blk_index == block_depth) then everything is in the right place
			   and we only need minor resets. Else, copy information from found block to current block
			   as necessary
			*/
			if (blk_index > psa->block_depth)
			{	/* Block exists in an older slot. Copy info to new slot */
				dbc_init_blk(psa, blk_set_new_p, blk_set_p->blk_num, gdsblk_read, blk_set_p->blk_len,
					     blk_set_p->blk_levl);
				blk_set_new_p->blk_type = blk_set_p->blk_type;
				memcpy(blk_set_new_p->old_buff,
				       ((gdsblk_read == blk_set_p->usage) ? blk_set_p->old_buff : blk_set_p->new_buff),
				       psa->dbc_cs_data->blk_size);
				blk_set_p->blk_num = -1;	/* Effectively invalidate this (now) older cache entry */
				DBC_DEBUG(("DBC_DEBUG: Found block in inactive cache differemt slot (%d) for blk_index %d\n",
					   blk_index, psa->block_depth));
			} else
			{
				assert(blk_index == psa->block_depth);
				switch(blk_set_new_p->usage)
				{
					case gdsblk_create:
					case gdsblk_update:
						/* Both of these have the current value for the buffer in new_buff.
						   Swap new_buff and old_buff to get things into proper order */
						assert(blk_set_p->old_buff);
						assert(blk_set_p->new_buff);
						tucp = blk_set_p->old_buff;
						blk_set_p->old_buff = blk_set_p->new_buff;
						blk_set_p->new_buff = tucp;
						/* Fall into code for block that was only read (it is all setup already) */
					case gdsblk_read:
						/* If block was not disturbed, the buffer pointers are already in the
						   correct configuration */
						dbc_init_blk(psa, blk_set_p, blk_set_p->blk_num, gdsblk_read, blk_set_p->blk_len,
							     blk_set_p->blk_levl);
						DBC_DEBUG(("DBC_DEBUG: Found block in inactive cache same slot for blk_index"
							   " %d\n", psa->block_depth));
						break;
					default:
						GTMASSERT;
				}
			}
			VMS_ONLY(GET_ULONG(blk_set_new_p->tn, &((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->tn));
			UNIX_ONLY(blk_set_new_p->tn = ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->tn);
			blk_set_new_p->found_in_cache = TRUE;
			return psa->block_depth;
		}
	}

	/* Initialize block we are about to use. Some values not yet known until after read so this serves mainly
	   to make sure all the structures (such as read buffer) are allocated.
	*/
	dbc_init_blk(psa, blk_set_new_p, blk_num, gdsblk_read, 0, 0);
	/* Now read the block */
	DBC_DEBUG(("DBC_DEBUG: Reading in database block 0x%x into blk_index %d\n", blk_num, psa->block_depth));
	psa->fc->op = FC_READ;
	psa->fc->op_buff = (sm_uc_ptr_t)blk_set_new_p->old_buff;
	psa->fc->op_pos = psa->dbc_cs_data->start_vbn + ((gtm_int64_t)(psa->dbc_cs_data->blk_size / DISK_BLOCK_SIZE) * blk_num);
	psa->fc->op_len = psa->dbc_cs_data->blk_size;	/* In case length field was modified during a file-extension */
	dbcertify_dbfilop(psa);				/* Read data/index block (no return if error) */
	/* Now that we know some value, call initialize again to set the values the way we want */
	dbc_init_blk(psa, blk_set_new_p, blk_num, gdsblk_read, ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->bsiz,
		     ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->levl);
	VMS_ONLY(GET_ULONG(blk_set_new_p->tn, &((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->tn));
	UNIX_ONLY(blk_set_new_p->tn = ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->tn);
	/* recalculate block type if necessary */
	switch(blk_type)
	{
		case gdsblk_gvtroot:
		case gdsblk_gvtindex:
		case gdsblk_gvtleaf:
		case gdsblk_dtroot:
		case gdsblk_dtindex:
		case gdsblk_dtleaf:
		case gdsblk_bitmap:
			blk_set_new_p->blk_type = blk_type;
			break;
		case gdsblk_gvtgeneric:
			if (0 == ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->levl)
				blk_set_new_p->blk_type = gdsblk_gvtleaf;
			else
				blk_set_new_p->blk_type = gdsblk_gvtindex;
			break;
		case gdsblk_dtgeneric:
			if (0 == ((v15_blk_hdr_ptr_t)blk_set_new_p->old_buff)->levl)
				blk_set_new_p->blk_type = gdsblk_dtleaf;
			else
				blk_set_new_p->blk_type = gdsblk_dtindex;
			break;
		default:
			GTMASSERT;
	}
	return psa->block_depth;
}

/* Find the end of a key using the current value of the key as a base */
void dbc_find_key(phase_static_area *psa, dbc_gv_key *key, uchar_ptr_t rec_p, int blk_levl)
{
	int		cmpc, rec_len;
	int		tmp_cmpc;
	unsigned short	us_rec_len;
	uchar_ptr_t	key_targ_p, key_src_p;

	cmpc = EVAL_CMPC((rec_hdr *)rec_p);
	GET_USHORT(us_rec_len, &((rec_hdr_ptr_t)rec_p)->rsiz);
	rec_len = us_rec_len;
	if (BSTAR_REC_SIZE == rec_len && 0 < blk_levl)
	{	/* This is a star key record .. there is no key */
		key->end = 0;
		DBC_DEBUG(("DBC_DEBUG: Found star key record in dbc_find_key\n"));
		return;
	}
	/* Loop till we find key termination */
	key_targ_p = key->base + cmpc;
	key_src_p = rec_p + SIZEOF(rec_hdr);
	while (1)
	{
		for (; *key_src_p; ++key_targ_p, ++key_src_p)
			*key_targ_p = *key_src_p;
		if (0 == *(key_src_p + 1))
		{	/* Just found the end of the key */
			*key_targ_p++ = 0;		/* Create key ending null */
			*key_targ_p = 0;		/* Install 2nd 0x00 as part of key but not part of length */
			key->end = (uint4)(key_targ_p - key->base);
			break;
		}
		/* Else, copy subscript separator char and keep scanning */
		*key_targ_p++ = *key_src_p++;
		assert((key_src_p - rec_p) < rec_len);	/* Sanity check */
	}
	assert(cmpc <= key->end);		       	/* Overrun sanity check */
	return;
}

/* Find the gvt root block by looking up the GVN in the directory tree */
int dbc_find_dtblk(phase_static_area *psa, dbc_gv_key *key, int min_levl)
{
	uchar_ptr_t		rec_p;
	int			blk_index;

	assert(MAX_MIDENT_LEN >= key->gvn_len);
	dbc_init_key(psa, &psa->gvn_key);
	memcpy(psa->gvn_key, key, SIZEOF(dbc_gv_key) + key->gvn_len);	/* Make key with GVN only (including trailing null) */
	psa->gvn_key->end = key->gvn_len;
	/* Look up GVN in directory tree */
	blk_index = dbc_find_record(psa, psa->gvn_key, (psa->phase_one ? 0 : 1), min_levl, gdsblk_dtroot, FALSE);
	return blk_index;
}

/* Find the record in a given block
   rc = -1   : integrity error detected
      = -2   : record not found
      = else : the index of the block in the cache where record was found (curr_blk_key is set for matching record).
   Note since this routine is used in certify phase to lookup all the right hand siblings of a gvtroot block given a
   maximum key, this flag tells us that failure is ok .. we just wanted to populate the cache with siblings.
*/
int dbc_find_record(phase_static_area *psa, dbc_gv_key *key, int blk_index, int min_levl, enum gdsblk_type newblk_type,
		    boolean_t fail_ok)
{
	uchar_ptr_t	rec_p, blk_p, blk_top, key1, key2;
	unsigned short	us_rec_len;
	int		blk_ptr, blk_levl, key_len, key_len1, key_len2, rec_len;
	int		tmp_cmpc;
	enum gdsblk_type blk_type;
	block_info	*blk_set_p;

	DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Beginning scan of block index %d\n", blk_index));
	/* If blk_index is 0, there is no starting block in the cache/set so we read block 1 (root) */
	assert(0 <= min_levl);
	blk_type = newblk_type;
	if (gdsblk_dtroot == blk_type)
	{
		assert((psa->phase_one && 0 == blk_index) || (!psa->phase_one && 1 == blk_index));
		assert((psa->phase_one && -1 == psa->block_depth) || (!psa->phase_one && 0 == psa->block_depth));
		blk_index = dbc_read_dbblk(psa, 1, gdsblk_dtroot);
		blk_type = gdsblk_dtgeneric;		/* Type of future blocks */
	} else if (gdsblk_gvtroot == blk_type)
		blk_type = gdsblk_gvtgeneric;		/* Type of future read blocks */
	blk_set_p = &psa->blk_set[blk_index];
	blk_levl = ((v15_blk_hdr_ptr_t)blk_set_p->old_buff)->levl;

	/* If we have reached the minimum level, we are done but ONLY if this is also our target block (blk_index == 0).
	   If we are not at blk_index 0 then we need to find where our key fits into this block. This is typcially
	   in the first search of the directory tree where min_levl is 0 but when we hit the dtleaf block we still
	   want to search it to find the pointer to the gvt root block.
	*/
	if (min_levl == blk_levl && 0 == blk_index)
	{	/* This is the level we were looking for and record is found */
		DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Reached minimum block level and found block we were looking for\n"));
		return blk_index;
	}
	/* Find first block that is greater than or equal to our gvn or if we find a star-key */
	blk_p = blk_set_p->old_buff;
	rec_p = blk_p + SIZEOF(v15_blk_hdr);
	blk_top = blk_p + ((v15_blk_hdr_ptr_t)blk_set_p->old_buff)->bsiz;
	while (rec_p < blk_top)
	{
		blk_set_p->prev_match = blk_set_p->curr_match;
		blk_set_p->curr_match = 0;
		GET_USHORT(us_rec_len, &((rec_hdr *)rec_p)->rsiz);
		rec_len = us_rec_len;
		if (0 >= rec_len || rec_len > psa->dbc_cs_data->max_rec_size)
			/* Something messed up integrity wise - matters for phase-2 but not for phase-1 */
			return -1;
		if (0 != blk_levl && BSTAR_REC_SIZE == rec_len)
		{	/* We have a star record - This is the record we are looking for in this block */
			blk_set_p->curr_blk_key->end = 0;			/* Key length is zero for this type */
			if (min_levl == blk_levl)
			{	/* Block record and key information set up. We can return now */
				DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Reached minimum block level -- matching scan was a star"
					   " key record\n"));
				return blk_index;
			}
			DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Recursing down a level via star key record at offset 0x%lx\n",
				   (rec_p - blk_p)));
			GET_ULONG(blk_ptr, rec_p + VMS_ONLY(3) UNIX_ONLY(4));
			blk_index = dbc_read_dbblk(psa, blk_ptr, blk_type);
			/* Keep looking next level down */
			return dbc_find_record(psa, key, blk_index, min_levl, blk_type, fail_ok);
		}
		/* Determine key for this record */
		dbc_find_key(psa, blk_set_p->curr_blk_key, rec_p, blk_set_p->blk_levl);
		/* Perform key comparison keeping track of how many chars match (compression count) */
		if (dbc_match_key(blk_set_p->curr_blk_key, blk_set_p->blk_levl,
				  key, &blk_set_p->curr_match))
		{	/* Found our record - If the record is in an index block, recurse. Else return the record we found */
			if (gdsblk_gvtleaf == blk_set_p->blk_type || min_levl == blk_levl)
			{	/* This is a terminal block. It is the end of the road */
				DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Reached minimum block level (or leaf level) -- matching"
					   " scan was a normal keyed record at offset 0x%lx\n", (rec_p - blk_p)));
				return blk_index;
			}
			/* We already know that the current block is not the one we are interested in and therefore
			   this record is known to contain a pointer to another block. Read the block in and
			   recurse to continue the search for the key.
			*/
			DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Recursing down a level via keyed index record at offset 0x%lx\n",
				   (rec_p - blk_p)));
			GET_ULONG(blk_ptr, (rec_p + SIZEOF(rec_hdr) + blk_set_p->curr_blk_key->end
					   - EVAL_CMPC((rec_hdr *)rec_p) + 1));
			blk_index = dbc_read_dbblk(psa, blk_ptr, blk_type);
			return dbc_find_record(psa, key, blk_index, min_levl, blk_type, fail_ok);
		}
		/* We want to be able to find the previous record */
		blk_set_p->prev_rec = rec_p;
		memcpy(blk_set_p->prev_blk_key, blk_set_p->curr_blk_key,
		       (SIZEOF(dbc_gv_key) + blk_set_p->curr_blk_key->end));
		rec_p += rec_len;					/* Point to next record in block */
		blk_set_p->curr_rec = rec_p;
	}
	/* If we don't find the record (or one greater), the block with the key we are looking for is no
	   longer existing in the GVT (assert that the search is for a level 0 GVT block).
	*/
	if (gdsblk_gvtleaf == psa->blk_set[0].blk_type)
	{	/* Key not found */
		DBC_DEBUG(("DBC_DEBUG: dbc_find_record: Searched for key was not found\n"));
		return -2;
	}
	/* Else we should have found the record or a star key. Globals are NOT removed once created so we
	   should always be able to find an appropriate record if this is not a GVT leaf block. Exception to this
	   is when we are just wanting to populate the right siblings of a gvtroot block in the cache.
	*/
	if (!fail_ok)
	{
		assert(FALSE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DBCINTEGERR, 2, RTS_ERROR_TEXT((char_ptr_t)psa->ofhdr.dbfn),
			  ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to find index record for an existing global"));
	}
	return -1;
}

/* Compare two keys. If key1 is logically greater than or equal to key2, return TRUE, else FALSE.
   Also, set output parameter matchc with the number of chars that matched */
boolean_t dbc_match_key(dbc_gv_key *key1, int blk_levl1, dbc_gv_key *key2, unsigned int *matchc)
{
	uchar_ptr_t 	key_val1, key_val2;
	int		key_len1, key_len2, key_len, lcl_matchc;

	lcl_matchc = 0;
	key_val1 = key1->base;
	key_val2 = key2->base;
	key_len1 = key1->end + 1;
	if (1 == key_len1 && 0 < blk_levl1)
	{	/* This is a star key record. It is always greater than the second key but
		   has no matching characters.
		*/
		*matchc = 0;
		return TRUE;
	}
	assert(1 < key_len1); 		/* Otherwise we expect to see a key here */
	key_len2 =  key2->end + 1;
	assert(1 < key_len2);		/* Not expecting a star key record in second position */
	key_len = MIN(key_len1, key_len2);
	for (; key_len; key_val1++, key_val2++, key_len--)
	{
		if (*key_val1 == *key_val2)
			++lcl_matchc;
		else
			break;
	}
	*matchc = lcl_matchc;
	if ((0 == key_len && key_len1 >= key_len2) || (0 != key_len && *key_val1 > *key_val2))
		return TRUE;
	return FALSE;
}

/* Initialize the given gv_key to a null status so dbc_find_key() start off with a new key */
void dbc_init_key(phase_static_area *psa, dbc_gv_key **key)
{
	if (NULL == *key)
	{	/* Need a key allocated for this block */
		*key = malloc(SIZEOF(dbc_gv_key) + psa->dbc_cs_data->max_key_size);
		(*key)->top = (uint4)(SIZEOF(dbc_gv_key) + psa->dbc_cs_data->max_key_size);
	}
	(*key)->end  = (*key)->gvn_len = 0;
	return;
}

/* Routine to initialize a blk_set block */
void dbc_init_blk(phase_static_area *psa, block_info *blk_set_p, int blk_num, enum gdsblk_usage blk_usage, int blk_len,
		  int blk_levl)
{
	blk_set_p->blk_num = blk_num;
	blk_set_p->usage = blk_usage;
	blk_set_p->ins_rec.blk_id = 0;
	blk_set_p->found_in_cache = FALSE;
	dbc_init_key(psa, &blk_set_p->curr_blk_key);
	dbc_init_key(psa, &blk_set_p->prev_blk_key);
	dbc_init_key(psa, &blk_set_p->ins_rec.ins_key);
	blk_set_p->blk_len = blk_len;
	blk_set_p->blk_levl = blk_levl;
	blk_set_p->curr_match = blk_set_p->prev_match = 0;
	if (NULL == blk_set_p->old_buff)
		blk_set_p->old_buff = malloc(psa->dbc_cs_data->blk_size);
	if (NULL == blk_set_p->new_buff)
		blk_set_p->new_buff = malloc(psa->dbc_cs_data->blk_size);
	blk_set_p->curr_rec = blk_set_p->prev_rec = blk_set_p->old_buff + SIZEOF(v15_blk_hdr);
	blk_set_p->upd_addr = NULL;
	blk_set_p->ins_blk_id_p = NULL;
	return;
}

/* Initialize database usage - open it and read in the file-header */
void dbc_init_db(phase_static_area *psa)
{	/* This routine does the database open and initialization for both scan (phase1) and certify
	   phases however the requirements for these phases differ somewhat. For the scan phase we
	   just want to open the database, read the file-header into dbc_cs_data and verify this is a
	   database. For the certify phase, we also have to obtain standalone access, make sure the file
	   hasn't moved around since the scan phase and add reserved_bytes and max_rec_size checks. But
	   standalone access is somewhat tricky as part of the standalone verification process involves
	   (for later V4 versions) knowing the key of the shared memory segment which is kept in the
	   file-header meaning we have to read the file-header before we can lock it on UNIX. Because
	   of this, on UNIX we will RE-READ the file-header after obtaining the lock to make sure there
	   are no stale values.
	*/

	/* We must have RW access in scan phase in order to do the DSE buffer flush to assure we are looking
	   at the correct database blocks and of course the certify phase needs R/W access to do its job.
	*/
	if (0 != ACCESS((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname, (R_OK | W_OK)))
	{
		if (EACCES == errno)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(psa->dbc_gv_cur_region));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBOPNERR, 2, DB_LEN_STR(psa->dbc_gv_cur_region), errno);
	}
	/* Open the database which on VMS gives standalone access (for phase 2) */
	psa->fc->op = FC_OPEN;
	dbcertify_dbfilop(psa);		/* Knows this is a phase 2 open so gives standalone access on VMS */
	psa->dbc_gv_cur_region->open = TRUE;

	if (!psa->phase_one)
	{	/* Verify the fileid has not changed */
		if (!is_gdid_gdid_identical(&psa->ofhdr.unique_id.uid,
					    &FILE_INFO(psa->dbc_gv_cur_region)->UNIX_ONLY(fileid)VMS_ONLY(file_id)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBCNOTSAMEDB);
	}

	/* Read in database file header */
	psa->fc->op = FC_READ;
	psa->fc->op_buff = (sm_uc_ptr_t)psa->dbc_cs_data;
	psa->fc->op_len = SIZEOF(*psa->dbc_cs_data);
	psa->fc->op_pos = 1;
	dbcertify_dbfilop(psa);

	/* Verify we (still) have a GT.M V4 database */
	if (0 != memcmp(psa->dbc_cs_data->label, V15_GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (memcmp(psa->dbc_cs_data->label, V15_GDS_LABEL, GDS_LABEL_SZ - 3))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBNOTGDS, 2, RTS_ERROR_TEXT((char_ptr_t)psa->ofhdr.dbfn));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADDBVER, 2, RTS_ERROR_TEXT((char_ptr_t)psa->ofhdr.regname));
	}

	if (!psa->phase_one)
	{
#ifdef UNIX
		/* Obtain standalone access to database.

		In UNIX, this requires creation/access to the "ftok" database semaphore. This is
		the main semaphore in some V4 releases and the "startup/rundown" semaphore in other
		releases. But in all cases, its creation and locking will govern standalone access
		to the database in question.

		On VMS, we just don't open the file as shared and we are guarranteed standalone
		access to it.
		*/
		dbc_aquire_standalone_access(psa);
		dbcertify_dbfilop(psa);		/* Re-read file header */
#endif
		/* Verify reserved_bytes and max_rec_len again and verify kill_in_prog is not set */
		if (VMS_ONLY(9) UNIX_ONLY(8) > psa->dbc_cs_data->reserved_bytes)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBMINRESBYTES, 2,
					VMS_ONLY(9) UNIX_ONLY(8), psa->dbc_cs_data->reserved_bytes);
		if (SIZEOF(blk_hdr) > (psa->dbc_cs_data->blk_size - psa->dbc_cs_data->max_rec_size))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBMAXREC2BIG, 3,
					psa->dbc_cs_data->max_rec_size, psa->dbc_cs_data->blk_size,
					  (psa->dbc_cs_data->blk_size - SIZEOF(blk_hdr)));
		if (0 != psa->dbc_cs_data->kill_in_prog)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBCKILLIP, 2,
					RTS_ERROR_TEXT((char_ptr_t)psa->ofhdr.dbfn));
		/* Turn off replication and/or journaling for our trip here */
		if (jnl_open == psa->dbc_cs_data->jnl_state)
		{
			psa->dbc_cs_data->jnl_state = jnl_closed;
			psa->dbc_cs_data->repl_state = repl_closed;
		}
	}
	return;
}

/* Close the database and remove any semaphores we had protecting it */
void dbc_close_db(phase_static_area *psa)
{
	if (psa->dbc_gv_cur_region->open)
	{	/* If region is open.. close it and release the semaphores.. */
		psa->fc->op = FC_CLOSE;
		dbcertify_dbfilop(psa);
		psa->dbc_gv_cur_region->open = FALSE;
		UNIX_ONLY(if (!psa->phase_one) dbc_release_standalone_access(psa));
	}
	return;
}

#ifdef UNIX
/* Aquire semaphores that on on all V4.x releases are the access control semaphores. In pre V4.2 releases
   they were based on an FTOK of the database name with an ID of '1'. In V4.2 and later, they are based on
   the FTOK of the database name with an ID of '43'. Since we do not know which flavor of database we are
   dealing with, we must create and acquire both flavors of semaphore and hold them for the duration of
   the phase 2 run. But just holding these semaphore is not sufficient to guarrantee standalone access. We
   also must attempt to attach to the shared memory for the segment. If it is found, standalone access
   is not achieved. Early V4 versions (prior to V4.2) created the shared memory with the same FTOK id as the
   semaphore. Later versions would have had the key of the created private section in the file-header. Use
   both approaches and fail our attempt if either succeeds.
*/
void dbc_aquire_standalone_access(phase_static_area *psa)
{
	mu_all_version_get_standalone((char_ptr_t)psa->dbc_gv_cur_region->dyn.addr->fname, &psa->sem_inf[0]);
}

void dbc_release_standalone_access(phase_static_area *psa)
{
	mu_all_version_release_standalone(&psa->sem_inf[0]);
}
#endif
