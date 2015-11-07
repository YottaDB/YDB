/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "iotcpdef.h"
#include "iotcproutine.h"
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

GBLDEF	inc_list_struct		in_files;
GBLREF	uint4			pipe_child;
GBLREF	tcp_library_struct	tcp_routines;
GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			restore_read_errno;

LITREF	char			*gtm_dbversion_table[];

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

CONDITION_HANDLER(iob_io_error)
{
	char 	s[80];
	char 	*fgets_res;

	START_CH;
	if (SIGNAL == ERR_IOEOF)
	{
		PRINTF("End of media reached, please mount next volume and press Enter: ");
		FGETS(s, 79, stdin, fgets_res);
		util_out_print(0, 2, 0);  /* clear error message */
		CONTINUE;
	}
	PRN_ERROR;
	UNWIND(NULL, NULL);
}


void mupip_restore(void)
{
	static readonly char	label[] =   GDS_LABEL;
	char			db_name[MAX_FN_LEN + 1], *inbuf = NULL, *p, *blk_ptr;
	inc_list_struct 	*ptr;
	inc_header		inhead;
	sgmnt_data		old_data;
	unsigned short		n_len;
	int4			status, rsize, temp, save_errno, old_start_vbn;
	uint4			rest_blks, totblks;
	trans_num		curr_tn;
	uint4			ii;
	block_id		blk_num;
	boolean_t		extend, truncate;
	uint4			cli_status;
	BFILE			*in;
	int			i, db_fd;
	uint4			old_blk_size, size, old_tot_blks, bplmap, old_bit_maps, new_bit_maps;
	off_t			new_eof, offset;
#	ifdef GTM_TRUNCATE
	off_t			new_size;
#	endif
	char			buff[DISK_BLOCK_SIZE];
	char			msg_buffer[1024], *newmap, *newmap_bptr;
	mstr			msg_string;
	char			addr[SA_MAXLEN+1];
	unsigned char		tcp[5];
	backup_type		type;
	unsigned short		port;
	int4			timeout, cut, match;
	void			(*common_read)();
	char			*errptr;
	pid_t			waitpid_res;
	muinc_blk_hdr_ptr_t	sblkh_p;
	int			rc;
#	ifdef GTM_CRYPT
	char			bkup_hash[GTMCRYPT_HASH_LEN], *inptr;
	int			in_len, gtmcrypt_errno;
	gtmcrypt_key_t		bkup_key_handle, target_key_handle;
	boolean_t		is_bkup_file_encrypted, is_same_hash = FALSE;
#	endif
	ZOS_ONLY(int 		realfiletag;)
	DEBUG_ONLY(unix_db_info	*udi;)

	extend = TRUE;
	if (CLI_NEGATED == (cli_status = cli_present("EXTEND")))
		extend = FALSE;
	truncate = FALSE;
	GTM_TRUNCATE_ONLY(truncate = TRUE;)
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
	DEBUG_ONLY(udi = FILE_INFO(gv_cur_region));
	OPENFILE(db_name, O_RDWR, db_fd);
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
	LSEEKREAD(db_fd, 0, &old_data, SIZEOF(sgmnt_data), save_errno);
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
	for (ptr = in_files.next;  ptr;  ptr = ptr->next)
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
		switch(type)
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
				ESTABLISH(iob_io_error);
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
				iotcp_fillroutine();
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
		if (memcmp(inhead.label, V5_INC_HEADER_LABEL, INC_HDR_LABEL_SZ) &&
		    (memcmp(inhead.label, INC_HEADER_LABEL, INC_HDR_LABEL_SZ)))
		{
			util_out_print("Input file !AD has an unrecognizable format", TRUE, ptr->input_file.len,
				ptr->input_file.addr);
			CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
		}
#		ifdef GTM_CRYPT
		is_bkup_file_encrypted = FALSE;
		if (inhead.is_encrypted)
		{
			COMMON_READ(in, bkup_hash, GTMCRYPT_HASH_LEN, inbuf);
			is_bkup_file_encrypted = inhead.is_encrypted;
		}
#		endif
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
			if (old_tot_blks > inhead.db_total_blks && truncate)
			{
				/* Truncate occurred between incremental backups. FTRUNCATE the db file to the appropriate size,
				 * write the EOF block. */
				new_eof = ((off_t)(old_start_vbn - 1) * DISK_BLOCK_SIZE)
						+ ((off_t)inhead.db_total_blks * old_blk_size);
				new_size = new_eof + DISK_BLOCK_SIZE;
				memset(buff, 0, DISK_BLOCK_SIZE);
				DB_LSEEKWRITE(NULL, NULL, db_fd, new_eof, buff, DISK_BLOCK_SIZE, status);
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
			{	/* The db must be exteneded which we will do ourselves (to avoid jnl and other interferences
				   in gdsfilext). These local bit map blocks will be created in GDSVCURR format (always). The
				   reason for this is that we do not know at this time whether these blocks will be replaced
				   by blocks in the backup or not. If we are in compatibility mode, this is highly likely
				   even if before image journaling is on which creates bit maps with TN=0. In either case,
				   a GDSVCURR format block is the only one that can be added to the database without affecting
				   the blks_to_upgrd counter.
				 */
				new_eof = ((off_t)(old_start_vbn - 1) * DISK_BLOCK_SIZE)
						+ ((off_t)inhead.db_total_blks * old_blk_size);
				memset(buff, 0, DISK_BLOCK_SIZE);
				DB_LSEEKWRITE(NULL, NULL, db_fd, new_eof, buff, DISK_BLOCK_SIZE, status);
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
				{	/* -- similar logic exist in bml_newmap.c, which need to pick up any new updates here -- */
					newmap = (char *)malloc(old_blk_size);
					((blk_hdr *)newmap)->bver = GDSVCURR;
					((blk_hdr *)newmap)->bsiz = (unsigned int)(BM_SIZE(bplmap));
					((blk_hdr *)newmap)->levl = LCL_MAP_LEVL;
					((blk_hdr *)newmap)->tn = curr_tn;
					newmap_bptr = newmap + SIZEOF(blk_hdr);
					*newmap_bptr++ = THREE_BLKS_FREE;
					memset(newmap_bptr, FOUR_BLKS_FREE, BM_SIZE(bplmap) - SIZEOF(blk_hdr) - 1);
					for (ii = ROUND_UP(old_tot_blks, bplmap); ii < inhead.db_total_blks; ii += bplmap)
					{
						new_eof = (off_t)(old_start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)ii * old_blk_size;
						DB_LSEEKWRITE(NULL, NULL, db_fd, new_eof, newmap, old_blk_size, status);
						if (0 != status)
						{
							util_out_print("Aborting restore!/", TRUE);
							util_out_print("Bitmap 0x!XL initialization error!", TRUE, ii);
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
							free(newmap);
							CLNUP_AND_EXIT(ERR_MUPRESTERR, inbuf);
						}
					}
					free(newmap);
				}
				old_tot_blks = inhead.db_total_blks;
				old_bit_maps = new_bit_maps;
			}
		}
		rsize = SIZEOF(muinc_blk_hdr) + inhead.blk_size;
#		ifdef GTM_CRYPT
		if (is_bkup_file_encrypted || old_data.is_encrypted)
		{
			/* See if the backup file and the target file have the same hash thereby speeding up the most common case */
			if (!memcmp(bkup_hash, old_data.encryption_hash, GTMCRYPT_HASH_LEN))
				is_same_hash = TRUE;
			ASSERT_ENCRYPTION_INITIALIZED;	/* should have been done in mu_rndwn_file (called from STANDALONE) */
			if (!is_same_hash)
			{
				if (is_bkup_file_encrypted)
				{
					GTMCRYPT_GETKEY(NULL, bkup_hash, bkup_key_handle, gtmcrypt_errno);
					if (0 != gtmcrypt_errno)
					{
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, ptr->input_file.len,
										ptr->input_file.addr);
						CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
					}
				}
				if (old_data.is_encrypted)
				{
					GTMCRYPT_GETKEY(NULL, old_data.encryption_hash, target_key_handle, gtmcrypt_errno);
					if (0 != gtmcrypt_errno)
					{
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, n_len, db_name);
						CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
					}
				}
			}
		}
