/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_fcntl.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include <errno.h>
#include <libgen.h>	/* needed for basename */
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_permissions.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "db_header_conversion.h"
#include "filestruct.h"
#include "gtmio.h"
#include "mupipbckup.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "sleep_cnt.h"
#include "util.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gtm_file_stat.h"
#include "shmpool.h"
#include "wcs_phase2_commit_wait.h"
#include "wbox_test_init.h"
#include "db_write_eof_block.h"
#include "mupip_exit.h"

#include "mu_outofband_setup.h"
#include "wcs_flu.h"
#include "jnl.h"
#ifdef __x86_64
#	include <math.h>
#	include <dlfcn.h>
#	define BUF_MAX		100
#	define MIN_ETA		10
#	define SHOWPERCENT	24
#	define ADJUSTED_ETA	5 /4	/* not using 1.25 to avoid [bugprone-integer-division] warning */
#	define MEGABYTE 	1024 * 1024
#	define GIGABYTE 	1024 * MEGABYTE
#endif

#define	TMPDIR_ACCESS_MODE	R_OK | W_OK | X_OK
#define	TMPDIR_CREATE_MODE	S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
#define	COMMAND_ARRAY_SIZE	1024
#define	MV_CMD			"mv "
#define	RMDIR_CMD		"rm "
#define	RMDIR_OPT		"-r "
#define	CD_CMD			"cd "
#define	CMD_SEPARATOR		" && "
#define CAN_RETRY 		1
#define CANNOT_RETRY		0
#ifdef __linux__
#	define	CP_CMD		"cp "
#	define	CP_OPT		"--sparse=always "
#elif defined(_AIX)
#	define	CP_CMD		"pax "
#	define	CP_OPT		"-r -w "
#else
#	define	CP_CMD		"cp "
#	define	CP_OPT		""
#endif
#define	NUM_CMD			3

#define	FREE_COMMAND_STR_IF_NEEDED		\
{						\
	if (command != &cmdarray[0])		\
	{					\
		free(command);			\
		command = &cmdarray[0];		\
	}					\
}

#define	MAX_TMP_CMD_LEN	((MAX_FN_LEN) * 2 + SIZEOF(UNALIAS))	/* SIZEOF includes 1 byte for null terminator too */

#define	CLEANUP_AND_RETURN_FALSE(RETRY)								\
{												\
	int	rc;										\
	int4	rv2, tmpcmdlen;									\
	char	tmpcmd[MAX_TMP_CMD_LEN];							\
												\
	if (CANNOT_RETRY == RETRY)								\
	{											\
		if (online)									\
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;				\
		*stopretries = TRUE;								\
	}											\
	FREE_COMMAND_STR_IF_NEEDED;								\
	if (FD_INVALID != backup_fd)								\
		CLOSEFILE_RESET(backup_fd, rc);	/* resets "backup_fd" to FD_INVALID */		\
	memset(tmpcmd, '\0', SIZEOF(tmpcmd));							\
	/* An error happened. We are not sure if the temp dir is empty. Can't use rmdir() */	\
	MEMCPY_LIT(tmpcmd, UNALIAS);								\
	tmpcmdlen = STR_LIT_LEN(UNALIAS);							\
	cmdpathlen = STRLEN(fulpathcmd[2]);							\
	memcpy(&tmpcmd[tmpcmdlen], fulpathcmd[2], cmdpathlen);					\
	tmpcmdlen += cmdpathlen;								\
	MEMCPY_LIT(&tmpcmd[tmpcmdlen], RMDIR_OPT);						\
	tmpcmdlen += STR_LIT_LEN(RMDIR_OPT);							\
	/* tempdir[] is not necessarily null-terminated so null terminate it and		\
	 * copy the null byte too into "tmpcmd" before calling SYSTEM.				\
	 */											\
	tempdir[tmpdirlen] = 0;	/* see comment "rm tempdir" for similar code */			\
	memcpy(&tmpcmd[tmpcmdlen], tempdir, tmpdirlen + 1);					\
	tmpcmdlen += tmpdirlen;									\
	rv2 = SYSTEM((char *)tmpcmd);								\
	if (0 != rv2)										\
	{											\
		if (-1 == rv2)									\
		{										\
			save_errno = errno;							\
			errptr = (char *)STRERROR(save_errno);					\
			util_out_print("system : !AZ", TRUE, errptr);				\
		}										\
		util_out_print("Error removing temp dir !AD.", TRUE, tmpcmdlen, tmpcmd);	\
	}											\
	return FALSE;										\
}

GBLREF	bool			file_backed_up;
GBLREF	gd_region		*gv_cur_region;
GBLREF	bool			record;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	bool			online;
GBLREF	uint4			process_id;
GBLREF	boolean_t		debug_mupip;
GBLREF bool                     mu_ctrlc_occurred;

#ifdef __x86_64
STATICDEF void		*func_ptr;
#endif
error_def(ERR_BKUPFILEPERM);
error_def(ERR_BKUPPROGRESS);
error_def(ERR_BCKUPBUFLUSH);
error_def(ERR_BUFFLUFAILED);
error_def(ERR_COMMITWAITSTUCK);
error_def(ERR_DBCCERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_ERRCALL);
error_def(ERR_TEXT);
error_def(ERR_TMPFILENOCRE);
error_def(ERR_BACKUPDBFILE);
error_def(ERR_BACKUPTN);
error_def(ERR_FILENAMETOOLONG);

