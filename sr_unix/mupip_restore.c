/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsdbver.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_select.h"

#include <sys/wait.h>
#include <stddef.h>
#include <errno.h>
#ifdef __MVS__
#include <sys/time.h>
#include "gtm_zos_io.h"
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mupipbckup.h"
#include "murest.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "cli.h"
#include "iosp.h"
#include "copy.h"
#include "iob.h"
#include "error.h"
#include "gtmio.h"
#include "iotimer.h"
#include "gtm_pipe.h"
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
#include "mu_outofband_setup.h"
#include "mu_gv_cur_reg_init.h"
#include "mupip_restore.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "db_ipcs_reset.h"
#include "gds_blk_downgrade.h"
#include "shmpool.h"
#include "min_max.h"
#include "gtmxc_types.h"
#include "gtmcrypt.h"
#include "jnl.h"
#include "anticipatory_freeze.h"
#include "db_write_eof_block.h"

GBLDEF	inc_list_struct		in_files;
GBLREF	uint4			pipe_child;
GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			restore_read_errno;

LITREF	char			*gtm_dbversion_table[];
LITREF	char			*mdb_ver_names[];

error_def(ERR_BADTAG);
error_def(ERR_IOEOF);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUPRESTERR);
error_def(ERR_TEXT);

#define COMMON_READ(S, BUFF, LEN, INBUF)		\
{							\
	assert(BACKUP_TEMPFILE_BUFF_SIZE >= LEN);	\
	(*common_read)(S, BUFF, LEN);			\
	if (0 != restore_read_errno)			\
		CLNUP_AND_EXIT(ERR_MUPRESTERR, INBUF);	\
}

#define CLNUP_AND_EXIT(EXIT_STATUS, INBUF)				\
{									\
	if (INBUF)							\
		free(INBUF);						\
	assert(FILE_INFO(gv_cur_region)->grabbed_access_sem);		\
	db_ipcs_reset(gv_cur_region);					\
	mu_gv_cur_reg_free();						\
	mupip_exit(EXIT_STATUS);					\
}