#		endif
		for ( ; ;)
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
			GTM_TRUNCATE_ONLY(
				if (blk_num >= old_tot_blks) /* Should be a bitmap block */
					continue;
			)
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
#			ifdef GTM_CRYPT
			assert((size <= old_blk_size) && (size >= SIZEOF(blk_hdr)));
			in_len = MIN(old_blk_size, size) - SIZEOF(blk_hdr);
			if (!is_same_hash
				&& (BLOCK_REQUIRE_ENCRYPTION(is_bkup_file_encrypted, (((blk_hdr_ptr_t)blk_ptr)->levl), in_len)))
			{
				inptr = blk_ptr + SIZEOF(blk_hdr);
				GTMCRYPT_DECRYPT(NULL, bkup_key_handle, inptr, in_len, NULL, gtmcrypt_errno);
				if (0 != gtmcrypt_errno)
				{
					GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, ptr->input_file.len,
									ptr->input_file.addr);
					CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
				}
			}
#			endif
			offset = (old_start_vbn - 1) * DISK_BLOCK_SIZE + ((off_t)old_blk_size * blk_num);
#			ifdef GTM_CRYPT
			if (!is_same_hash
				&& (BLOCK_REQUIRE_ENCRYPTION(old_data.is_encrypted, (((blk_hdr_ptr_t)blk_ptr)->levl), in_len)))
			{
				inptr = blk_ptr + SIZEOF(blk_hdr);
				GTMCRYPT_ENCRYPT(NULL, target_key_handle, inptr, in_len, NULL, gtmcrypt_errno);
				if (0 != gtmcrypt_errno)
				{
					GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, n_len, db_name);
					CLNUP_AND_EXIT(gtmcrypt_errno, inbuf);
				}
			}
