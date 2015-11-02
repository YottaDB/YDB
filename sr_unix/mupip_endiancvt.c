/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_sem.h"

#include <sys/wait.h>
#include <stddef.h>
#include <errno.h>
#include "gtm_time.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#include <sys/time.h>
#endif

#include "gdsroot.h"
#include "v15_gdsroot.h"	/* for v15_trans_num */
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdsbml.h"
#include "cli.h"
#include "iosp.h"
#include "copy.h"
#include "error.h"
#include "gtmio.h"
#include "iotimer.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "stp_parms.h"
#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "io.h"
#include "is_proc_alive.h"
#include "mu_rndwn_file.h"
#include "mupip_exit.h"
#include "mu_gv_cur_reg_init.h"
#include "mupip_endiancvt.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "gvcst_lbm_check.h"		/* gvcst_blk_ever_allocated */
#include "shmpool.h"
#include "min_max.h"
#include "spec_type.h"			/* collation info */
#include "anticipatory_freeze.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "db_ipcs_reset.h"

GBLREF	gd_region		*gv_cur_region;

LITREF	char			*gtm_dbversion_table[];

error_def(ERR_BADTAG);
error_def(ERR_DBRDONLY);
error_def(ERR_ENDIANCVT);
error_def(ERR_IOEOF);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUSTANDALONE);
error_def(ERR_NOENDIANCVT);
error_def(ERR_TEXT);

#define NOTCURRDBFORMAT		"database format is not the current version"
#define NOTCURRMDBFORMAT	"minor database format is not the current version"
#define NOTFULLYUPGRADED	"some blocks are not upgraded to the current version"
#define KILLINPROG		"kills in progress"
#define ABANDONED_KILLS		"abandoned kills present"
#define GTCMSERVERACTIVE	"a GT.CM server accessing the database"
#define	RECOVINTRPT		"recovery was interrupted"
#define DBCREATE		"database creation in progress"
#define DBCORRUPT		"the database is corrupted"

#define MAX_CONF_RESPONSE	30

#ifdef GTM_CRYPT
boolean_t		is_encrypted = FALSE;
gtmcrypt_key_t		encr_key_handle;
#endif

typedef struct
{	/* adapted from dbcertify.h */
	unsigned int	top;		/* Offset to top of the key */
	unsigned int	end;		/* End of the current key */
	unsigned int	gvn_len;	/* Length of key */
	unsigned char	*key;		/* Pointer to the key */
} end_gv_key;

typedef struct
{
	int			db_fd;
	int			outdb_fd;		/* FD_INVALID if inplace */
	boolean_t		inplace;		/* update in place */
	boolean_t		endian_native;		/* original database */
	uint4			tot_blks;
	int			bsize;			/* GDS block size */
	int4			startvbn;		/* in DISK_BLOCK_SIZE units */
	block_id		last_blk_cvt;		/* highest block converted so far not lbm */
	char			*database_fn;
	int			database_fn_len;
	struct	/* used by find_dtblk related routines */
	{
		char		*buff;
		char		*dtrbuff;	/* keep the DT root */
		block_id	blkid;
		boolean_t	native;		/* records are native endian */
		boolean_t	dtrnative;	/* records in dtrbuff are native endian */
		int		count;		/* number of times needed */
	}	dtblk;
} endian_info;

void	endian_header(sgmnt_data *new, sgmnt_data *old, boolean_t new_is_native);
int4	endian_process(endian_info *info, sgmnt_data *new_data, sgmnt_data *old_data);
void	endian_cvt_blk_hdr(blk_hdr_ptr_t blkhdr, boolean_t new_is_native, boolean_t make_empty);
void	endian_cvt_blk_recs(endian_info *info, char *new_block, blk_hdr_ptr_t blkhdr, int blknum);
char	*endian_read_dbblk(endian_info *info, block_id blk_to_get);
void	endian_find_key(endian_info *info, end_gv_key *gv_key, char *rec_p, int rec_len, int blk_levl);
boolean_t	endian_match_key(end_gv_key *gv_key1, int blk_levl, end_gv_key *key2);
block_id	endian_find_dtblk(endian_info *info, end_gv_key *gv_key);

/* If we acquired standalone access, we need to release it before we exit. Ideally, mupip_exit_handler should take care of doing
 * it. But, since we free up memory allocated to gv_cur_region before exiting out of this module. An alternative would be to free
 * gv_cur_region AFTER db_ipcs_reset in mupip_exit_handler but since various code paths set gv_cur_region, the implication of such
 * a change is not clear at this writing.
 */
#define DO_STANDALONE_CLNUP_IF_NEEDED(ENDIAN_NATIVE)			\
{									\
	if (ENDIAN_NATIVE)						\
	{	/* release standalone access */				\
		assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);	\
		db_ipcs_reset(gv_cur_region);				\
		mu_gv_cur_reg_free();					\
	}								\
}