void mupip_restore(void)
{
	static readonly char	label[] =   GDS_LABEL;
	char			db_name[MAX_FN_LEN + 1], *inbuf, *p, *blk_ptr;
	inc_list_struct 	*ptr;
	inc_header		inhead;
	sgmnt_data		old_data;
	unsigned short		n_len;
	int4			status, rsize, temp, save_errno, old_start_vbn;
	uint4			rest_blks, totblks;
	trans_num		curr_tn;
	uint4			ii;
	block_id		blk_num;
	boolean_t		extend;
	uint4			cli_status;
	BFILE			*in;
	int			i, db_fd;
	uint4			old_blk_size, orig_size, size, old_tot_blks, bplmap, old_bit_maps, new_bit_maps;
	off_t			new_eof, offset;
	off_t			new_size;
	char			msg_buffer[1024], *newmap;
	mstr			msg_string;
	char			addr[SA_MAXLEN + 1];
	unsigned char		tcp[5];
	backup_type		type;
	unsigned short		port;
	int4			timeout, cut, match;
	void			(*common_read)();
	char			*errptr;
	pid_t			waitpid_res;
	muinc_blk_hdr_ptr_t	sblkh_p;
	int			rc;
	char			*inptr;
	int			in_len, gtmcrypt_errno;
	boolean_t		same_encr_settings;
	boolean_t		check_mdb_ver, bad_mdb_ver;
	boolean_t		in_is_encrypted, in_to_be_encrypted;
	boolean_t		old_is_encrypted, old_to_be_encrypted;
	boolean_t		in_use_new_key;
	enc_handles		in_encr_handles, old_encr_handles;
	int4			cur_mdb_ver;
	gd_segment		*seg;
	unix_db_info		*udi;
	sgmnt_data_ptr_t	csd;
	ZOS_ONLY(int 		realfiletag;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inbuf = NULL;
	extend = TRUE;
	if (CLI_NEGATED == (cli_status = cli_present("EXTEND")))
		extend = FALSE;
	mu_outofband_setup();
	mu_gv_cur_reg_init();
	n_len = SIZEOF(db_name);
	if (cli_get_str("DATABASE", db_name, &n_len) == FALSE)
		mupip_exit(ERR_MUPCLIERR);
	strcpy((char *)gv_cur_region->dyn.addr->fname, db_name);
	gv_cur_region->dyn.addr->fname_len = n_len;
	if (!STANDALONE(gv_cur_region))
	{
		util_out_print("Error securing stand alone access to output file !AD. Aborting restore.", TRUE, n_len, db_name);
		mupip_exit(ERR_MUPRESTERR);
	}
	udi = FILE_INFO(gv_cur_region);
	seg = gv_cur_region->dyn.addr;
	OPENFILE_DB(db_name, O_RDWR, udi, seg);
	db_fd = udi->fd;
	if (FD_INVALID == db_fd)
	{
		save_errno = errno;
		util_out_print("Error accessing output file !AD. Aborting restore.", TRUE, n_len, db_name);
		errptr = (char *)STRERROR(save_errno);
		util_out_print("open : !AZ", TRUE, errptr);
		CLNUP_AND_EXIT(save_errno, NULL);
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(db_fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(db_name, realfiletag, TAG_BINARY, errno);
#	endif
	murgetlst();
	csd = !udi->fd_opened_with_o_direct ? &old_data : (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned;
	DB_LSEEKREAD(udi, db_fd, 0, csd, SGMNT_HDR_LEN, save_errno);
	if (0 != save_errno)
	{
		util_out_print("Error accessing output file !AD. Aborting restore.", TRUE, n_len, db_name);
		if (-1 != save_errno)
		{
			errptr = (char *)STRERROR(save_errno);
			util_out_print("read : !AZ", TRUE, errptr);
			CLNUP_AND_EXIT(save_errno, NULL);
		} else
			CLNUP_AND_EXIT(ERR_IOEOF, NULL);
	}
	if (udi->fd_opened_with_o_direct)
		memcpy(&old_data, csd, SGMNT_HDR_LEN);
	if (memcmp(old_data.label, label, GDS_LABEL_SZ))
	{
		util_out_print("Output file !AD has an unrecognizable format", TRUE, n_len, db_name);
		CLNUP_AND_EXIT(ERR_MUPRESTERR, NULL);
	}
	CHECK_DB_ENDIAN(&old_data, n_len, db_name);
	curr_tn = old_data.trans_hist.curr_tn;
	old_blk_size = old_data.blk_size;
	old_tot_blks = old_data.trans_hist.total_blks;
	old_start_vbn = old_data.start_vbn;
	bplmap = old_data.bplmap;
	old_bit_maps = DIVIDE_ROUND_UP(old_tot_blks, bplmap);
	inbuf = (char *)malloc(BACKUP_TEMPFILE_BUFF_SIZE);
	sblkh_p = (muinc_blk_hdr_ptr_t)inbuf;
	msg_string.addr = msg_buffer;
	msg_string.len = SIZEOF(msg_buffer);
	memset(&inhead, 0, SIZEOF(inc_header));
	rest_blks = 0;
	for (ptr = in_files.next; NULL != ptr; ptr = ptr->next)
	{	/* --- determine source type --- */
		type = backup_to_file;
		if (0 == ptr->input_file.len)
			continue;
		else if ('|' == *(ptr->input_file.addr + ptr->input_file.len - 1))
		{
			type = backup_to_exec;
			ptr->input_file.len--;
			*(ptr->input_file.addr + ptr->input_file.len) = '\0';
		} else if (ptr->input_file.len > 5)
		{
			lower_to_upper(tcp, (uchar_ptr_t)ptr->input_file.addr, 5);
			if (0 == memcmp(tcp, "TCP:/", 5))
			{
				type = backup_to_tcp;
				cut = 5;
				while ('/' == *(ptr->input_file.addr + cut))
					cut++;
				ptr->input_file.len -= cut;
				p = ptr->input_file.addr;
				while (p < ptr->input_file.addr + ptr->input_file.len)
				{
					*p = *(p + cut);
					p++;
				}
				*p = '\0';
			}
		}
		/* --- open the input stream --- */
		restore_read_errno = 0;
		switch (type)
		{
			case backup_to_file:
				common_read = iob_read;
				if ((in = iob_open_rd(ptr->input_file.addr, DISK_BLOCK_SIZE, BLOCKING_FACTOR)) == NULL)
				{
					save_errno = errno;
					util_out_print("Error accessing input file !AD. Aborting restore.", TRUE,
						ptr->input_file.len, ptr->input_file.addr);
					errptr = (char *)STRERROR(save_errno);
					util_out_print("open : !AZ", TRUE, errptr);
					CLNUP_AND_EXIT(save_errno, inbuf);
				}
				break;
			case backup_to_exec:
				pipe_child = 0;
				common_read = exec_read;
				in = (BFILE *)malloc(SIZEOF(BFILE));
				if (0 > (in->fd = gtm_pipe(ptr->input_file.addr, input_from_comm)))
				{
					util_out_print("Error creating input pipe from !AD.",
						TRUE, ptr->input_file.len, ptr->input_file.addr);
					CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
#				ifdef DEBUG_ONLINE
				DBGFPF(stdout, "file descriptor for the openned pipe is %d.\n", in->fd);
				DBGFPF(stdout, "the command passed to gtm_pipe is %s.\n", ptr->input_file.addr);
#				endif
				break;
			case backup_to_tcp:
				common_read = tcp_read;
				/* parse the input */
				switch (match = SSCANF(ptr->input_file.addr, "%[^:]:%hu", addr, &port))
				{
					case 1 :
						port = DEFAULT_BKRS_PORT;
					case 2 :
						break;
					default :
						util_out_print("Error : A hostname has to be specified.", TRUE);
						CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
				assert(SIZEOF(timeout) == SIZEOF(int));
				if ((0 == cli_get_int("NETTIMEOUT", (int4 *)&timeout)) || (0 > timeout))
					timeout = DEFAULT_BKRS_TIMEOUT;
				in = (BFILE *)malloc(SIZEOF(BFILE));
				if (0 > (in->fd = tcp_open(addr, port, timeout, TRUE)))
				{
					util_out_print("Error establishing TCP connection to !AD.",
						TRUE, ptr->input_file.len, ptr->input_file.addr);
					CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
				break;
			default:
				util_out_print("Aborting restore!/", TRUE);
				util_out_print("Unrecognized input format !AD", TRUE, ptr->input_file.len, ptr->input_file.addr);
				CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
		COMMON_READ(in, &inhead, SIZEOF(inc_header), inbuf);
		check_mdb_ver = FALSE;
		if (!memcmp(inhead.label, INC_HEADER_LABEL_V5_NOENCR, INC_HDR_LABEL_SZ))
			assert(!IS_ENCRYPTED(inhead.is_encrypted));
		else if (!memcmp(inhead.label, INC_HEADER_LABEL_V6_ENCR, INC_HDR_LABEL_SZ))
			assert(IS_ENCRYPTED(inhead.is_encrypted));
		else if (!memcmp(inhead.label, INC_HEADER_LABEL_V7, INC_HDR_LABEL_SZ))
			check_mdb_ver = TRUE;
		else
		{
			util_out_print("Input file !AD has an unrecognizable format", TRUE, ptr->input_file.len,
				ptr->input_file.addr);
			CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
		if (check_mdb_ver)
		{
			COMMON_READ(in, (char *)&cur_mdb_ver, SIZEOF(int4), inbuf);
			bad_mdb_ver = (old_data.minor_dbver != cur_mdb_ver);
		} else
		{
			cur_mdb_ver = -1;
			bad_mdb_ver = TRUE;
		}
		if (bad_mdb_ver)
		{
			if (0 > cur_mdb_ver)
				util_out_print("Minor DB version in the extract predates that in the database (!AD)", TRUE,
						LEN_AND_STR(mdb_ver_names[old_data.minor_dbver]));
			else if (GDSMVLAST <= cur_mdb_ver)
				util_out_print("Minor DB version in the extract is higher than in the database (!AD)", TRUE,
						LEN_AND_STR(mdb_ver_names[old_data.minor_dbver]));
			else
				util_out_print("Minor DB version in the extract (!AD) is different from that in the database (!AD)",
						TRUE, LEN_AND_STR(mdb_ver_names[cur_mdb_ver]),
						LEN_AND_STR(mdb_ver_names[old_data.minor_dbver]));
			CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
		if (!SAME_ENCRYPTION_SETTINGS(&inhead, &old_data))
		{
			same_encr_settings = FALSE;
			INIT_DB_OR_JNL_ENCRYPTION(&in_encr_handles, &inhead, 0, NULL, gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, ptr->input_file.len, ptr->input_file.addr);
				CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
			}
			INIT_DB_OR_JNL_ENCRYPTION(&old_encr_handles, &old_data, seg->fname_len, seg->fname, gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
				CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
			}
			in_is_encrypted = IS_ENCRYPTED(inhead.is_encrypted);
			in_to_be_encrypted = USES_NEW_KEY(&inhead);
			old_is_encrypted = IS_ENCRYPTED(old_data.is_encrypted);
			old_to_be_encrypted = USES_NEW_KEY(&old_data);
		} else
			same_encr_settings = TRUE;
		if (curr_tn != inhead.start_tn)
		{
			util_out_print("Transaction in input file !AD does not align with database TN.!/DB: !16@XQ!_"
				       "Input file: !16@XQ", TRUE, ptr->input_file.len, ptr->input_file.addr,
				       &curr_tn, &inhead.start_tn);
			CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
		if (old_blk_size != inhead.blk_size)
		{
			util_out_print("Incompatible block size.  Output file !AD has block size !XL,", TRUE, n_len, db_name,
				       old_blk_size);
			util_out_print("while input file !AD is from a database with block size !XL,", TRUE, ptr->input_file.len,
				ptr->input_file.addr, inhead.blk_size);
			CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
		new_bit_maps = DIVIDE_ROUND_UP(inhead.db_total_blks, bplmap);
		if (old_tot_blks != inhead.db_total_blks)
		{
			if (old_tot_blks > inhead.db_total_blks)
			{
				/* Truncate occurred between incremental backups. FTRUNCATE the db file to the appropriate size,
				 * write the EOF block.
				 */
				new_eof = (off_t)BLK_ZERO_OFF(old_start_vbn) + ((off_t)inhead.db_total_blks * old_blk_size);
				new_size = new_eof + old_blk_size;
				status = db_write_eof_block(udi, db_fd, old_blk_size, new_eof, &(TREF(dio_buff)));
				if (0 != status)
				{
					util_out_print("Aborting restore!/", TRUE);
					util_out_print("lseek or write error : Error accessing output file !AD while truncating.",
						TRUE, n_len, db_name);
					totblks = old_tot_blks - old_bit_maps;
					util_out_print("Output file !AD has!/  !UL (!XL hex) total blocks,",
						TRUE, n_len, db_name, totblks, totblks);
					totblks = inhead.db_total_blks - new_bit_maps;
					util_out_print("while input file !AD is from a database with!/  !UL (!XL hex) total blocks",
						TRUE, ptr->input_file.len, ptr->input_file.addr, totblks, totblks);
					CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
				FTRUNCATE(db_fd, new_size, status);
				if (0 != status)
				{
					util_out_print("Aborting restore!/", TRUE);
					util_out_print("FTRUNCATE error : Error error truncating output file !AD.",
						TRUE, n_len, db_name);
					totblks = old_tot_blks - old_bit_maps;
					util_out_print("Output file !AD has!/  !UL (!XL hex) total blocks,",
						TRUE, n_len, db_name, totblks, totblks);
					totblks = inhead.db_total_blks - new_bit_maps;
					util_out_print("while input file !AD is from a database with!/  !UL (!XL hex) total blocks",
						TRUE, ptr->input_file.len, ptr->input_file.addr, totblks, totblks);
					CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
				old_tot_blks = inhead.db_total_blks;
				old_bit_maps = new_bit_maps;
			} else if (old_tot_blks > inhead.db_total_blks || !extend)
			{
				totblks = old_tot_blks - old_bit_maps;
				util_out_print("Incompatible database sizes.  Output file !AD has!/  !UL (!XL hex) total blocks,",
						TRUE, n_len, db_name, totblks, totblks);
				totblks = inhead.db_total_blks - new_bit_maps;
				util_out_print("while input file !AD is from a database with!/  !UL (!XL hex) total blocks",
						TRUE, ptr->input_file.len, ptr->input_file.addr, totblks, totblks);
				CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
			} else
			{	/* The db must be extended which we will do ourselves (to avoid jnl and other interferences
				 * in gdsfilext). These local bit map blocks will be created in GDSVCURR format (always). The
				 * reason for this is that we do not know at this time whether these blocks will be replaced
				 * by blocks in the backup or not. If we are in compatibility mode, this is highly likely
				 * even if before image journaling is on which creates bit maps with TN=0. In either case,
				 * a GDSVCURR format block is the only one that can be added to the database without affecting
				 * the blks_to_upgrd counter.
				 */
				new_eof = (off_t)BLK_ZERO_OFF(old_start_vbn) + ((off_t)inhead.db_total_blks * old_blk_size);
				status = db_write_eof_block(udi, db_fd, old_blk_size, new_eof, &(TREF(dio_buff)));
				if (0 != status)
				{
					util_out_print("Aborting restore!/", TRUE);
					util_out_print("lseek or write error : Unable to extend output file !AD!/",
												TRUE, n_len, db_name);
					util_out_print("  from !UL (!XL hex) total blocks to !UL (!XL hex) total blocks.!/",
						TRUE, old_tot_blks, old_tot_blks, inhead.db_total_blks, inhead.db_total_blks);
					util_out_print("  Current input file is !AD with !UL (!XL hex) total blocks!/",
						TRUE, ptr->input_file.len, ptr->input_file.addr,
						inhead.db_total_blks, inhead.db_total_blks);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
					CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
				}
				/* --- initialize all new bitmaps, just in case they are not touched later --- */
				if (new_bit_maps > old_bit_maps)
				{
					if (udi->fd_opened_with_o_direct)
					{	/* Align buffers for O_DIRECT */
						DIO_BUFF_EXPAND_IF_NEEDED(udi, old_blk_size, &(TREF(dio_buff)));
						newmap = (TREF(dio_buff)).aligned;
					} else
						newmap = (char *)malloc(old_blk_size);
					bml_newmap((blk_hdr_ptr_t)newmap, BM_SIZE(bplmap), curr_tn);
					for (ii = ROUND_UP(old_tot_blks, bplmap); ii < inhead.db_total_blks; ii += bplmap)
					{
						offset = (off_t)BLK_ZERO_OFF(old_start_vbn) + (off_t)ii * old_blk_size;
						DB_LSEEKWRITE(NULL, udi, NULL, db_fd, offset, newmap, old_blk_size, status);
						if (0 != status)
						{
							util_out_print("Aborting restore!/", TRUE);
							util_out_print("Bitmap 0x!XL initialization error!", TRUE, ii);
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
							free(newmap);
							CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
						}
					}
					if (!udi->fd_opened_with_o_direct)
						free(newmap);
				}
				old_tot_blks = inhead.db_total_blks;
				old_bit_maps = new_bit_maps;
			}
		}
		rsize = SIZEOF(muinc_blk_hdr) + inhead.blk_size;
		for ( ; ; )
		{        /* All records are of fixed size so process until we get to a zeroed record marking the end */
			COMMON_READ(in, inbuf, rsize, inbuf);	/* Note rsize == sblkh_p */
			if (0 == sblkh_p->blkid && FALSE == sblkh_p->valid_data)
			{	/* This is supposed to be the end of list marker (null entry */
				COMMON_READ(in, &rsize, SIZEOF(rsize), inbuf);
				if (SIZEOF(END_MSG) + SIZEOF(int4) == rsize)
				{	/* the length of our secondary check is correct .. now check substance */
					COMMON_READ(in, inbuf, rsize - SIZEOF(int4), inbuf);
					if (0 == MEMCMP_LIT(inbuf, END_MSG))
						break;	/* We are done */
				}
				util_out_print("Invalid information in restore file !AD. Aborting restore.",
					       TRUE, ptr->input_file.len,
					       ptr->input_file.addr);
				assert(FALSE);
				if (backup_to_file == type)
					iob_close(in);
				CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
			}
			rest_blks++;
			blk_num = sblkh_p->blkid;
			/* Between incremental backups, an extend followed by a truncate could have occurred. The block below
			 * would have been truncated, so no need to write it. */
			if (blk_num >= old_tot_blks) /* Should be a bitmap block */
				continue;
			/* For blocks that were read during the main backup phase of stream backup, the blocks are
			   recorded without version (there may even be some garbage blocks in the stream of
			   indeterminate/invalid format if a bitmap was written out prior to the data blocks that
			   were recently allocated in it). For these blocks, we just write out what we have as a
			   full block. For blocks that were written out during the backup as part of the online
			   image processing, these are always recorded in V5 mode. We will rewrite these in the mode
			   they were oringally found on disk (potentially necessitating a downgrade of the block).
			   This allows us to exactly match the blks_to_upgrade counter in the saved file-header without
			   worrying about what blocks were converted (or not) in the interim.
			*/
			blk_ptr = inbuf + SIZEOF(muinc_blk_hdr);
			size = old_blk_size;
			if (GDSNOVER != sblkh_p->use.bkup.ondsk_blkver)
			{	/* Specifically versioned blocks - Put them back in the version they were originally */
				if (GDSV4 == sblkh_p->use.bkup.ondsk_blkver)
				{
					gds_blk_downgrade((v15_blk_hdr_ptr_t)blk_ptr, (blk_hdr_ptr_t)blk_ptr);
					size = (((v15_blk_hdr_ptr_t)blk_ptr)->bsiz + 1) & ~1;
				} else
					size = (((blk_hdr_ptr_t)blk_ptr)->bsiz + 1) & ~1;
			}
			assert((size <= old_blk_size) && (size >= SIZEOF(blk_hdr)));
			in_len = MIN(old_blk_size, size) - SIZEOF(blk_hdr);
			if (!same_encr_settings && IS_BLK_ENCRYPTED(((blk_hdr_ptr_t)blk_ptr)->levl, in_len))
			{
				gtmcrypt_errno = 0;
				in_use_new_key = in_to_be_encrypted
					&& (((blk_hdr_ptr_t)blk_ptr)->tn >= inhead.encryption_hash2_start_tn);
				if (in_use_new_key || in_is_encrypted)
				{
					inptr = blk_ptr + SIZEOF(blk_hdr);
					if (in_use_new_key)
					{
						GTMCRYPT_DECRYPT(NULL, TRUE, in_encr_handles.encr_key_handle2,
								inptr, in_len, NULL, blk_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
					} else
					{
						GTMCRYPT_DECRYPT(NULL, inhead.non_null_iv, in_encr_handles.encr_key_handle,
								inptr, in_len, NULL, blk_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
					}
					if (0 != gtmcrypt_errno)
					{
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, ptr->input_file.len,
										ptr->input_file.addr);
						CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
					}
				}
				if (old_to_be_encrypted || old_is_encrypted)
				{
					inptr = blk_ptr + SIZEOF(blk_hdr);
					if (old_to_be_encrypted)
					{
						GTMCRYPT_ENCRYPT(NULL, TRUE, old_encr_handles.encr_key_handle2,
							inptr, in_len, NULL, blk_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
					} else
					{
						GTMCRYPT_ENCRYPT(NULL, old_data.non_null_iv, old_encr_handles.encr_key_handle,
							inptr, in_len, NULL, blk_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
					}
					if (0 != gtmcrypt_errno)
					{
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, n_len, db_name);
						CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
					}
				}
			}
			offset = (off_t)BLK_ZERO_OFF(old_start_vbn) + (off_t)blk_num * old_blk_size;
			if (udi->fd_opened_with_o_direct)
			{	/* Align buffers for O_DIRECT */
				assert(size <= old_blk_size);
				DIO_BUFF_EXPAND_IF_NEEDED(udi, old_blk_size, &(TREF(dio_buff)));
				memcpy((TREF(dio_buff)).aligned, blk_ptr, size);
				memset((TREF(dio_buff)).aligned + size, 0, old_blk_size - size);
				blk_ptr = (char *)(TREF(dio_buff)).aligned;
				size = old_blk_size;
			}
			DB_LSEEKWRITE(NULL, udi, NULL, db_fd, offset, blk_ptr, size, save_errno);
			if (0 != save_errno)
			{
				util_out_print("Error accessing output file !AD. Aborting restore.", TRUE, n_len, db_name);
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				CLNUP_AND_EXIT(save_errno, inbuf);
			}
		}
		/* Next section is the file header which we need to restore */
		COMMON_READ(in, &rsize, SIZEOF(rsize), inbuf);
		assert((SGMNT_HDR_LEN + SIZEOF(int4)) == rsize);
		COMMON_READ(in, inbuf, rsize, inbuf);
		((sgmnt_data_ptr_t)inbuf)->start_vbn = old_start_vbn;
		((sgmnt_data_ptr_t)inbuf)->free_space = (uint4)(BLK_ZERO_OFF(old_start_vbn) - SIZEOF_FILE_HDR(inbuf));
		GTMCRYPT_COPY_ENCRYPT_SETTINGS(&old_data, ((sgmnt_data_ptr_t)inbuf));
		assert((udi->semid == old_data.semid) && (udi->gt_sem_ctime == old_data.gt_sem_ctime.ctime)
			&& (udi->shmid == old_data.shmid) && (udi->gt_shm_ctime == old_data.gt_shm_ctime.ctime));
		/* Since the file header we are about to write is taken from the BACKUP database, the semid/shmid (and the
		 * corresponding ctime fields) will be INVALID. But, we want the restor'ed file to continue having standalone
		 * access until we are done with MUPIP RESTORE (and release the semaphores and clear the file header fields in
		 * db_ipcs_reset). So, before writing the new file header, set the semid/shmid (and ctime fields) to the semid and
		 * shmid fields that is currently valid in the system (when we did the mu_rndwn_file).
		 */
		csd = (sgmnt_data_ptr_t)inbuf;
		csd->semid = old_data.semid;
		csd->gt_sem_ctime.ctime = old_data.gt_sem_ctime.ctime;
		csd->shmid = old_data.shmid;
		csd->gt_shm_ctime.ctime = old_data.gt_shm_ctime.ctime;
		if (udi->fd_opened_with_o_direct)
		{	/* Align buffers for O_DIRECT */
			DIO_BUFF_EXPAND_IF_NEEDED(udi, SGMNT_HDR_LEN, &(TREF(dio_buff)));
			memcpy((TREF(dio_buff)).aligned, csd, SGMNT_HDR_LEN);
			csd = (sgmnt_data_ptr_t)(TREF(dio_buff)).aligned;
		}
		DB_LSEEKWRITE(NULL, udi, NULL, db_fd, 0, csd, SGMNT_HDR_LEN, save_errno);
		if (0 != save_errno)
		{
			util_out_print("Error accessing output file !AD. Aborting restore.", TRUE, n_len, db_name);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			CLNUP_AND_EXIT(save_errno, inbuf);
		}
		GET_LONG(temp, (inbuf + rsize - SIZEOF(int4)));
		rsize = temp;
		COMMON_READ(in, inbuf, rsize, inbuf);
		if (0 != MEMCMP_LIT(inbuf, HDR_MSG))
		{
			util_out_print("Unexpected backup format error restoring !AD. Aborting restore.", TRUE, n_len, db_name);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			CLNUP_AND_EXIT(save_errno, inbuf);
		}
		GET_LONG(temp, (inbuf + rsize - SIZEOF(int4)));
		rsize = temp;
		offset = (MM_BLOCK - 1) * DISK_BLOCK_SIZE;
		assert(SGMNT_HDR_LEN == offset);	/* Still have contiguou master map for now */
		for (i = 0; ; i++)			/* Restore master map */
		{
			COMMON_READ(in, inbuf, rsize, inbuf);
			if (!MEMCMP_LIT(inbuf, MAP_MSG))
				break;
			size = rsize - SIZEOF(int4);
			if (udi->fd_opened_with_o_direct)
			{	/* Align buffers for O_DIRECT */
				orig_size = size;
				size = ROUND_UP2(orig_size, DIO_ALIGNSIZE(udi));
				DIO_BUFF_EXPAND_IF_NEEDED(udi, size, &(TREF(dio_buff)));
				memcpy((TREF(dio_buff)).aligned, inbuf, orig_size);
				p = (char *)(TREF(dio_buff)).aligned;
			} else
				p = (char *)inbuf;
			DB_LSEEKWRITE(NULL, udi, NULL, db_fd, offset, p, size, save_errno);
			if (0 != save_errno)
			{
				util_out_print("Error accessing output file !AD. Aborting restore.",
					TRUE, n_len, db_name);
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				CLNUP_AND_EXIT(save_errno, inbuf);
			}
			offset += size;
			GET_LONG(temp, inbuf + size);
			rsize = temp;
		}
		curr_tn = inhead.end_tn;
		switch (type)
		{
			case backup_to_file:
				iob_close(in);
				break;
			case backup_to_exec:
				CLOSEFILE_RESET(in->fd, rc);	/* resets "in->fd" to FD_INVALID */
				if ((pipe_child > 0) && (FALSE != is_proc_alive(pipe_child, 0)))
					WAITPID(pipe_child, (int *)&status, 0, waitpid_res);
				break;
			case backup_to_tcp:
				break;
		}
	}
	util_out_print("!/RESTORE COMPLETED", TRUE);
	util_out_print("!UL blocks restored", TRUE, rest_blks);
	CLNUP_AND_EXIT(SS_NORMAL, inbuf);
}

STATICFNDEF void exec_read(BFILE *bf, char *buf, int nbytes)
{
	int	needed, got;
	int4	status;
	char	*curr;
	pid_t	waitpid_res;
	int	rc;

	assert(nbytes > 0);
	needed = nbytes;
	curr = buf;
#	ifdef DEBUG_ONLINE
	DBGFPF(stdout, "file descriptor is %d and bytes needed is %d\n", bf->fd, needed);
#	endif
	while (0 != (got = (int)(read(bf->fd, curr, needed))))
	{
		if (got == needed)
			break;
		else if (got > 0)
		{
			needed -= got;
			curr += got;
		}
		/* the check for EINTR below is valid and should not be converted to an EINTR
		 * wrapper macro, for an immediate retry is not attempted. Instead, wcs_sleep
		 * is called.
		 */
		else if ((EINTR != errno) && (EAGAIN != errno))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
			if ((pipe_child > 0) && (FALSE != is_proc_alive(pipe_child, 0)))
				WAITPID(pipe_child, (int *)&status, 0, waitpid_res);
			CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
			restore_read_errno = errno;
			break;
		}
		wcs_sleep(100);
	}
	return;
}

/* the logic here can be reused in iosocket_readfl.c */
STATICFNDEF void tcp_read(BFILE *bf, char *buf, int nbytes)
{
	int     	needed, status;
	char		*curr;
	fd_set          fs;
	struct timeval	save_nap, nap;
	int		rc;

	needed = nbytes;
	curr = buf;
	nap.tv_sec = 1;
	nap.tv_usec = 0;
	while (1)
	{
		assertpro(FD_SETSIZE > bf->fd);
		FD_ZERO(&fs);
		FD_SET(bf->fd, &fs);
		assert(0 != FD_ISSET(bf->fd, &fs));
		/* Note: the check for EINTR from the select below should remain, as aa_select is a
		 * function, and not all callers of aa_select behave the same when EINTR is returned.
		 */
		save_nap = nap;
		status = select(bf->fd + 1, (void *)(&fs), (void *)0, (void *)0, &nap);
		nap = save_nap;
		if (status > 0)
		{
			RECV(bf->fd, curr, needed, 0, status);
			if ((0 == status) || (needed == status))        /* lost connection or all set */
			{
				break;
			} else if (status > 0)
			{
				needed -= status;
				curr += status;
			}
		}
		if ((status < 0) && (errno != EINTR))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
			CLOSEFILE_RESET(bf->fd, rc);	/* resets "bf->fd" to FD_INVALID */
			restore_read_errno = errno;
			break;
		}
	}
	return;
}