#			endif
			DB_LSEEKWRITE(NULL, NULL, db_fd, offset, blk_ptr, size, save_errno);
			if (0 != save_errno)
			{
				util_out_print("Error accessing output file !AD. Aborting restore.",
					TRUE, n_len, db_name);
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				CLNUP_AND_EXIT(save_errno, inbuf);
			}
		}
		/* Next section is the file header which we need to restore */
		COMMON_READ(in, &rsize, SIZEOF(rsize), inbuf);
		assert((SIZEOF(sgmnt_data) + SIZEOF(int4)) == rsize);
		COMMON_READ(in, inbuf, rsize, inbuf);
		((sgmnt_data_ptr_t)inbuf)->start_vbn = old_start_vbn;
		((sgmnt_data_ptr_t)inbuf)->free_space = (uint4)(((old_start_vbn - 1) * DISK_BLOCK_SIZE) - SIZEOF_FILE_HDR(inbuf));
		GTMCRYPT_ONLY(
			memcpy(((sgmnt_data_ptr_t)inbuf)->encryption_hash, old_data.encryption_hash, GTMCRYPT_HASH_LEN);
			((sgmnt_data_ptr_t)inbuf)->is_encrypted = old_data.is_encrypted;
		)
		assert((udi->semid == old_data.semid) && (udi->gt_sem_ctime == old_data.gt_sem_ctime.ctime)
			&& (udi->shmid == old_data.shmid) && (udi->gt_shm_ctime == old_data.gt_shm_ctime.ctime));
		/* Since the file header we are about to write is taken from the BACKUP database, the semid/shmid (and the
		 * corresponding ctime fields) will be INVALID. But, we want the restor'ed file to continue having standalone
		 * access until we are done with MUPIP RESTORE (and release the semaphores and clear the file header fields in
		 * db_ipcs_reset). So, before writing the new file header, set the semid/shmid (and ctime fields) to the semid and
		 * shmid fields that is currently valid in the system (when we did the mu_rndwn_file).
		 */
		((sgmnt_data_ptr_t)inbuf)->semid = old_data.semid;
		((sgmnt_data_ptr_t)inbuf)->gt_sem_ctime.ctime = old_data.gt_sem_ctime.ctime;
		((sgmnt_data_ptr_t)inbuf)->shmid = old_data.shmid;
		((sgmnt_data_ptr_t)inbuf)->gt_shm_ctime.ctime = old_data.gt_shm_ctime.ctime;
		DB_LSEEKWRITE(NULL, NULL, db_fd, 0, inbuf, rsize - SIZEOF(int4), save_errno);
		if (0 != save_errno)
		{
			util_out_print("Error accessing output file !AD. Aborting restore.",
				       TRUE, n_len, db_name);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			CLNUP_AND_EXIT(save_errno, inbuf);
		}
		GET_LONG(temp, (inbuf + rsize - SIZEOF(int4)));
		rsize = temp;
		COMMON_READ(in, inbuf, rsize, inbuf);
		if (0 != MEMCMP_LIT(inbuf, HDR_MSG))
		{
			util_out_print("Unexpected backup format error restoring !AD. Aborting restore.",
				       TRUE, n_len, db_name);
			errptr = (char *)STRERROR(save_errno);
			util_out_print("write : !AZ", TRUE, errptr);
			CLNUP_AND_EXIT(save_errno, inbuf);
		}

		GET_LONG(temp, (inbuf + rsize - SIZEOF(int4)));
		rsize = temp;
		offset = (MM_BLOCK - 1) * DISK_BLOCK_SIZE;
		assert(SGMNT_HDR_LEN == offset);	/* Still have contiguou master map for now */
		for (i = 0;  ;  i++)			/* Restore master map */
		{
			COMMON_READ(in, inbuf, rsize, inbuf);
			if (!MEMCMP_LIT(inbuf, MAP_MSG))
				break;
			DB_LSEEKWRITE(NULL, NULL, db_fd,
				   offset,
				   inbuf,
				   rsize - SIZEOF(int4),
				   save_errno);
			if (0 != save_errno)
			{
				util_out_print("Error accessing output file !AD. Aborting restore.",
					TRUE, n_len, db_name);
				errptr = (char *)STRERROR(save_errno);
				util_out_print("write : !AZ", TRUE, errptr);
				CLNUP_AND_EXIT(save_errno, inbuf);
			}
			offset += rsize - SIZEOF(int4);
			GET_LONG(temp, (inbuf + rsize - SIZEOF(int4)));
			rsize = temp;
		}
		curr_tn = inhead.end_tn;
		switch (type)
		{
			case backup_to_file:
				REVERT;
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
	while(0 != (got = (int)(read(bf->fd, curr, needed))))
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

/* the logic here can be reused in iotcp_readfl.c and iosocket_readfl.c */
STATICFNDEF void tcp_read(BFILE *bf, char *buf, int nbytes)
{
	int     	needed, status;
	char		*curr;
	fd_set          fs;
	ABS_TIME	save_nap, nap;
	int		rc;

	needed = nbytes;
	curr = buf;
	nap.at_sec = 1;
	nap.at_usec = 0;
	while (1)
	{
		FD_ZERO(&fs);
		FD_SET(bf->fd, &fs);
		assert(0 != FD_ISSET(bf->fd, &fs));
		/* Note: the check for EINTR from the select below should remain, as aa_select is a
		 * function, and not all callers of aa_select behave the same when EINTR is returned.
		 */
		save_nap = nap;
		status = tcp_routines.aa_select(bf->fd + 1, (void *)(&fs), (void *)0, (void *)0, &nap);
		nap = save_nap;
		if (status > 0)
		{
			status = tcp_routines.aa_recv(bf->fd, curr, needed, 0);
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