void mupip_endiancvt(void)
{
	char			db_name[MAX_FN_LEN + 1], *t_name;
	sgmnt_data		*old_data, *new_data;
	int4			status, save_errno;
	int			rc;
	uint4			swap_uint4;
	enum db_ver		swap_dbver;
	enum mdb_ver		swap_mdbver;
	trans_num		curr_tn;
	block_id		blk_num;
	uint4			cli_status;
	int			i, db_fd, outdb_fd, mastermap_size;
	unsigned short		n_len, outdb_len, t_len;
	boolean_t		outdb_specified, endian_native, swap_boolean, got_standalone, override_specified;
	char			outdb[MAX_FN_LEN + 1], conf_buff[MAX_CONF_RESPONSE + 1], *response;

	char			*errptr, *check_error, *mastermap;
	char			*from_endian, *to_endian;
	endian32_struct		endian_check;
	endian_info		info;
#	ifdef GTM_CRYPT
	int			gtmcrypt_errno;
#	endif
	ZOS_ONLY(int 		realfiletag;)

	if (CLI_PRESENT == (cli_status = cli_present("OUTDB")))
	{
		outdb_specified = TRUE;
		outdb_len = SIZEOF(outdb) - 1;
		if (!cli_get_str("OUTDB", outdb, &outdb_len))
			mupip_exit(ERR_MUPCLIERR);
	} else
		outdb_specified = FALSE;
	n_len = SIZEOF(db_name) - 1;
	if (cli_get_str("DATABASE", db_name, &n_len) == FALSE)
		mupip_exit(ERR_MUPCLIERR);

	OPENFILE(db_name, (!outdb_specified ? O_RDWR : O_RDONLY), db_fd);
	if (FD_INVALID == db_fd)
	{
		save_errno = errno;
		util_out_print("Error accessing database file !AD. Aborting endiancvt.", TRUE, n_len, db_name);
		errptr = (char *)STRERROR(save_errno);
		util_out_print("open : !AZ", TRUE, errptr);
		mupip_exit(save_errno);
	}
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(db_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(db_name, realfiletag, TAG_BINARY, errno);
#endif
	old_data = (sgmnt_data *)malloc(SIZEOF(sgmnt_data));
	LSEEKREAD(db_fd, 0, old_data, SIZEOF(sgmnt_data), save_errno);
	if (0 != save_errno)
	{
		free(old_data);
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		util_out_print("Error reading database file !AD header. Aborting endiancvt.", TRUE, n_len, db_name);
		if (-1 != save_errno)
		{
			errptr = (char *)STRERROR(save_errno);
			util_out_print("read : !AZ", TRUE, errptr);
			mupip_exit(save_errno);
		} else
			mupip_exit(ERR_IOEOF);
	}
	if (MEMCMP_LIT(&old_data->label[0], GDS_LABEL))
	{
		util_out_print("Database file !AD has an unrecognizable format", TRUE, n_len, db_name);
		free(old_data);
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		mupip_exit(ERR_MUNOACTION);
	}
	override_specified = (CLI_PRESENT == (cli_status = cli_present("OVERRIDE")));
	check_error = NULL;
	endian_check.word32 = (uint4)old_data->minor_dbver;
#ifdef BIGENDIAN
	if (!endian_check.shorts.big_endian)
#else
	if (!endian_check.shorts.little_endian)
#endif
	{
		endian_native = FALSE;		/* nobody can be using the db */
		/* do checks after swapping fields */
		assert(SIZEOF(int4) == SIZEOF(old_data->desired_db_format));
		/* If OVERRIDE is specified, skip checking for those fields that are not critical for the integrity of the db.
		 * Any field that indicates the db is potentially damaged cannot be overridden.
		 */
		if (!override_specified)
		{
			swap_mdbver = (enum mdb_ver)GTM_BYTESWAP_32(old_data->minor_dbver);
			if (GDSMVCURR != swap_mdbver)
			{
				check_error = NOTCURRMDBFORMAT;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			swap_uint4 = GTM_BYTESWAP_32(old_data->kill_in_prog);
			if (0 != swap_uint4)
			{
				check_error = KILLINPROG;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			swap_uint4 = GTM_BYTESWAP_32(old_data->abandoned_kills);
			if (0 != swap_uint4)
			{
				check_error = ABANDONED_KILLS;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			swap_uint4 = GTM_BYTESWAP_32(old_data->rc_srv_cnt);
			if (0 != swap_uint4)
			{
				check_error = GTCMSERVERACTIVE;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
		}
		swap_dbver = (enum db_ver)GTM_BYTESWAP_32(old_data->desired_db_format);
		if (GDSVCURR != swap_dbver)
		{
			check_error = NOTCURRDBFORMAT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		assert(SIZEOF(int4) == SIZEOF(old_data->fully_upgraded));
		swap_boolean = GTM_BYTESWAP_32(old_data->fully_upgraded);
		if (!swap_boolean)
		{
			check_error = NOTFULLYUPGRADED;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		swap_uint4 = GTM_BYTESWAP_32(old_data->recov_interrupted);
		if (0 != swap_uint4)
		{
			check_error = RECOVINTRPT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		swap_uint4 = GTM_BYTESWAP_32(old_data->createinprogress);
		if (0 != swap_uint4)
		{
			check_error = DBCREATE;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		swap_uint4 = GTM_BYTESWAP_32(old_data->file_corrupt);
		if (0 != swap_uint4)
		{
			check_error = DBCORRUPT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
	} else
	{
		endian_native = TRUE;		/* need to get standalone */
		mu_gv_cur_reg_init();
		strcpy((char *)gv_cur_region->dyn.addr->fname, db_name);
		gv_cur_region->dyn.addr->fname_len = n_len;
		got_standalone = STANDALONE(gv_cur_region);
		if (FALSE == got_standalone)
		{
			mu_gv_cur_reg_free();
			free(old_data);
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			gtm_putmsg(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR), 2, n_len, db_name);
			mupip_exit(ERR_MUNOACTION);
		}
		if (gv_cur_region->read_only && !outdb_specified)
		{
			DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
			free(old_data);
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, n_len, db_name);
			mupip_exit(ERR_MUNOACTION);
		}
		if (!override_specified)
		{
			if (GDSMVCURR != old_data->minor_dbver)
			{
				check_error = NOTCURRMDBFORMAT;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			if (0 != old_data->kill_in_prog)
			{
				check_error = KILLINPROG;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			if (0 != old_data->abandoned_kills)
			{
				check_error = ABANDONED_KILLS;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
			if (0 != old_data->rc_srv_cnt)
			{
				check_error = GTCMSERVERACTIVE;
				gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
			}
		}
		if (GDSVCURR != old_data->desired_db_format)
		{
			check_error = NOTCURRDBFORMAT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		if (!old_data->fully_upgraded)
		{
			check_error = NOTFULLYUPGRADED;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		if (0 != old_data->recov_interrupted)
		{
			check_error = RECOVINTRPT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		if (0 != old_data->createinprogress)
		{
			check_error = DBCREATE;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		if (0 != old_data->file_corrupt)
		{
			check_error = DBCORRUPT;
			gtm_putmsg(VARLSTCNT(6) ERR_NOENDIANCVT, 4, n_len, db_name, LEN_AND_STR(check_error));
		}
		if (!check_error && !outdb_specified)
		{
			if (JNL_ENABLED(old_data))
			{
				/* report what we are doing ERR_JNLSTATE */
				util_out_print("!_Journaling was enabled - now closed",TRUE);
				old_data->jnl_state = jnl_closed;
			}
			if (REPL_ENABLED(old_data))
			{
				/* report what we are doing ERR_REPLSTATE */
				util_out_print("!_Replication was enabled - now closed",TRUE);
				old_data->repl_state = repl_closed;
			}
		}
	}
	if (check_error)
	{
		DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
		free(old_data);
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		mupip_exit(ERR_MUNOACTION);
	}
#	ifdef GTM_CRYPT
	is_encrypted = endian_native ? old_data->is_encrypted : GTM_BYTESWAP_32(old_data->is_encrypted);
	if (is_encrypted)
	{	/* Database is encrypted. Initialize encryption and setup the keys to be used in later encryption/decryption */
		INIT_PROC_ENCRYPTION(NULL, gtmcrypt_errno);
		if (0 == gtmcrypt_errno)
			GTMCRYPT_GETKEY(NULL, old_data->encryption_hash, encr_key_handle, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, n_len, db_name);
			mupip_exit(gtmcrypt_errno);
		}
		info.database_fn = &db_name[0];
		info.database_fn_len = n_len;
	}
#	endif
	from_endian = endian_check.shorts.big_endian ? "BIG" : "LITTLE";
	to_endian = endian_check.shorts.big_endian ? "LITTLE" : "BIG";
	util_out_print("Converting database file !AD from !AZ endian to !AZ endian on a !AZ endian system", TRUE,
		n_len, db_name, from_endian, to_endian, ENDIANTHIS);
	if (!outdb_specified)
	{
		util_out_print("Converting in place - database will be damaged by an abnormal termination", TRUE);
		util_out_print("You must have a backup before proceeding", TRUE);
	} else
		util_out_print("Converting to new file !AD", TRUE, outdb_len, outdb);
	util_out_print("Proceed [yes/no] ?", TRUE);
	response = util_input(conf_buff, MAX_CONF_RESPONSE, stdin, TRUE);
	if (NULL == response || ('Y' != conf_buff[0] && 'y' != conf_buff[0]))
	{
		DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
		free(old_data);
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		mupip_exit(ERR_MUNOACTION);
	}
	new_data = (sgmnt_data *)malloc(SIZEOF(sgmnt_data));
	memcpy(new_data, old_data, SIZEOF(sgmnt_data));
	new_data->file_corrupt = endian_native ? GTM_BYTESWAP_32(TRUE) : TRUE;
	info.db_fd = db_fd;
	info.inplace = !outdb_specified;
	info.endian_native = info.dtblk.native = info.dtblk.dtrnative = endian_native;
	info.tot_blks = info.bsize = info.startvbn = info.last_blk_cvt = 0;
	info.dtblk.buff = info.dtblk.dtrbuff = NULL;
	info.dtblk.blkid = -1;			/* invalid block number */
	info.dtblk.count = 0;
	endian_header(new_data, old_data, !endian_native); /* convert file header fields */
	if (outdb_specified)
	{
		OPENFILE3(outdb, O_RDWR | O_CREAT | O_EXCL, 0666, outdb_fd);
		if (FD_INVALID == outdb_fd)
		{	/* error */
			save_errno = errno;
			util_out_print("Error creating converted databasae file !AD.  Aborting endiancvt.", TRUE, outdb_len, outdb);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("open : !AZ", TRUE, errptr);
			DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
			free(new_data);
			free(old_data);
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			mupip_exit(save_errno);
		}
#ifdef __MVS__
		if (-1 == gtm_zos_set_tag(outdb_fd, TAG_BINARY, TAG_NOTTEXT, TAG_DONTFORCE, &realfiletag))
			TAG_POLICY_GTM_PUTMSG(outdb, realfiletag, TAG_BINARY, errno);
#endif
		new_data->file_corrupt = endian_native ? GTM_BYTESWAP_32(TRUE) : TRUE;
		LSEEKWRITE(outdb_fd, 0, new_data, SIZEOF(sgmnt_data), save_errno);
		if (0 != save_errno)
		{
			free(new_data);
			free(old_data);
			CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
			util_out_print("Error writing converted database file !AD header.  Aborting endiancvt.", TRUE,
				outdb_len, outdb);
			if (-1 != save_errno)
			{
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				mupip_exit(save_errno);
			} else
			{
				util_out_print("write : unexpected error", TRUE);
				mupip_exit(ERR_MUNOFINISH);
			}
		}
		/* read master bit map from old file */
		mastermap_size = endian_native ? old_data->master_map_len : new_data->master_map_len;
		mastermap = malloc(mastermap_size);
		LSEEKREAD(db_fd, SGMNT_HDR_LEN, mastermap, mastermap_size, save_errno);
		if (0 != save_errno)
		{
			free(new_data);
			free(old_data);
			CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
			util_out_print("Error reading database file !AD master map.  Aborting endiancvt.", TRUE,
				n_len, db_name);
			if (-1 != save_errno)
			{
				errptr = (char *)STRERROR(save_errno);
				util_out_print("read : !AZ", TRUE, errptr);
				mupip_exit(save_errno);
			} else
				mupip_exit(ERR_IOEOF);
		}
		/* write master bit map to new file */
		LSEEKWRITE(outdb_fd, SGMNT_HDR_LEN, mastermap, mastermap_size, save_errno);
		if (0 != save_errno)
		{
			free(new_data);
			free(old_data);
			CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
			CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
			DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
			util_out_print("Error writing converted database file !AD master map.  Aborting endiancvt.", TRUE,
				outdb_len, outdb);
			if (-1 != save_errno)
			{
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				mupip_exit(save_errno);
			} else
			{
				util_out_print("write : unexpected error", TRUE);
				mupip_exit(ERR_MUNOFINISH);
			}
		}
	} else
		outdb_fd = FD_INVALID;
	info.outdb_fd = outdb_fd;
	status = endian_process(&info, new_data, old_data);
	if (0 != status)
	{
		/* db_ipcs_reset in the macro below works even with the now converted opposite endian header since it just sets
		 * csd->s{e|h}mid to INVALIDS{E|H}MID and zeroes s{e|h}m_ctime.
		 */
		DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
		free(new_data);
		free(old_data);
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		if (outdb_specified)
			CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
		mupip_exit(ERR_MUNOFINISH);	/* endian_process issued specific message */
	}
	new_data->file_corrupt = endian_native ? GTM_BYTESWAP_32(FALSE) : FALSE;
	if (outdb_specified)
	{
		LSEEKWRITE(outdb_fd, 0, new_data, SIZEOF(sgmnt_data), save_errno);
	} else
		DB_LSEEKWRITE((sgmnt_addrs *)NULL, (char *)NULL, db_fd, 0, new_data, SIZEOF(sgmnt_data), save_errno);
	if (0 != save_errno)
	{
		free(new_data);
		free(old_data);
		if (outdb_specified)
			CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
		CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
		DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
		util_out_print("Error writing!AZ database file !AD header.  Aborting endiancvt.", TRUE,
			outdb_specified ? "new" : "", outdb_specified ? outdb_len : n_len, outdb_specified ? outdb : db_name);
		if (-1 != save_errno)
		{
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			mupip_exit(save_errno);
		} else
		{
			util_out_print("write : unexpected error", TRUE);
			mupip_exit(ERR_MUNOFINISH);
		}
	}
	DO_STANDALONE_CLNUP_IF_NEEDED(endian_native);
	free(new_data);
	free(old_data);
	if (outdb_specified)
	{
		GTM_FSYNC(outdb_fd, rc);
	} else
		GTM_DB_FSYNC((sgmnt_addrs *)NULL, db_fd, rc);
	if (-1 == rc)
	{
		save_errno = errno;
		assert(FALSE);
		if (-1 != save_errno)
		{
			errptr = (char *)STRERROR(save_errno);
			util_out_print("fsync : !AZ : !AZ", TRUE, (outdb_specified ? "outdb_fd" : "db_fd"), errptr);
			mupip_exit(save_errno);
		} else
		{
			util_out_print("fsync : !AZ : unexpected error", TRUE, (outdb_specified ? "outdb_fd" : "db_fd"));
			mupip_exit(ERR_MUNOFINISH);
		}
	}
	CLOSEFILE_RESET(db_fd, rc);	/* resets "db_fd" to FD_INVALID */
	if (outdb_specified)
		CLOSEFILE_RESET(outdb_fd, rc);	/* resets "outdb_fd" to FD_INVALID */
	/* Display success message only after all data has been synced to disk and the file descriptors closed */
	gtm_putmsg(VARLSTCNT(7) ERR_ENDIANCVT, 5, n_len, db_name, from_endian, to_endian, ENDIANTHIS);
 	mupip_exit(SS_NORMAL);
}

#define SWAP_SD4(FIELD)	new->FIELD = GTM_BYTESWAP_32(old->FIELD)
#define SWAP_SD4_CAST(FIELD, castType)	new->FIELD = (castType)GTM_BYTESWAP_32(old->FIELD)
#define SWAP_SD8(FIELD)	new->FIELD = GTM_BYTESWAP_64(old->FIELD)

void endian_header(sgmnt_data *new, sgmnt_data *old, boolean_t new_is_native)
{
	int	idx;
	time_t	ctime;

	SWAP_SD4(blk_size);
	SWAP_SD4(master_map_len);
	SWAP_SD4(bplmap);
	SWAP_SD4(start_vbn);
	assert(SIZEOF(int4) == SIZEOF(old->acc_meth));		/* enum */
	SWAP_SD4_CAST(acc_meth, enum db_acc_method);
	SWAP_SD4(max_bts);
	SWAP_SD4(n_bts);
	SWAP_SD4(bt_buckets);
	SWAP_SD4(reserved_bytes);
	SWAP_SD4(max_rec_size);
	SWAP_SD4(max_key_size);
	SWAP_SD4(lock_space_size);
	SWAP_SD4(extension_size);
	SWAP_SD4(def_coll);
	SWAP_SD4(def_coll_ver);
	assert(SIZEOF(int4) == SIZEOF(old->std_null_coll));	/* boolean_t */
	SWAP_SD4(std_null_coll);
	SWAP_SD4(null_subs);
	SWAP_SD4(free_space);
	SWAP_SD4(mutex_spin_parms.mutex_hard_spin_count);	/* gdsbt.h */
	SWAP_SD4(mutex_spin_parms.mutex_sleep_spin_count);
	SWAP_SD4(mutex_spin_parms.mutex_spin_sleep_mask);
	SWAP_SD4(max_update_array_size);
	SWAP_SD4(max_non_bm_update_array_size);
	/* SWAP_SD4(file_corrupt); is set in main routine	*/
	assert(SIZEOF(int4) == SIZEOF(old->minor_dbver));	/* enum */
	SWAP_SD4_CAST(minor_dbver, enum mdb_ver);
	SWAP_SD4(jnl_checksum);
	SWAP_SD4(wcs_phase2_commit_wait_spincnt);
	/* SWAP_SD4(createinprogress);	checked above as FALSE so no need */
	assert(SIZEOF(int4) == SIZEOF(old->creation_time4));
	time(&ctime);
	assert(SIZEOF(ctime) >= SIZEOF(int4));
	new->creation_time4 = (int4)ctime;/* Take only lower order 4-bytes of current time */
	if (!new_is_native)
		SWAP_SD4(creation_time4);
	assert(SIZEOF(gtm_int64_t) == SIZEOF(old->max_tn));	/* trans_num */
	SWAP_SD8(max_tn);
	SWAP_SD8(max_tn_warn);
	SWAP_SD8(last_inc_backup);
	SWAP_SD8(last_com_backup);
	SWAP_SD8(last_rec_backup);
	assert(SIZEOF(int4) == SIZEOF(old->last_inc_bkup_last_blk));	/* block_id */
	SWAP_SD4(last_inc_bkup_last_blk);
	SWAP_SD4(last_com_bkup_last_blk);
	SWAP_SD4(last_rec_bkup_last_blk);
	SWAP_SD4(reorg_restart_block);
	new->image_count = 0;		/* should be zero when db is not open so reset it unconditionally */
	new->freeze = 0;		/* should be zero when db is not open so reset it unconditionally */
	SWAP_SD4(kill_in_prog);
	SWAP_SD4(abandoned_kills);
	SWAP_SD8(tn_upgrd_blks_0);
	SWAP_SD8(desired_db_format_tn);
	SWAP_SD8(reorg_db_fmt_start_tn);
	SWAP_SD4(reorg_upgrd_dwngrd_restart_block);
	SWAP_SD4(blks_to_upgrd);
	SWAP_SD4(blks_to_upgrd_subzero_error);
	assert(SIZEOF(int4) == SIZEOF(old->desired_db_format));	/* enum */
	SWAP_SD4_CAST(desired_db_format, enum db_ver);
	SWAP_SD4(fully_upgraded);	/* should be TRUE */
	assert(new->fully_upgraded);
	/* Since the source database is fully upgraded and since all RECYCLED blocks will be marked as FREE, we are guaranteed
	 * there are NO V4 format block that is too full to be upgraded to V5 format (i.e. will cause DYNUPGRDFAIL error).
	 */
	new->db_got_to_v5_once = TRUE;	/* should be TRUE */
	SWAP_SD8(trans_hist.curr_tn);
	SWAP_SD8(trans_hist.early_tn);
	SWAP_SD8(trans_hist.last_mm_sync);
	SWAP_SD8(trans_hist.mm_tn);
	SWAP_SD4(trans_hist.lock_sequence);
	SWAP_SD4(trans_hist.ccp_jnl_filesize);
	SWAP_SD4(trans_hist.total_blks);
	SWAP_SD4(trans_hist.free_blocks);
	SWAP_SD4(flush_time[0]);
	SWAP_SD4(flush_time[1]);
	SWAP_SD4(flush_trigger);
	SWAP_SD4(n_wrt_per_flu);
	SWAP_SD4(wait_disk_space);
	SWAP_SD4(defer_time);
	SWAP_SD4(reserved_for_upd);
	SWAP_SD4(avg_blks_per_100gbl);
	SWAP_SD4(pre_read_trigger_factor);
	SWAP_SD4(writer_trigger_factor);
	/* Solaris complains about swapping -1
	   assert(INVALID_SEMID == GTM_BYTESWAP_32(INVALID_SEMID));
	   assert(INVALID_SHMID == GTM_BYTESWAP_32(INVALID_SHMID));
	*/
	assert(-1 == INVALID_SEMID);
	assert(-1 == INVALID_SHMID);
	if (new_is_native)
	{	/* Since we have standalone access, reset volatile fields in the database file header */
		new->semid = INVALID_SEMID;
		new->shmid = INVALID_SHMID;
		new->gt_sem_ctime.ctime = 0;
		new->gt_shm_ctime.ctime = 0;
		memset(new->machine_name, 0, MAX_MCNAMELEN);
	}
	/* Convert GVSTATS information */
#	define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)	SWAP_SD8(gvstats_rec.COUNTER);
#	include "tab_gvstats_rec.h"
#	undef TAB_GVSTATS_REC
	SWAP_SD4(staleness[0]);
	SWAP_SD4(staleness[1]);
	SWAP_SD4(ccp_tick_interval[0]);
	SWAP_SD4(ccp_tick_interval[1]);
	SWAP_SD4(ccp_quantum_interval[0]);
	SWAP_SD4(ccp_quantum_interval[1]);
	SWAP_SD4(ccp_response_interval[0]);
	SWAP_SD4(ccp_response_interval[1]);
	SWAP_SD4(ccp_jnl_before);
	SWAP_SD4(clustered);
	SWAP_SD4(unbacked_cache);
        /* RC server related fields sb zero when not active	*/
	SWAP_SD4(rc_srv_cnt);
	SWAP_SD4(dsid);
	SWAP_SD4(rc_node);
	assert(SIZEOF(gtm_int64_t) == SIZEOF(old->reg_seqno));	/* seq_num */
	SWAP_SD8(reg_seqno);
	SWAP_SD8(pre_multisite_resync_seqno);
	/* Note some of the following names were added or renamed in V5.1 but
	   should be of no issue for V5.0 builds since we will be swapping
	   unused fields.
	*/
	SWAP_SD8(zqgblmod_tn);
	SWAP_SD8(zqgblmod_seqno);
	new->repl_state = repl_closed;
	if (!new_is_native)
		SWAP_SD4(repl_state);
	SWAP_SD4(multi_site_open);
	for (idx = 0; idx < ARRAYSIZE(old->tp_cdb_sc_blkmod); idx++)
		new->tp_cdb_sc_blkmod[idx] = 0;
	SWAP_SD4(jnl_alq);
	SWAP_SD4(jnl_deq);
	SWAP_SD4(jnl_buffer_size);
	SWAP_SD4(jnl_before_image);
	new->jnl_state = jnl_closed;
	if (!new_is_native)
		SWAP_SD4(jnl_state);
	SWAP_SD4(jnl_file_len);
	SWAP_SD4(autoswitchlimit);
	SWAP_SD4(epoch_interval);
	SWAP_SD4(alignsize);
	SWAP_SD4(jnl_sync_io);
	SWAP_SD4(yield_lmt);
	assert(SIZEOF(gtm_int64_t) == SIZEOF(old->intrpt_recov_resync_seqno));
	SWAP_SD8(intrpt_recov_resync_seqno);
	assert(SIZEOF(int4) == SIZEOF(old->intrpt_recov_tp_resolve_time));
	SWAP_SD4(intrpt_recov_tp_resolve_time);
	SWAP_SD4(recov_interrupted);
	SWAP_SD4(intrpt_recov_jnl_state);
	SWAP_SD4(intrpt_recov_repl_state);
	for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
	{
		SWAP_SD8(strm_reg_seqno[idx]);
		SWAP_SD8(intrpt_recov_resync_strm_seqno[idx]);
		SWAP_SD8(save_strm_reg_seqno[idx]);
	}
	SWAP_SD4(is_encrypted);
#define TAB_BG_TRC_REC(A,B)	new->B##_cntr = (bg_trc_rec_cntr) 0; new->B##_tn = (bg_trc_rec_tn) 0;
#include "tab_bg_trc_rec.h"
#undef TAB_BG_TRC_REC
#define TAB_DB_CSH_ACCT_REC(A,B,C)	new->A.cumul_count = new->A.curr_count = 0;
#include "tab_db_csh_acct_rec.h"
#undef TAB_DB_CSH_ACCT_REC
	SWAP_SD4_CAST(creation_db_ver, enum db_ver);
	SWAP_SD4_CAST(creation_mdb_ver, enum mdb_ver);
	SWAP_SD4_CAST(certified_for_upgrade_to, enum db_ver);
	/* next_upgrd_warn	isn't valid since the database is fully_upgraded
	   and the latch values differ by platform and since we don't know where
	   the db will be used, we will ignore it.
	*/
}

int4	endian_process(endian_info *info, sgmnt_data *new_data, sgmnt_data *old_data)
{	/* returns 0 for success
	   This routine based on mubinccpy and dbcertify_scan_phase
	*/
	int4			startvbn;
	int			save_errno, bsize, lbmap_cnt, lbm_status;
	int			buff_native, buff_old, buff_new;
	int			mm_offset, lm_offset;
	int4			bplmap;
	uint4			totblks, lbm_done, busy_done, recycled_done, free_done, last_blk_written;
	off_t			dbptr;
	block_id		blk_num;
	boolean_t		new_is_native;
	char			*blk_buff[2], *lbmap_buff[2], *errptr;
#	ifdef GTM_CRYPT
	int			crypt_blk_size, gtmcrypt_errno;
	blk_hdr_ptr_t		bp_new, bp_native;
	boolean_t		blk_needs_encryption;
#	endif
	if (info->endian_native)
	{	/* use fields from old header */
		bplmap = old_data->bplmap;
		totblks = old_data->trans_hist.total_blks;
		lbmap_cnt = (totblks + bplmap - 1) / bplmap;
		bsize = old_data->blk_size;
		startvbn = old_data->start_vbn;
		buff_native = buff_old = 0;
		new_is_native = FALSE;
		buff_new = 1;
	} else
	{	/* use swapped fields from new header */
		bplmap = new_data->bplmap;
		totblks = new_data->trans_hist.total_blks;
		lbmap_cnt = (totblks + bplmap - 1) / bplmap;
		bsize = new_data->blk_size;
		startvbn = new_data->start_vbn;
		buff_native = buff_new = 1;
		new_is_native = TRUE;
		buff_old = 0;
	}
	GTMCRYPT_ONLY(assert((0 != info->database_fn_len) || !is_encrypted));
	dbptr = (off_t)(startvbn - 1) * DISK_BLOCK_SIZE;
	info->tot_blks = totblks;
	info->bsize = bsize;
	info->startvbn = startvbn;
	blk_buff[0] = malloc(bsize);
	blk_buff[1] = malloc(bsize);
	lbmap_buff[0] = malloc(bsize);
	lbmap_buff[1] = malloc(bsize);
	blk_num = last_blk_written = lbm_done = busy_done = recycled_done = free_done = 0;
	for (mm_offset = 0; (mm_offset < lbmap_cnt) && (blk_num < totblks); ++mm_offset)
	{	/* for each local bit map */
		assert(0 == (blk_num % bplmap));	/* check proper local bit map alignment */
		LSEEKREAD(info->db_fd, dbptr, lbmap_buff[buff_old], bsize, save_errno);
		if (0 != save_errno)
		{
			free(blk_buff[0]);
			free(lbmap_buff[0]);
			free(blk_buff[1]);
			free(lbmap_buff[1]);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("Error reading local bit map block !UL : !AZ", TRUE, blk_num, errptr);
			return save_errno;
		}
		memcpy(lbmap_buff[buff_new], lbmap_buff[buff_old], bsize);
		endian_cvt_blk_hdr((blk_hdr_ptr_t)lbmap_buff[buff_new], new_is_native, FALSE);
		assert(LCL_MAP_LEVL == ((blk_hdr_ptr_t)lbmap_buff[buff_native])->levl);
		/* set all recycled bits to free to avoid trouble if pre GDSV6 */
		/* lm_offset 0 is the local bit map itself */
		for (lm_offset = 1; lm_offset < bplmap && (blk_num + lm_offset) < totblks; lm_offset++)
		{
			GET_BM_STATUS(lbmap_buff[buff_new], lm_offset, lbm_status);
			if (BLK_RECYCLED == lbm_status)
			{
				SET_BM_STATUS(lbmap_buff[buff_new], lm_offset, BLK_FREE);
				recycled_done++;
			} else if (BLK_FREE == lbm_status)
				free_done++;		/* count before changing recycled to free */
			else if (BLK_MAPINVALID == lbm_status)
				GTMASSERT;
		}
		if (info->inplace)
		{
			DB_LSEEKWRITE(NULL, NULL, info->db_fd, dbptr, lbmap_buff[buff_new], bsize, save_errno);
		} else
			LSEEKWRITE(info->outdb_fd, dbptr, lbmap_buff[buff_new], bsize, save_errno);
		if (0 != save_errno)
		{
			free(blk_buff[0]);
			free(lbmap_buff[0]);
			free(blk_buff[1]);
			free(lbmap_buff[1]);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("Error writing local bit map block !UL : !AZ", TRUE, blk_num, errptr);
			return save_errno;
		}
		last_blk_written = blk_num;
		lbm_done++;
		/* lm_offset 0 is the local bit map itself */
		for (lm_offset = 1, dbptr += bsize, blk_num++;
			(blk_num < totblks) && (lm_offset < bplmap);
			lm_offset++, dbptr += bsize, blk_num++)
		{	/* for each local bit map entry - there will only be busy or free blocks in the (new) database */
			GET_BM_STATUS(lbmap_buff[buff_new], lm_offset, lbm_status);
			if (BLK_BUSY == lbm_status)
			{
				LSEEKREAD(info->db_fd, dbptr, blk_buff[buff_old], bsize, save_errno);
				if (0 != save_errno)
				{
					free(blk_buff[0]);
					free(lbmap_buff[0]);
					free(blk_buff[1]);
					free(lbmap_buff[1]);
					errptr = (char *)STRERROR(save_errno);
					util_out_print("Error reading block !UL : !AZ", TRUE, blk_num, errptr);
					return save_errno;
				}
				memcpy(blk_buff[buff_new], blk_buff[buff_old], bsize);

#				ifdef GTM_CRYPT
				if (is_encrypted)
				{
					ASSERT_ENCRYPTION_INITIALIZED;
					bp_new = (blk_hdr_ptr_t)blk_buff[buff_new];
					bp_native = (blk_hdr_ptr_t)blk_buff[buff_native];
					if (new_is_native)
						endian_cvt_blk_hdr(bp_new, new_is_native, BLK_RECYCLED == lbm_status);
					assert((bp_new->bsiz <= bsize) && (bp_new->bsiz >= SIZEOF(*bp_new)));
					crypt_blk_size = MIN(bsize, bp_new->bsiz) - (SIZEOF(*bp_new));
					blk_needs_encryption = BLK_NEEDS_ENCRYPTION(bp_new->levl, crypt_blk_size);
					if (blk_needs_encryption)
					{
						GTMCRYPT_DECRYPT(NULL, encr_key_handle, (char *)(bp_new + 1), crypt_blk_size, NULL,
									gtmcrypt_errno);
						if (0 != gtmcrypt_errno)
						{
							GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, info->database_fn_len,
											info->database_fn);
							return gtmcrypt_errno;
						}
					}
					if (!new_is_native)
						endian_cvt_blk_hdr(bp_new,
								   new_is_native,
								   BLK_RECYCLED == lbm_status);
					endian_cvt_blk_recs(info, (char *)bp_new, bp_native, blk_num);
					if (blk_needs_encryption)
					{
						GTMCRYPT_ENCRYPT(NULL, encr_key_handle, (char *)(bp_new + 1), crypt_blk_size, NULL,
								     gtmcrypt_errno);
						if (0 != gtmcrypt_errno)
						{
							GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, info->database_fn_len,
											info->database_fn);
							return gtmcrypt_errno;
						}
					}
				} else
				{
#				endif
					endian_cvt_blk_hdr((blk_hdr_ptr_t)blk_buff[buff_new],
								new_is_native, BLK_RECYCLED == lbm_status);
					endian_cvt_blk_recs(info, blk_buff[buff_new],
								(blk_hdr_ptr_t)blk_buff[buff_native], blk_num);
#				ifdef GTM_CRYPT
				}
#				endif
				if (info->inplace)
				{
					DB_LSEEKWRITE(NULL, NULL, info->db_fd, dbptr, blk_buff[buff_new], bsize, save_errno);
				} else
					LSEEKWRITE(info->outdb_fd, dbptr, blk_buff[buff_new], bsize, save_errno);
				if (0 != save_errno)
				{
					free(blk_buff[0]);
					free(lbmap_buff[0]);
					free(blk_buff[1]);
					free(lbmap_buff[1]);
					errptr = (char *)STRERROR(save_errno);
					util_out_print("Error writing block !UL : !AZ", TRUE, blk_num, errptr);
					return save_errno;
				}
				last_blk_written = info->last_blk_cvt = blk_num;
				if (BLK_BUSY == lbm_status)
					busy_done++;
			}
		}
	}
	if (last_blk_written < totblks)
	{	/* need to create last disk block */
		memset(blk_buff[0], 0, DISK_BLOCK_SIZE);
		dbptr = ((off_t)(startvbn - 1) * DISK_BLOCK_SIZE) + ((off_t)totblks * bsize);
		if (info->inplace)
		{
			DB_LSEEKWRITE(NULL, NULL, info->db_fd, dbptr, blk_buff[0], DISK_BLOCK_SIZE, save_errno);
		} else
			LSEEKWRITE(info->outdb_fd, dbptr, blk_buff[0], DISK_BLOCK_SIZE, save_errno);
		if (0 != save_errno)
		{
			free(blk_buff[0]);
			free(lbmap_buff[0]);
			free(blk_buff[1]);
			free(lbmap_buff[1]);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("Error writing last block : !AZ", TRUE, errptr);
			return save_errno;
		}
	}
	free(lbmap_buff[0]);
	free(lbmap_buff[1]);
	free(blk_buff[0]);
	free(blk_buff[1]);
	if (NULL != info->dtblk.buff)
	{
		free(info->dtblk.buff);
		info->dtblk.buff = NULL;
		info->dtblk.blkid = -1;
	}
	if (NULL != info->dtblk.dtrbuff)
	{
		free(info->dtblk.dtrbuff);
		info->dtblk.dtrbuff = NULL;
	}
	return 0;
}

void endian_cvt_blk_hdr(blk_hdr_ptr_t blkhdr, boolean_t new_is_native, boolean_t make_empty)
{	/* convert fields in block header */
	uint4		v15bsiz, v15levl, bsiz;
	v15_trans_num	v15tn;
	trans_num	tn;
	unsigned short	bver;

	v15bsiz = blkhdr->bver;
	blkhdr->bver = GTM_BYTESWAP_16(blkhdr->bver);
	if (new_is_native)
		v15bsiz = blkhdr->bver;		/* now it is native endian */
	if (SIZEOF(v15_blk_hdr) <= v15bsiz)
	{	/* old format block so it must be recycled and not upgraded */
		assert(FALSE);		/* should have been changed to FREE  above */
		assert(make_empty);
		v15levl = ((v15_blk_hdr *)blkhdr)->levl;
		v15tn = ((v15_blk_hdr *)blkhdr)->tn;
		assert(SIZEOF(char) == SIZEOF(blkhdr->levl));	/* no need to swap */
		blkhdr->levl = v15levl;
		bver = GDSV6;
		bsiz = SIZEOF(v15_blk_hdr);
		if (!new_is_native)
		{
			tn = ((v15_blk_hdr *)blkhdr)->tn;	/* expand while native; */
			blkhdr->tn = GTM_BYTESWAP_64(tn);
			bsiz = GTM_BYTESWAP_32(bsiz);
			bver = GTM_BYTESWAP_16(bver);
		} else
		{
			v15tn = GTM_BYTESWAP_32(v15tn);
			blkhdr->tn = v15tn;			/* expand while native */
		}
		blkhdr->bver = bver;
		blkhdr->bsiz = bsiz;
		return;
	}
	assert(SIZEOF(char) == SIZEOF(blkhdr->levl));	/* no need to swap */
	if (make_empty)
		blkhdr->bsiz = new_is_native ? SIZEOF(blk_hdr) : GTM_BYTESWAP_32(SIZEOF(blk_hdr));
	else
		blkhdr->bsiz = GTM_BYTESWAP_32(blkhdr->bsiz);
	blkhdr->tn = GTM_BYTESWAP_64(blkhdr->tn);
	return;
}

char *endian_read_dbblk(endian_info *info, block_id blk_to_get)
{
	off_t		blkoff;
	int		save_errno;
	boolean_t	blk_is_native;
	char		*buff;
#	ifdef GTM_CRYPT
	int		gtmcrypt_errno;
	int		req_dec_blk_size;
	char		*inbuf;
	blk_hdr_ptr_t	bp;
#	endif

	if (DIR_ROOT == blk_to_get)
	{
		if (NULL == info->dtblk.dtrbuff)
		{	/* need to really get it */
			info->dtblk.dtrbuff = malloc(info->bsize);
			buff = info->dtblk.dtrbuff;
		} else	/* already have it */
			return info->dtblk.dtrbuff;
	} else
	{
		if (NULL == info->dtblk.buff)
		{
			info->dtblk.buff = malloc(info->bsize);
			info->dtblk.blkid = -1;		/* invalid */
		} else if (blk_to_get == info->dtblk.blkid)
			return info->dtblk.buff;	/* already have it */
		buff = info->dtblk.buff;
	}
	blkoff = ((off_t)(info->startvbn - 1) * DISK_BLOCK_SIZE) + ((off_t)blk_to_get * info->bsize);
	LSEEKREAD(info->db_fd, blkoff, buff, info->bsize, save_errno);
	if (0 != save_errno)
	{
		return NULL;
	}
	if (info->inplace && info->last_blk_cvt >= blk_to_get)
		blk_is_native = !info->endian_native;	/* already converted */
	else
		blk_is_native = info->endian_native;	/* still original endian */
	if (!blk_is_native)
		endian_cvt_blk_hdr((blk_hdr_ptr_t)buff, TRUE, FALSE);
#	ifdef GTM_CRYPT
	if (is_encrypted)
	{
		bp = (blk_hdr_ptr_t)buff;
		assert((bp->bsiz <= info->bsize) && (bp->bsiz >= SIZEOF(*bp)));
		req_dec_blk_size = MIN(info->bsize, bp->bsiz) - (SIZEOF(*bp));
		if (BLOCK_REQUIRE_ENCRYPTION(is_encrypted, bp->levl, req_dec_blk_size))
		{
			ASSERT_ENCRYPTION_INITIALIZED;
			inbuf = (char *)(bp + 1);
			GTMCRYPT_DECRYPT(NULL, encr_key_handle, inbuf, req_dec_blk_size, NULL, gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, info->database_fn, info->database_fn_len);
				return NULL;
			}
		}
	}
#	endif
	if (DIR_ROOT == blk_to_get)
		info->dtblk.dtrnative = blk_is_native;
	else
	{
		info->dtblk.blkid = blk_to_get;
		info->dtblk.native = blk_is_native;
	}
	return buff;
}

void endian_find_key(endian_info *info, end_gv_key *targ_gv_key, char *rec_p, int rec_len, int blk_levl)
{	/* find the key for the record and set targ_gv_key */
	int		cmpc;
	int		tmp_cmpc;
	unsigned char	*targ_key;
	char		*rec_key;

	if (BSTAR_REC_SIZE == rec_len && 0 < blk_levl)
	{	/* no key for star key records */
		targ_gv_key->end = 0;
		return;
	}
	cmpc = EVAL_CMPC((rec_hdr_ptr_t)rec_p);
	targ_key = targ_gv_key->key + cmpc;
	rec_key = rec_p + SIZEOF(rec_hdr);
	while (TRUE)
	{
		for (; *rec_key; ++targ_key, ++rec_key)
			*targ_key = *rec_key;
		if (0 == *(rec_key + 1))
		{	/* end of key since two nulls */
			*targ_key++ = 0;	/* end the target key */
			*targ_key = 0;		/* with two as well */
			targ_gv_key->end = (unsigned int)(targ_key - targ_gv_key->key);
			break;
		}
		/* Else, copy subscript separator char and keep scanning */
		*targ_key++ = *rec_key++;
		assert((rec_key - rec_p) < rec_len);
	}
	assert(cmpc <= targ_gv_key->end);
	return;
}

boolean_t	endian_match_key(end_gv_key *gv_key1, int blk_levl, end_gv_key *gv_key2)
{
	unsigned char	*key1, *key2;
	int	key1_len, key2_len, key_len;

	key1 = gv_key1->key;
	key2 = gv_key2->key;
	key1_len = gv_key1->end + 1;
	if (1 == key1_len && 0 < blk_levl)
		return TRUE;	/* a star key record is greater than the second key */
	assert(1 < key1_len);
	key2_len = gv_key2->end + 1;
	assert(1 < key2_len);		/* should never look for star key */
	key_len = MIN(key1_len, key2_len);
	for (; key_len; key1++, key2++, key_len--)
	{
		if (*key1 != *key2)
			break;
	}
	if ((0 == key_len && key1_len >= key2_len) || (0 != key_len && *key1 > *key2))
		return TRUE;
	return FALSE;
}

/* find the directory tree leaf block for a key to check if a level zero
   block is in the DT or GVT.  The need for this should be rare so little
   attempt is made at efficiency other than caching the DT root block
   since it is the start of all searches.
   */
block_id	endian_find_dtblk(endian_info *info, end_gv_key *gv_key)
{
	block_id	blk_to_get, blk_ptr;
	int		save_errno, rec_len, blk_levl, ptroffset;
	int		tmp_cmpc;
	boolean_t	blk_is_native;
	char		*buff, *blk_top, *rec_p;
	unsigned short	us_rec_len;
	end_gv_key	found_gv_key;
	unsigned char	found_gv_key_buff[MAX_KEY_SZ + 1];

	info->dtblk.count++;
	found_gv_key.key = found_gv_key_buff;
	found_gv_key.end = found_gv_key.top = found_gv_key.gvn_len = 0;
	blk_to_get = DIR_ROOT;
	buff = endian_read_dbblk(info, blk_to_get);	/* will use dtrbuff after first time */
	if (!buff)
		return -1;
	blk_is_native = info->dtblk.dtrnative;
	while (TRUE)
	{
		blk_top = buff + ((blk_hdr_ptr_t)buff)->bsiz;
		blk_levl = ((blk_hdr_ptr_t)buff)->levl;
		rec_p = buff + SIZEOF(blk_hdr);
		while (rec_p < blk_top)
		{
			GET_USHORT(us_rec_len, &((rec_hdr *)rec_p)->rsiz);
			if (!blk_is_native)
				us_rec_len = GTM_BYTESWAP_16(us_rec_len);
			rec_len = us_rec_len;
			if (0 >= rec_len)
				return -1;
			if (0 != blk_levl && BSTAR_REC_SIZE == rec_len)
			{	/* down to the next level */
				GET_ULONG(blk_ptr, rec_p + SIZEOF(rec_hdr));
				if (!blk_is_native)
					blk_ptr = GTM_BYTESWAP_32(blk_ptr);
				if (blk_ptr > info->tot_blks)
					return -1;	/* past end of database */
				blk_to_get = blk_ptr;
				break;
			}
			endian_find_key(info, &found_gv_key, rec_p, rec_len, blk_levl);
			if (endian_match_key(&found_gv_key, blk_levl, gv_key))
			{
				if (0 == blk_levl)
					return blk_to_get;	/* found dtleaf block we are looking for */
				ptroffset = found_gv_key.end - EVAL_CMPC((rec_hdr *)rec_p) + 1;
				GET_ULONG(blk_ptr, (rec_p + SIZEOF(rec_hdr) + ptroffset));
				if (!blk_is_native)
					blk_ptr = GTM_BYTESWAP_32(blk_ptr);
				if (blk_ptr > info->tot_blks)
					return -1;	/* past end of database */
				blk_to_get = blk_ptr;
				break;
			}
			rec_p = rec_p + rec_len;
		}
		if (0 == blk_levl)
			return -1;		/* we didn't find what should have been there */
		buff = endian_read_dbblk(info, blk_to_get);
		if (!buff)
			return -1;
		blk_is_native = info->dtblk.native;
	}
}

void endian_cvt_blk_recs(endian_info *info, char *new_block, blk_hdr_ptr_t blkhdr, int blknum)
{	/* convert records in new_block, could be data, index, or directory
	   use converted header fields from blkhdr which is in native format */
	int		rec1_len, rec1_gvn_len, rec2_cmpc, rec2_len;
	int		tmp_cmpc;
	int		blk_levl;
	block_id	ptr2blk, ptr2blk_swap, dtblk;
	boolean_t	new_is_native, have_dt_blk;
	unsigned short	us_rec_len, us_rec_len_swap;
	unsigned char	*rec1_ptr, *rec2_ptr, *blk_top, *key_top;
	boolean_t	have_gvtleaf;
	end_gv_key	gv_key;

	if (SIZEOF(v15_blk_hdr) <= blkhdr->bver)
		return;		/* pre V5 version so ignore records */
	new_is_native = !info->endian_native;
	blk_levl = blkhdr->levl;
	blk_top = (unsigned char *)new_block + blkhdr->bsiz;
	rec1_ptr = (unsigned char *)new_block + SIZEOF(blk_hdr);
	GET_USHORT(us_rec_len, &((rec_hdr *)rec1_ptr)->rsiz);
	rec1_len = new_is_native ? GTM_BYTESWAP_16(us_rec_len) : us_rec_len;
	/* May not need this whole thing, just do what dump_record does
	   and check if block_id follows keys - but what if data is 4 chars */
	/* need to check there really is a 2nd record */
	rec2_ptr = rec1_ptr + rec1_len;
	if (rec2_ptr < blk_top)
		rec2_cmpc = EVAL_CMPC((rec_hdr *)rec2_ptr);
	else
		rec2_cmpc = -1;		/* no second record */

	/* Determine type of block (DT lvl 0, DT lvl !0, GVT lvl 0, GVT lvl !0)

	    Rules for checking (from dbcertify_scan_phase.c):

	    1) If compression count of 2nd record is zero, it *must* be a directory tree block. This is a fast path
	       check to avoid doing the strlen in the second check.

	    2) If compression count of second record is less than or equal to the length of the global variable name,
	       then this must be a directory tree block. The reason this check works is a GVT index or data block
	       would have same GVN in the 2nd record as the first so the compression count would be a minimum of
	       (length(GVN) + 1). The "+ 1" is for the terminating null of the GVN.

	    dbcertify only cares about too full blocks so the above rules may not apply in all other cases.
	    endian cvt only care about index (levl > 0), dtleaf, or gvtleaf.
	*/
	have_dt_blk = FALSE;
	if (0 == rec2_cmpc)
		have_dt_blk = TRUE;
	else
	{
		rec1_gvn_len = STRLEN((char *)rec1_ptr + SIZEOF(rec_hdr));
		if (-1 != rec2_cmpc && rec2_cmpc <= rec1_gvn_len)
			have_dt_blk = TRUE;
	}
	if (have_dt_blk)
		have_gvtleaf = FALSE;			/* Could be dtleaf, dtindex, or dtroot but not gvtleaf */
	else if (-1 != rec2_cmpc)
	{	/* more than one record */
		if (0 == blk_levl)
			have_gvtleaf = TRUE;		/* gdsblk_gvtleaf only sure if more than one record */
		else	/* ambiguous whether gvtroot or gvtindex */
			have_gvtleaf = FALSE;
	} else if (blk_levl)
		have_gvtleaf = FALSE;			/* gdsblk_gvtindex at least not leaf */
	else
	{	/* only one record and level is 0 */
		/* if subscripts at level 0, it must be gvtleaf */
		/* find key_top to check if it has four bytes of data which look like a valid pointer */
		/* note dtleaf may have collation info after the pointer */
		for (key_top = rec1_ptr + SIZEOF(rec_hdr); *key_top && key_top < (rec1_ptr + rec1_len); key_top++)
			;
		if (*++key_top)
			have_gvtleaf = TRUE;		/* gdsblk_gvtleaf subscript so must be */
		else if (SIZEOF(block_id) <= ((rec1_ptr + rec1_len) - ++key_top) &&
			 (SIZEOF(block_id) + MAX_SPEC_TYPE_LEN) >= ((rec1_ptr + rec1_len) - key_top))
		{	/* record value long enough for block_id but not longer than block_id plus collation information */
			GET_LONG(ptr2blk, key_top);
			if (new_is_native)
				ptr2blk = GTM_BYTESWAP_32(ptr2blk);
			if (ptr2blk <= info->tot_blks)
			{	/* might be a pointer so need to check the hard way */
				gv_key.key = rec1_ptr + SIZEOF(rec_hdr);
				gv_key.top = gv_key.end = gv_key.gvn_len = (unsigned int)(key_top - gv_key.key - 1);
				dtblk = endian_find_dtblk(info, &gv_key);
				if (dtblk != blknum)
					have_gvtleaf = TRUE;	/* blknum is not DT level 0 */
				else
					have_gvtleaf = FALSE;	/* DT level 0 has pointers */
			} else	/* points after last block so not a block_id */
				have_gvtleaf = TRUE;		/* gdsblk_gvtleaf should be data */
		} else
			have_gvtleaf = TRUE;		/* gdsblk_gvtleaf too short for pointer or too long with collation info */
	}
	for (; rec1_ptr < blk_top; rec1_ptr += rec1_len)
	{
		GET_USHORT(us_rec_len, &((rec_hdr *)rec1_ptr)->rsiz);
		us_rec_len_swap = GTM_BYTESWAP_16(us_rec_len);
		PUT_USHORT(&((rec_hdr *)rec1_ptr)->rsiz, us_rec_len_swap);
		rec1_len = new_is_native ? us_rec_len_swap : us_rec_len;
		/* leave cmpc and cmpc2 bytes alone */
		if (!have_gvtleaf)
		{	/* fix up pointers as well */
			key_top = rec1_ptr + SIZEOF(rec_hdr);
			if (BSTAR_REC_SIZE != rec1_len || 0 == blk_levl)
			{	/* find pointer after subscripts */
				for ( ; key_top < (rec1_ptr + rec1_len); )
					if (!*key_top++ && !*key_top++)
						break;		/* 2 nulls is end of subscripts */
			} else
				assert((key_top + SIZEOF(block_id) == blk_top) || blk_levl);	/* must be last if not leaf */
			assert((key_top + SIZEOF(block_id)) <= (rec1_ptr + rec1_len));
			GET_LONG(ptr2blk, key_top);
			ptr2blk_swap = GTM_BYTESWAP_32(ptr2blk);
			PUT_LONG(key_top, ptr2blk_swap);
#ifdef DEBUG
			if (new_is_native)
				ptr2blk = ptr2blk_swap;
			assert(ptr2blk <= info->tot_blks);
#endif
		}
	}
}