boolean_t	mubfilcpy (backup_reg_list *list, boolean_t showprogress, int attemptcnt, boolean_t *stopretries)
{
	mstr			*file, tempfile;
	unsigned char		cmdarray[COMMAND_ARRAY_SIZE], *command = &cmdarray[0];
	char 			fulpathcmd[NUM_CMD][MAX_FN_LEN] = {{CP_CMD}, {MV_CMD}, {RMDIR_CMD}};
	sgmnt_data_ptr_t	header_cpy;
	int4			backup_fd = FD_INVALID, counter, hdrsize, rsize, ntries;
	ssize_t			status;
	block_id		blk_num;
	int4			cmdlen, rv, save_errno, tempfilelen, tmpdirlen, tmplen;
	int4			sourcefilelen, sourcedirlen, realpathlen;
	struct stat		stat_buf, stat;
	off_t			filesize, offset;
	char 			*inbuf, *ptr, *errptr, *sourcefilename, *sourcedirname;
	char			tempfilename[MAX_FN_LEN + 1];
	char			tempdir[MAX_FN_LEN + 1], prefix[MAX_FN_LEN];
	char			tmpsrcfname[MAX_FN_LEN], tmpsrcdirname[MAX_FN_LEN], realpathname[PATH_MAX];
	char			sourcefilepathname[GTM_PATH_MAX + MAX_FN_LEN], tempfilepathname[GTM_PATH_MAX + MAX_FN_LEN];
	char 			tmprealpath[GTM_PATH_MAX + MAX_FN_LEN];
	int			fstat_res, i, cmdpathlen;
	uint4			ustatus, size;
	muinc_blk_hdr_ptr_t	sblkh_p;
	ZOS_ONLY(int		realfiletag;)
	int			user_id;
	int			group_id;
	int			perm;
	struct perm_diag_data	pdd;
	int			ftruncate_res;
	trans_num		ONE = 0x1;
	boolean_t		in_kernel, use_cp_or_pax = TRUE;
#	ifdef __x86_64
	int 			digicnt = 0, eta = MIN_ETA, infd, outfd, speedcnt = 0, transpadcnt = 0;
	int 			csdigicnt = 0, cspadcnt = 0, speedigits = 0;
	double			progper = 0, progfact = SHOWPERCENT;
	size_t	 		tmpsize, transfersize, remaining;
	ssize_t 		ret;
	size_t			bytesspliced, currspeed = 0, trans_cnt = 0, start_cnt = 0;
	time_t			endtm, strtm;
	char 			sizep[BUF_MAX], progperstr[BUF_MAX], padding[MAX_DIGITS_IN_INT8], fil=' ';
	boolean_t		sizeGiB = TRUE;
	char 			*etaptr;
	char 			transferbuf[BUF_MAX], speedbuf[BUF_MAX], errstrbuff[BUF_MAX + GTM_PATH_MAX];
	ssize_t			(*copy_file_range_p)(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out,
						size_t len, unsigned int flags);
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	file = &(list->backup_file);
	file->addr[file->len] = '\0';
	header_cpy = list->backup_hdr;
	hdrsize = (int4)ROUND_UP(SIZEOF_FILE_HDR(header_cpy), DISK_BLOCK_SIZE);
#	ifdef DEBUG /* This code is shared by two WB tests */
	if (WBTEST_ENABLED(WBTEST_MM_CONCURRENT_FILE_EXTEND) ||
		(WBTEST_ENABLED(WBTEST_WSSTATS_PAUSE) && (10 == ydb_white_box_test_case_count)))
		if (!MEMCMP_LIT(gv_cur_region->rname, "DEFAULT") && !cs_addrs->nl->wbox_test_seq_num)
		{
			cs_addrs->nl->wbox_test_seq_num = 1;
			while (1 == cs_addrs->nl->wbox_test_seq_num)
				SHORT_SLEEP(1);	/* wait for signal from gdsfilext that mupip backup can continue */
		}
#	endif
	/* the temporary file should be located in the destination directory */
	ptr = file->addr + file->len - 1;
	while (('/' != *ptr) && (ptr > file->addr))
		ptr--;
	if (ptr > file->addr)
	{
		memcpy(tempdir, file->addr, ptr - file->addr + 1);
		tempdir[ptr - file->addr + 1] = '\0';
	}
	else
	{
		tempdir[0] = '.';
		tempdir[1] = '\0';
	}
	memset(prefix, 0, MAX_FN_LEN);
	memcpy(prefix, gv_cur_region->rname, gv_cur_region->rname_len);
	/* For testing purposes, we set the pid part of the temp file name to 99999 instead of the
	 * pid. This may be necessary if the test is testing for FILENAMETOOLONG errors such as in
	 * the r136/ydb864 test because a process_id with fewer or more digits than expected will
	 * cause the temp file name to be shorter or longer than the test.
	 */
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_YDB_STATICPID))
		SNPRINTF(&prefix[gv_cur_region->rname_len], MAX_FN_LEN - gv_cur_region->rname_len, "_%d_", 99999);
	else
#	endif
		SNPRINTF(&prefix[gv_cur_region->rname_len], MAX_FN_LEN - gv_cur_region->rname_len, "_%d_", process_id);
	/* verify that we have access to the temporary directory to avoid /tmp */
	if (0 != ACCESS(tempdir, TMPDIR_ACCESS_MODE))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("access : !AZ", TRUE, errptr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		util_out_print("ERROR: Do NOT have full access to directory !AD", TRUE,
			       LEN_AND_STR(tempdir));
		return FALSE;
	}
	/* verify that we have write access to the destination backup file */
	if (0 == ACCESS(file->addr, F_OK) && (0 != ACCESS(file->addr, W_OK)))
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_BKUPFILEPERM, 2, file->len, file->addr);
		if (online)
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		return FALSE;
	}
	/* make this cp a two step process ==> cp followed by mv */
	ntries = 0;
	fstat_res = FILE_NOT_FOUND;
	/* do go into the loop for the first time, irrespective of fstat_res*/
	while ((FILE_NOT_FOUND != fstat_res) || (!ntries))
	{
		int	len;

		len = SNPRINTF(tempfilename, SIZEOF(tempfilename), "%s%s%d.tmp", tempdir, prefix, ntries);
		if (SIZEOF(tempfilename) <= len)
		{
			char	tmpbuff[MAX_FN_LEN * 2];

			len = SNPRINTF(tmpbuff, SIZEOF(tmpbuff), "%s%s%d.tmp", tempdir, prefix, ntries);
			assert(SIZEOF(tmpbuff) > len);
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FILEPARSE, 2, len, tmpbuff);
			mubclnup(list, need_to_del_tempfile);
			mupip_exit(ERR_FILENAMETOOLONG);
		}
		tempfile.addr = tempfilename;
		tempfile.len = STRLEN(tempfilename);
		if ((FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempfile, NULL, NULL, FALSE, &ustatus))) ||
		    (ntries > MAX_TEMP_OPEN_TRY))
		{
			if (FILE_STAT_ERROR != fstat_res)
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr,
					   ERR_TEXT, 2, LEN_AND_LIT("Tried a maximum number of times, clean-up temporary files " \
								    "in backup directory and retry."));
			else
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_TMPFILENOCRE, 2, tempfile.len, tempfile.addr,
						ustatus);
			return FALSE;
		}
		ntries++;
	}
	/* basename() may modify the argument passed to it. so pass it a temp string */
	memcpy(tmpsrcfname, gv_cur_region->dyn.addr->fname, gv_cur_region->dyn.addr->fname_len);
	tmpsrcfname[gv_cur_region->dyn.addr->fname_len] = 0;
	sourcefilename = basename((char *)tmpsrcfname);
	sourcefilelen = STRLEN(sourcefilename);
	/* dirname() may modify its argument too. Also, basename() above may've m odified tmp str. reset it */
	memcpy(tmpsrcdirname, gv_cur_region->dyn.addr->fname, gv_cur_region->dyn.addr->fname_len);
	tmpsrcdirname[gv_cur_region->dyn.addr->fname_len] = 0;
	sourcedirname = dirname((char *)tmpsrcdirname);
	sourcedirlen = STRLEN(sourcedirname);
	/* Right now, "tempfilename" is the full path to the temp file where the backup will get created.
	 * Save this directory into tempdir, which will later be used to remove the temp file.
	 * We also check to make sure that the temp file will not overflow the tempdir buffer
	 * and throw a FILENAMETOOLONG if necessary.
	 */
	tempfilelen = STRLEN(tempfilename);
	assert(SIZEOF(tempfilename) == SIZEOF(tempdir));
	assert(SIZEOF(tempdir) >= (tempfilelen + 1)); /* +1 because a later line sets tempdir[tmpdirlen] to '\0' */
	memcpy(tempdir, tempfilename, tempfilelen);
	tmpdirlen = tempfilelen;
	/* mkdir tempdir*/
	if (0 != MKDIR(tempfilename, TMPDIR_CREATE_MODE))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("Temporary directory !AD could not be created.", TRUE, tempfilelen, tempfilename);
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errptr));
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	if (NULL == realpath(tempfilename, realpathname))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("Temporary directory !AD could not be found after creation.", TRUE, tempfilelen, tempfilename);
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errptr));
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	realpathlen = STRLEN(realpathname);
	/* Get the path to system utilities and prepend it to the command. */
	for (i = 0; i < NUM_CMD; i++)
	{
		rv = CONFSTR(fulpathcmd[i], MAX_FN_LEN);
		if (0 != rv)
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	/* Check for copy_file_range symbol. Use copy_file_range() in the first attempt   */
#	ifdef __x86_64
	if (1 == attemptcnt)
	{
		use_cp_or_pax = (NULL == (func_ptr = dlsym(RTLD_DEFAULT, "copy_file_range")))
				? TRUE : FALSE; /* Inline assignment */
	}
	if (!use_cp_or_pax)
	{
		mu_outofband_setup();
		if (NULL == realpath((char *)tmpsrcfname, sourcefilepathname))
		{
			SNPRINTF(errstrbuff, GTM_PATH_MAX, "Unable to resolve path to database file %s. "
					"Reason: %s", sourcefilename, (char *)STRERROR(errno));
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errstrbuff));
			if (online)
				cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			return FALSE;
		}
		if (NULL == realpath((char *)tempfilename, tmprealpath))
		{
			SNPRINTF(errstrbuff, GTM_PATH_MAX, "Unable to resolve path to temp file %s. "
					"Reason: %s", tempfilename, (char *)STRERROR(errno));
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errstrbuff));
			if (online)
				cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			return FALSE;
		}
		SNPRINTF(tempfilepathname, SIZEOF(tempfilepathname),"%s/%s", tmprealpath, sourcefilename);
		infd = open(sourcefilepathname, O_RDONLY);
		if (-1 == infd)
		{
			if (1 == handle_err("Unable to open() the database file", errno))
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		if (-1 == fstat(infd, &stat))
		{
			if (1 == handle_err("Error obtaining fstat() from the database file", errno))
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		outfd = open(tempfilepathname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (-1 == outfd)
		{
			if (1 == handle_err("Unable to open() the backup file/location", errno))
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		/* set transfer size as maximum. Full copy-acceleration */
		remaining = stat.st_size;
		tmpsize = transfersize = remaining;
		if (GIGABYTE > tmpsize)
			sizeGiB = FALSE;
		tmpsize = (sizeGiB) ? DIVIDE_ROUND_UP(tmpsize, GIGABYTE) : DIVIDE_ROUND_UP(tmpsize, MEGABYTE);
		digicnt = SNPRINTF(transferbuf, MAX_DIGITS_IN_INT8 + 7, "/ %lld %siB", tmpsize, (sizeGiB ? "G" : "M")) - 6;
		memcpy(sizep, transferbuf, STRLEN(transferbuf) + 1);
		start_cnt = header_cpy->trans_hist.curr_tn;
		if (showprogress)
			util_out_print("Starting BACKUP for region : !AD", TRUE, REG_LEN_STR(gv_cur_region));

		size_t	in_off, out_off, data_off, hole_off;
		in_off = out_off = data_off = hole_off = 0;
		do
		{
			if ((in_off == data_off) || (in_off == hole_off))
			{
				for ( ; ; )
				{
					size_t		ret_off;
					boolean_t	is_seek_data;

					if (in_off == data_off)
					{
						ret_off = hole_off = lseek(infd, data_off, SEEK_HOLE);
						is_seek_data = FALSE;
					} else
					{
						ret_off = data_off = lseek(infd, hole_off, SEEK_DATA);
						is_seek_data = TRUE;
					}
					if ((off_t)-1 == ret_off)
					{
						int	save_errno;
						char	errstr[256];

						save_errno = errno;
						/* ENXIO implies last section of the file is a HOLE. It is not an error.
						 * Just skip copying that tail end HOLE section.
						 */
						if (is_seek_data && (ENXIO == save_errno))
						{
							remaining = 0;
							break;
						}
						SNPRINTF(errstr, SIZEOF(errstr), "lseek(0x%llx, %s)", (unsigned long long)ret_off,
							(!is_seek_data ? "SEEK_HOLE" : "SEEK_DATA"));
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
								ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
						CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
					}
					/* Note that it is possible for concurrent updates during an online backup to write
					 * holes/data past the size of the source file at the start of the backup. Therefore
					 * limit the data/hole analysis to the original size of the source file.
					 */
					if (!is_seek_data)
					{
						if (hole_off > stat.st_size)
							hole_off = stat.st_size;
					} else
					{
						if (data_off > stat.st_size)
							data_off = stat.st_size;
					}
					if (is_seek_data)
					{	/* Skip copying any hole sections in the file */
						in_off = out_off = data_off;
						remaining -= (data_off - hole_off);
						assert(0 <= remaining);
						continue;
					}
					break;
				}
				if (0 == remaining)
					break;
			}
			assert(in_off < stat.st_size);
			assert(out_off < stat.st_size);
			assert(data_off < stat.st_size);
			assert(hole_off <= stat.st_size);
			strtm = time(NULL);
			copy_file_range_p = func_ptr;

			size_t	max_cp_len;
			max_cp_len = hole_off - in_off;
			assert(max_cp_len <= remaining);
			ret = copy_file_range_p(infd, (loff_t *)&in_off, outfd, (loff_t *)&out_off, max_cp_len, 0);
			save_errno = errno;
			if (1 > ret)
			{
				if (EXDEV == save_errno)
					CLEANUP_AND_RETURN_FALSE(CAN_RETRY); /* Silently ignore and switch to cp */
				if (1 == (status = handle_err("Error occurred during the copy phase of MUPIP BACKUP",
						save_errno))) /* WARNING assignment */
					CLEANUP_AND_RETURN_FALSE(CAN_RETRY);
			}
			if (WBTEST_ENABLED(WBTEST_BACKUP_FORCE_SLEEP))
			{
				util_out_print("BACKUP_STARTED", TRUE);
				LONG_SLEEP(20);
				showprogress = TRUE;
			}
			endtm = time(NULL);
			bytesspliced = in_off + (size_t)ret;
			if ((endtm > strtm) && (0 < ret))
				currspeed = DIVIDE_ROUND_UP(ret, (endtm - strtm)); /* bytes per second*/
			remaining = remaining - ret;
			if (showprogress)
			{
				if (0 < currspeed)
					eta = (int)(DIVIDE_ROUND_UP(remaining, currspeed) * ADJUSTED_ETA); /* Increase ETA */
				progper = ((double)bytesspliced / (double)transfersize) * 100;
				if (progper > progfact)
				{
					progfact = progfact + SHOWPERCENT;
					tmpsize = (sizeGiB) ? DIVIDE_ROUND_UP(bytesspliced, GIGABYTE) : DIVIDE_ROUND_UP
							(bytesspliced, MEGABYTE);
					memset(padding, '\0', 3);
					if (100 != (int)progper)
						memset(padding, fil, (10 > progper) ? 2 : 1); /* left pad progress percent string */
					SNPRINTF(progperstr, BUF_MAX, "%s (%s%d%c)", sizep, padding, (int)progper, '%');
					memset(padding, '\0', MAX_DIGITS_IN_INT8);
					transpadcnt = digicnt - countdigits(tmpsize);
					if (transpadcnt > 0)
						memset(padding, fil, transpadcnt); /* left pad transferbuf string */
					SNPRINTF(transferbuf, BUF_MAX, "%s%lld %s", padding, tmpsize, progperstr);
					currspeed = DIVIDE_ROUND_DOWN(currspeed, 1024);
					csdigicnt = countdigits(currspeed);
					speedigits = (csdigicnt > speedigits) ? csdigicnt: speedigits;
					cspadcnt = speedigits - csdigicnt;
					memset(padding, '\0', MAX_DIGITS_IN_INT8);
					if (cspadcnt > 0)
						memset(padding, fil, cspadcnt); /* left pad speedbuf string */
					SNPRINTF(speedbuf, BUF_MAX, "%s%lld", padding, currspeed);
					if (eta > 60)
					{
						eta = DIVIDE_ROUND_UP(eta, 60);
						etaptr = (1 < eta) ? "minutes": "minute";
					}
					else
						etaptr = (1 < eta) ? "seconds": "second";
					trans_cnt = cs_data->trans_hist.curr_tn - start_cnt;
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(10) ERR_BKUPPROGRESS, 8,
						LEN_AND_STR(transferbuf), LEN_AND_STR(speedbuf), &trans_cnt,
						eta, LEN_AND_STR(etaptr));
					if (WBTEST_ENABLED(WBTEST_BACKUP_FORCE_SLEEP))
						LONG_SLEEP(10);
				}
			}
			if (TRUE == mu_ctrlc_occurred)
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		} while ((0 < remaining) && (-1 < ret));
		close(infd);
		close(outfd);
		if ((0 != remaining) || (ret == -1))
			CLEANUP_AND_RETURN_FALSE(CAN_RETRY); /* Copy failed so no need to retry */
	}
#	endif
	if (TRUE == use_cp_or_pax)
	{
		/* Calculate total line length for commands to execute (pushd + cp). *
		 * If cannot fit in local variable array, malloc space *
		 * commands to be executed :
			pushd sourcedir && CP_CMD fname tempfilename
		*/
		cmdlen = STR_LIT_LEN(UNALIAS) + STR_LIT_LEN(CD_CMD) + sourcedirlen + STR_LIT_LEN(CMD_SEPARATOR);
		cmdlen += STR_LIT_LEN(fulpathcmd[0]) + STR_LIT_LEN(CP_OPT) + sourcefilelen + 5 /* 4 quotes and 1 space */
									+ realpathlen + 1 /* terminating NULL byte */;
		if (cmdlen > SIZEOF(cmdarray))
			command = malloc(cmdlen);	/* allocate memory and use that instead of local array "cmdarray" */
		/* cd */
		MEMCPY_LIT(command, UNALIAS);
		cmdlen = STR_LIT_LEN(UNALIAS);
		MEMCPY_LIT(&command[cmdlen], CD_CMD);
		cmdlen += STR_LIT_LEN(CD_CMD);
		memcpy(&command[cmdlen], sourcedirname, sourcedirlen);
		cmdlen += sourcedirlen;
		MEMCPY_LIT(&command[cmdlen], CMD_SEPARATOR);
		cmdlen += STR_LIT_LEN(CMD_SEPARATOR);
		/* cp */
		cmdpathlen = STRLEN(fulpathcmd[0]);
		memcpy(&command[cmdlen], fulpathcmd[0], cmdpathlen);
		cmdlen += cmdpathlen;
		MEMCPY_LIT(&command[cmdlen], CP_OPT);
		cmdlen += STR_LIT_LEN(CP_OPT);
		command[cmdlen++] = '\'';
		memcpy(&command[cmdlen], sourcefilename, sourcefilelen);
		cmdlen += sourcefilelen;
		command[cmdlen++] = '\'';
		command[cmdlen++] = ' ';
		command[cmdlen++] = '\'';
		memcpy(&command[cmdlen], realpathname, realpathlen);
		cmdlen += realpathlen;
		command[cmdlen++] = '\'';
		command[cmdlen] = 0;
		if (debug_mupip)
			util_out_print("!/MUPIP INFO:   !AD", TRUE, cmdlen, command);
		if (WBTEST_ENABLED(WBTEST_BACKUP_FORCE_SLEEP))
		{
			util_out_print("BACKUP_STARTED", TRUE);
			LONG_SLEEP(20);
		}
		/* Perform the copy (cp or pax) */
		rv = SYSTEM((char *)command);
		if (0 != rv)
		{
			if (-1 == rv)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("system : !AZ", TRUE, errptr);
			}
			util_out_print("Error doing !AD", TRUE, cmdlen, command);
			CLEANUP_AND_RETURN_FALSE(CAN_RETRY);
		}
		if (TRUE == mu_ctrlc_occurred)
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	FREE_COMMAND_STR_IF_NEEDED;
	assert(command == &cmdarray[0]);
	/* tempfilename currently contains the name of temporary directory created.
	 * add the DB filename (only the final filename, without the pathname) to point to tmpfilename.
	 * Check that enough space is there in the buffer.
	 */
	if (SIZEOF(tempfilename) < (tempfilelen + 1 + sourcefilelen + 1))	/* +1 for the '/', +1 for '\0' */
	{	/* Buffer was not enough. Print the too-long file name but allocate memory first */
		char	*tempfilename2;
		int	nbytes, nbytes2;

		nbytes = tempfilelen + sourcefilelen + 1;
		tempfilename2 = malloc(nbytes + 1);	/* + 1 is for terminating null */
		nbytes2 = SNPRINTF(tempfilename2, nbytes + 1, "%s/%s", tempfilename, sourcefilename);
		assert((0 < nbytes2) && (nbytes2 < (nbytes + 1)));
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FILEPARSE, 2, nbytes2, tempfilename2);
		free(tempfilename2);
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_FILENAMETOOLONG);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	tempfilename[tempfilelen++] = '/';
	memcpy(&tempfilename[tempfilelen], sourcefilename, sourcefilelen);
	tempfilelen += sourcefilelen;
	tempfilename[tempfilelen] = 0;
	/* give temporary files the group and permissions as other shared resources - like journal files */
	OPENFILE(tempfilename, O_RDWR, backup_fd);
	if (FD_INVALID == backup_fd)
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("open : !AZ", TRUE, errptr);
		util_out_print("Error opening backup file !AD.", TRUE, file->len, file->addr);
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	FSTAT_FILE(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fd, &stat_buf, fstat_res);
	if (-1 != fstat_res)
		if (!gtm_permissions(&stat_buf, &user_id, &group_id, &perm, PERM_FILE, &pdd))
		{
			send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
				RTS_ERROR_STRING(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
				PERMGENDIAG_ARGS(pdd));
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
				ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
				RTS_ERROR_STRING(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
				PERMGENDIAG_ARGS(pdd));
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
	/* Setup new group and permissions if indicated by the security rules. */
	if ((-1 == fstat_res) || (-1 == FCHMOD(backup_fd, perm))
		|| (((INVALID_UID != user_id) || (INVALID_GID != group_id)) && (-1 == fchown(backup_fd, user_id, group_id))))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("system : !AZ", TRUE, errptr);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
#	if defined(__MVS__)
	if (-1 == gtm_zos_tag_to_policy(backup_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG( tempfilename, realfiletag, TAG_BINARY, errno);
#	endif
	if (online)
	{
		cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		if ((cs_addrs->onln_rlbk_cycle != cs_addrs->nl->onln_rlbk_cycle) ||
			(0 != cs_addrs->nl->onln_rlbk_pid))
		{	/* A concurrent online rollback happened since we did the gvcst_init. The backup is not reliable.
			 * Cleanup and exit
			 */
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DBROLLEDBACK);
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		/* if there has been an extend, truncate it */
		FSTAT_FILE(backup_fd, &stat_buf, fstat_res);
		if (-1 == fstat_res)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("fstat : !AZ", TRUE, errptr);
			util_out_print("Error obtaining status of backup file !AD.", TRUE, file->len, file->addr);
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		filesize = (off_t)BLK_ZERO_OFF(header_cpy->start_vbn)
				+ (off_t)(header_cpy->trans_hist.total_blks + 1) * header_cpy->blk_size;
		if (filesize != stat_buf.st_size)
		{	/* file has been extended, so truncate it and set the end of database block */
			FTRUNCATE(backup_fd, filesize, ftruncate_res);
			if (-1 == ftruncate_res)
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print("ftruncate : !AZ", TRUE, errptr);
				util_out_print("Error truncating backup file !AD.", TRUE, file->len, file->addr);
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
			}
			status = db_write_eof_block(NULL, backup_fd, cs_data->blk_size, filesize - header_cpy->blk_size,
													&(TREF(dio_buff)));
			if (0 != status)
			{
				util_out_print("Error writing the last block in database !AD.", TRUE, file->len, file->addr);
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
			}
		}
		/* By getting crit here, we ensure that there is no process still in transaction logic that sees
		 * (nbb != BACKUP_NOT_IN_PRORESS). After rel_crit(), any process that enters transaction logic will
		 * see (nbb == BACKUP_NOT_IN_PRORESS) because we just set it to that value. At this point, backup
		 * buffer is complete and there will not be any more new entries in the backup buffer until the next
		 * backup.
		 */
		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(gv_cur_region, WS_89);
		assert(cs_data == cs_addrs->hdr);
		if (dba_bg == cs_data->acc_meth)
		{	/* Now that we have crit, wait for any pending phase2 updates to finish. Since phase2 updates happen
			 * outside of crit, we don't want them to keep writing to the backup temporary file even after the
			 * backup is complete and the temporary file has been deleted.
			 */
			if (cs_addrs->nl->wcs_phase2_commit_pidcnt && !wcs_phase2_commit_wait(cs_addrs, NULL))
			{
				assert(FALSE);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1,
					cs_addrs->nl->wcs_phase2_commit_pidcnt, DB_LEN_STR(gv_cur_region));
				rel_crit(gv_cur_region);
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
			}
		}
		if (debug_mupip)
		{
			util_out_print("MUPIP INFO:   Current Transaction # at end of backup is 0x!16@XQ", TRUE,
				&cs_data->trans_hist.curr_tn);
		}
		rel_crit(gv_cur_region);
		counter = 0;
		while ((0 != cs_addrs->shmpool_buffer->backup_cnt) && (0 == cs_addrs->shmpool_buffer->failed))
		{
			backup_buffer_flush(gv_cur_region);
			if (++counter > MAX_BACKUP_FLUSH_TRY)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_BCKUPBUFLUSH);
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
			}
			if (counter & 0xF)
			{
#				ifdef DEBUG
				if (WBTEST_ENABLED(WBTEST_FORCE_SHMPLRECOV)) /* Fake shmpool_blocked */
					cs_addrs->shmpool_buffer->shmpool_blocked = TRUE;
#				endif
				wcs_sleep(counter);
			} else
			{	/* Force recovery every few retries - this should not be happening */
				if (FALSE == shmpool_lock_hdr(gv_cur_region))
				{
					assert(FALSE);
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
				}
				shmpool_abandoned_blk_chk(gv_cur_region, TRUE);
				shmpool_unlock_hdr(gv_cur_region);
			}
		}
		if (0 != cs_addrs->shmpool_buffer->failed)
		{
			assert(EACCES == cs_addrs->shmpool_buffer->backup_errno);
			util_out_print("Process !UL encountered the following error.", TRUE, cs_addrs->shmpool_buffer->failed);
			if (0 != cs_addrs->shmpool_buffer->backup_errno)
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) cs_addrs->shmpool_buffer->backup_errno);
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		FSTAT_FILE(list->backup_fd, &stat_buf, fstat_res);
		if (-1 == fstat_res)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("fstat : !AZ", TRUE, errptr);
			util_out_print("Error obtaining status of temporary file !AD.",
					TRUE, LEN_AND_STR(list->backup_tempfile));
			util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
			CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
		}
		if (0 < stat_buf.st_size)
		{
			rsize = (int4)(SIZEOF(muinc_blk_hdr) + header_cpy->blk_size);
			sblkh_p = (muinc_blk_hdr_ptr_t)malloc(rsize);
			/* Do not use LSEEKREAD macro here because of dependence on setting filepointer for
			 * subsequent reads.
			 */
			 if (-1 != (ssize_t)lseek(list->backup_fd, 0, SEEK_SET))
			{
				DOREADRC(list->backup_fd, (sm_uc_ptr_t)sblkh_p, rsize, status);
			} else
				status = errno;
			if (0 != status)
			{
				if (0 < status)
				{
					errptr = (char *)STRERROR((int)status);
					util_out_print("read : ", TRUE, errptr);
					util_out_print("Error reading the temporary file !AD.",
							TRUE, LEN_AND_STR(list->backup_tempfile));
				}
				else
					util_out_print("Premature end of temporary file !AD.",
							TRUE, LEN_AND_STR(list->backup_tempfile));
				util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
				free(sblkh_p);
				CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
			}
			while (TRUE)
			{
				assert(sblkh_p->valid_data);
				blk_num = sblkh_p->blkid;
				if (WBTEST_ENABLED(WBTEST_BACKUP_FORCE_SLEEP))
				{
					util_out_print("RESTORE_BLOCK_STAGE", TRUE);
					LONG_SLEEP(10);
				}
				if (debug_mupip)
					util_out_print("MUPIP INFO:     Restoring block 0x!XL from temporary file.",
							TRUE, blk_num);

				if (blk_num < header_cpy->trans_hist.total_blks)
				{
					inbuf = (char_ptr_t)(sblkh_p + 1);
					/* Previously, blocks could be downgraded here as needed */
					size = (((blk_hdr_ptr_t)inbuf)->bsiz + 1) & ~1;

					if (cs_data->write_fullblk)
						size = ROUND_UP(size, cs_addrs->fullblockwrite_len);
					assert((uint4)cs_addrs->hdr->blk_size >= size);
					offset = BLK_ZERO_OFF(header_cpy->start_vbn) + ((off_t)header_cpy->blk_size * blk_num);
					LSEEKWRITE(backup_fd,
						   offset,
						   inbuf,
						   size,
						   save_errno);
					if (0 != save_errno)
					{
						util_out_print("Error accessing output file !AD. Aborting restore.",
								TRUE, file->len, file->addr);
						errptr = (char *)STRERROR(save_errno);
						util_out_print("write : !AZ", TRUE, errptr);
						free(sblkh_p);
						CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
					}
				} /* Else ignore block that is larger than file was at time backup initiated */
				DOREADRC(list->backup_fd, (sm_uc_ptr_t)sblkh_p, rsize, save_errno);
				if (0 != save_errno)
				{
					if (0 < save_errno)
					{
						util_out_print("Error accessing temporary file !AD.",
								TRUE, LEN_AND_STR(list->backup_tempfile));
						errptr = (char *)STRERROR(save_errno);
						util_out_print("read : !AZ", TRUE, errptr);
						free(sblkh_p);
						CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
					} else
						/* End of file .. Note this does not detect the difference between
						 * clean end of file and partial record end of file.
						 */
						break;
				}
			}
			free(sblkh_p);
		}
	}
	if (0 == memcmp(header_cpy->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
		db_header_dwnconv(header_cpy);
	LSEEKWRITE(backup_fd, 0, header_cpy, hdrsize, save_errno);
	if (0 != save_errno)
	{
		errptr = (char *)STRERROR(save_errno);
		util_out_print("write : !AZ", TRUE, errptr);
		util_out_print("Error writing data to backup file !AD.", TRUE, file->len, file->addr);
		util_out_print("WARNING: backup file !AD is not valid.", TRUE, file->len, file->addr);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	CLOSEFILE_RESET(backup_fd, status);	/* resets "backup_fd" to FD_INVALID */
	/* mv it to destination */
	/* Calculate total line length for "mv" command. If cannot fit in local variable array, malloc space */
	assert(command == &cmdarray[0]);
	tmplen = file->len;
	/* Command to be executed : mv tempfilename backup_file */
	cmdlen = STR_LIT_LEN(UNALIAS) + STR_LIT_LEN(fulpathcmd[1]);
	cmdlen += tempfilelen + 5 /* 4 quotes, 1 space */ + tmplen + 1 /* terminating NULL byte */;
	if (cmdlen > SIZEOF(cmdarray))
		command = malloc(cmdlen);	/* allocate memory and use that instead of local array "cmdarray" */
	/* mv tmpfile destfile */
	MEMCPY_LIT(command, UNALIAS);
	cmdlen = STR_LIT_LEN(UNALIAS);
	cmdpathlen = STRLEN(fulpathcmd[1]);
	memcpy(&command[cmdlen], fulpathcmd[1], cmdpathlen);
	cmdlen += cmdpathlen;
	command[cmdlen++] = '\'';
	memcpy(&command[cmdlen], tempfilename, tempfilelen);
	cmdlen += tempfilelen;
	command[cmdlen++] = '\'';
	command[cmdlen++] = ' ';
	command[cmdlen++] = '\'';
	memcpy(&command[cmdlen], file->addr, tmplen);
	cmdlen += tmplen;
	command[cmdlen++] = '\'';
	command[cmdlen] = 0;
	if (debug_mupip)
		util_out_print("MUPIP INFO:   !AD", TRUE, cmdlen, command);
	rv = SYSTEM((char *)command);
	if (WBTEST_ENABLED(WBTEST_BACKUP_FORCE_MV_RV))
	{
		util_out_print("Simulated mv returns an error.", TRUE);
		rv = -1;
	}
	if (0 != rv)
	{
		if (-1 == rv)
		{
			save_errno = errno;
			errptr = (char *)STRERROR(save_errno);
			util_out_print("system : !AZ", TRUE, errptr);
		}
		util_out_print("Error executing command : !AD", TRUE, cmdlen, command);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	/* rm tempdir */
	tempdir[tmpdirlen] = 0;
	if (0 != rmdir(tempdir))
	{
		save_errno = errno;
		errptr = (char *)STRERROR(save_errno);
		util_out_print("access : !AZ", TRUE, errptr);
		util_out_print("Removing temp dir : !AD", TRUE, tmpdirlen, tempdir);
		CLEANUP_AND_RETURN_FALSE(CANNOT_RETRY);
	}
	FREE_COMMAND_STR_IF_NEEDED;
	assert(command == &cmdarray[0]);
	gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_BACKUPDBFILE, 4, gv_cur_region->dyn.addr->fname_len,
			gv_cur_region->dyn.addr->fname, file->len, file->addr);
	gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_BACKUPTN, 2, &ONE, &header_cpy->trans_hist.curr_tn);
	cs_addrs->hdr->last_com_backup = header_cpy->trans_hist.curr_tn;
	cs_addrs->hdr->last_com_bkup_last_blk = header_cpy->trans_hist.total_blks;
	if (record)
	{
		cs_addrs->hdr->last_rec_backup = header_cpy->trans_hist.curr_tn;
		cs_addrs->hdr->last_rec_bkup_last_blk = header_cpy->trans_hist.total_blks;
		cs_addrs->hdr->last_start_backup = header_cpy->last_start_backup;
	}
	file_backed_up = TRUE;
	if (TRUE == grab_crit_immediate(gv_cur_region, TRUE, NOT_APPLICABLE))
	{
		 if (!wcs_flu(WCSFLU_FLUSH_HDR))
		 {
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) MAKE_MSG_WARNING(ERR_BUFFLUFAILED), 4,
					LEN_AND_LIT("MUPIP BACKUP -DATABASE"), DB_LEN_STR(gv_cur_region));

		 }
		 rel_crit(gv_cur_region);
        }
	return TRUE;
}
#ifdef __x86_64
/* error handler for copy_file_range() */
inline int handle_err(char errorstr[], int saved_errno)
{
	char *customptr, *adviceptr;
	char oserror[BUF_MAX] = "\0";
	int status;
	customptr = "\0";
	switch (saved_errno)
	{
		case ENOTSUP:
		case ENOSYS:
			return 1;
		case EFAULT:
		case ENOSPC:
		{
			SNPRINTF(oserror, BUF_MAX, "%s, %s for backing up region %s", "ENOSPC",
			(char *)STRERROR(saved_errno), gv_cur_region->rname);
			if (NULL == getenv("gtm_baktmpdir"))
			{
				customptr = "As $gtm_baktmpdir is not defined, GT.M uses the backup destination "
						"to store temporary files. Consider defining $gtm_baktmpdir to "
						"an appropriate location.";
			}
			else
				customptr = "Ensure that you have adequate space available on the backup location.";
			break;
		}
		default:
		{
			if (0 != saved_errno)
			{
				SNPRINTF(oserror, BUF_MAX, "Error code: %d, %s for backing up region %s", saved_errno,
						(char *)STRERROR(saved_errno), gv_cur_region->rname);
			}
			break;
		}
	}
	if (0 < STRLEN(errorstr))
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(errorstr));
	if (0 < STRLEN(oserror))
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(oserror));
	/* Extra error information about what the error may mean to GT.M */
	if (0 < STRLEN(customptr))
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(customptr));
	return 1; /* In all error conditions, return 1 which initiates the CLEANUP sequence  */
}
/* countdigits(size_t num) returns the number of digits */
inline int countdigits(size_t n)
{
	uint4	digitcnt = 0;
	do
	{
		n = n / 10;
		digitcnt++;
	} while (n);
	return digitcnt;
}
#endif
