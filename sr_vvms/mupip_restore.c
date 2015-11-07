/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <descrip.h>
#include <iodef.h>
#include <lckdef.h>
#include <psldef.h>
#include <rms.h>
#include <ssdef.h>
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "efn.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "mupipbckup.h"
#include "murest.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "bit_set.h"
#include "is_proc_alive.h"
#include "locks.h"
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#ifdef BACKUP_TO_EXEC
#include "gtm_pipe.h"
#include "mupip_restore.h"
#endif
#include "mu_outofband_setup.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "shmpool.h"
#include "gds_blk_downgrade.h"
#include "min_max.h"

#define TCP_LENGTH		5
#define COMMON_READ(A, B, C)	{						\
					(*common_read)(A, B, C);		\
					if (0 != restore_read_errno)		\
					{					\
						free(inbuf);			\
						free(old_data);			\
						mupip_exit(restore_read_errno);	\
					}					\
				}

GBLDEF	inc_list_struct 	in_files;
GBLREF 	gd_region		*gv_cur_region;
GBLREF	uint4			restore_read_errno;
GBLREF  tcp_library_struct	tcp_routines;
GBLREF  bool			mubtomag;
GBLREF 	int4 			mubmaxblk;

LITREF	char			*gtm_dbversion_table[];

error_def(ERR_MUNODBNAME);
error_def(ERR_MUPRESTERR);
error_def(ERR_MUSTANDALONE);

static void tcp_read(char *temp, char *buf, int nbytes);
static void record_read(char *temp, char *buf, int nbytes);
void exec_read(char *temp, char *buf, int nbytes);

void mupip_restore(void)
{
	static readonly char	label[] = GDS_LABEL;
	inc_list_struct 	*ptr;
	inc_header		*inhead;
	sgmnt_data		*old_data;
	trans_num		curr_tn;
	block_id		blk_num;
	struct FAB		extfab, infab;
	struct XABFHC		muxab;
	struct RAB		inrab, extrab;
	file_control		*fc;
	int			i, size, next_pos, backup_socket, size1;
	uint4			cli_status, cur_tot, rest_blks, status, totblks, bplmap, ii;
	char			buff[DISK_BLOCK_SIZE], *inbuf, *common, *p, tcp[TCP_LENGTH], addr[SA_MAXLEN+1], *blk_ptr;
	char			*newmap, *newmap_bptr;
	char_ptr_t		ptr1, ptr1_top;
	backup_type		type;
	int4			timeout, cut, match, temp_int4;
	unsigned short		port;
	boolean_t		extend;
	void			(*common_read)(char *, char *, int);
	muinc_blk_hdr_ptr_t	sblkh_p;

	if (!mubtomag)
		mubmaxblk = BACKUP_TEMPFILE_BUFF_SIZE;
	extend = TRUE;
	if (CLI_NEGATED == (cli_status = cli_present("EXTEND")))
		extend = FALSE;
	mu_outofband_setup();
	mu_gv_cur_reg_init();
	gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
	if (0 == cli_get_str("DATABASE", gv_cur_region->dyn.addr->fname, &gv_cur_region->dyn.addr->fname_len))
		mupip_exit(ERR_MUNODBNAME);
	if (!mu_rndwn_file(TRUE))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
		mupip_exit(ERR_MUPRESTERR);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->file_type = dba_bg;
	fc->op = FC_OPEN;
	status = dbfilop(fc);
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("Error accessing output file !AD. Aborting restore.", TRUE, DB_LEN_STR(gv_cur_region));
		mupip_exit(status);
	}
	murgetlst();
	old_data = malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	fc->op = FC_READ;
	fc->op_buff = old_data;
	fc->op_len = ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
	fc->op_pos = 1;
	dbfilop(fc);
	if (memcmp(&old_data->label[0], &label[0], SIZEOF(old_data->label)))
	{
		util_out_print("Output file !AD has an unrecognizable format", TRUE, DB_LEN_STR(gv_cur_region));
		free(old_data);
		mupip_exit(ERR_MUPRESTERR);
	}
	inbuf = malloc(MAX(mubmaxblk, MAX((old_data->blk_size + SIZEOF(muinc_blk_hdr)),
					  MAX(SGMNT_HDR_LEN, MASTER_MAP_SIZE_MAX))));
	cur_tot = old_data->trans_hist.total_blks;
	curr_tn = old_data->trans_hist.curr_tn;
	bplmap = old_data->bplmap;
	inhead = malloc(SIZEOF(inc_header));
	rest_blks = 0;
	for (ptr = in_files.next; ptr; ptr = ptr->next)
	{
		/* --- determine source type --- */
		type = backup_to_file;
		if (0 == ptr->input_file.len)
			continue;
		else if ('|' == *(ptr->input_file.addr + ptr->input_file.len - 1))
		{
			type = backup_to_exec;
			ptr->input_file.len--;
			*(ptr->input_file.addr + ptr->input_file.len) = '\0';
		} else if (ptr->input_file.len > TCP_LENGTH)
		{
			lower_to_upper(tcp, ptr->input_file.addr, TCP_LENGTH);
			if (0 == memcmp(tcp, "TCP:/", TCP_LENGTH))
			{
				type = backup_to_tcp;
				cut = TCP_LENGTH;
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
				infab = cc$rms_fab;
				infab.fab$b_fac = FAB$M_GET;
				infab.fab$l_fna = ptr->input_file.addr;
				infab.fab$b_fns = ptr->input_file.len;
				inrab = cc$rms_rab;
				inrab.rab$l_fab = &infab;
				if ((RMS$_NORMAL != (status = sys$open(&infab))) ||
					(RMS$_NORMAL != (status = sys$connect(&inrab))))
				{
					gtm_putmsg(VARLSTCNT(1) status);
					util_out_print("Error accessing input file !AD. Aborting restore.", TRUE,
						infab.fab$b_fns, infab.fab$l_fna);
					free(inbuf);
					free(old_data);
					free(inhead);
					mupip_exit(status);
				}
				common_read = record_read;
				common = (char *)(&inrab);
				break;
			case backup_to_exec:
#				ifdef BACKUP_TO_EXEC
				pipe_child = 0;
				common_read = exec_read;
				in = (BFILE *)malloc(SIZEOF(BFILE));
				if (0 > (in->fd = gtm_pipe(ptr->input_file.addr, input_from_comm)))
				{
					util_out_print("Error creating input pipe from !AD.", TRUE, ptr->input_file.len, ptr->input_
						file.addr);
					mupip_exit(ERR_MUPRESTERR);
				}
				DBGFPF(stdout, "file descriptor for the openned pipe is %d.\n", in->fd);
				DBGFPF(stdout, "the command passed to gtm_pipe is %s.\n", ptr->input_file.addr);
				break;
#				endif
			case backup_to_tcp:
				/* parse the input */
				switch (match = SSCANF(ptr->input_file.addr, "%[^:]:%hu", addr, &port))
				{
					case 1:
						port = DEFAULT_BKRS_PORT;
					case 2:
						break;
					default:
						util_out_print("Error : A hostname has to be specified.", TRUE);
						free(inbuf);
						free(old_data);
						free(inhead);
						mupip_exit(ERR_MUPRESTERR);
				}
				if ((0 == cli_get_int("NETTIMEOUT", &timeout)) || (0 > timeout))
					timeout = DEFAULT_BKRS_TIMEOUT;
				iotcp_fillroutine();
				if (0 > (backup_socket = tcp_open(addr, port, timeout, TRUE)))
				{
					util_out_print("Error establishing TCP connection to !AD.", TRUE,
						ptr->input_file.len, ptr->input_file.addr);
					free(inbuf);
					free(old_data);
					free(inhead);
					mupip_exit(ERR_MUPRESTERR);
				}
				common_read = tcp_read;
				common = (char *)(&backup_socket);
				break;
			default:
				util_out_print("Aborting restore!/", TRUE);
				util_out_print("Unrecognized input format !AD", TRUE, ptr->input_file.len, ptr->input_file.addr);
				free(inbuf);
				free(old_data);
				free(inhead);
				mupip_exit(ERR_MUPRESTERR);
		}
		size = SIZEOF(inc_header);
		COMMON_READ(common, (char *)(inhead), size);
		/* validate incremental backup header */
		if (0 != memcmp(&inhead->label[0], INC_HEADER_LABEL, INC_HDR_LABEL_SZ))
		{
			util_out_print("Input !AD has an unrecognizable format", TRUE, ptr->input_file.len, ptr->input_file.addr);
			free(inbuf);
			free(old_data);
			free(inhead);
			mupip_exit(ERR_MUPRESTERR);
		}
		if (curr_tn != inhead->start_tn)
		{
			util_out_print("Transaction in input !AD does not align with database TN.!/DB: 0x!16@XQ!_Input : 0x!16@XQ",
				TRUE, ptr->input_file.len, ptr->input_file.addr, &curr_tn, &inhead->start_tn);
			free(inbuf);
			free(old_data);
			free(inhead);
			mupip_exit(ERR_MUPRESTERR);
		}
		if (old_data->blk_size != inhead->blk_size)
		{
			util_out_print("Incompatible block size.  Output file !AD has block size !XL,", TRUE,
					DB_LEN_STR(gv_cur_region), old_data->blk_size);
			util_out_print("while input !AD is from a database with block size !XL,", TRUE,
					ptr->input_file.len, ptr->input_file.addr, inhead->blk_size);
			free(inbuf);
			free(old_data);
			free(inhead);
			mupip_exit(ERR_MUPRESTERR);
		}
		assert(0 < cur_tot);
		if (cur_tot != inhead->db_total_blks)
		{
			if (cur_tot > inhead->db_total_blks || !extend)
			{
				totblks = cur_tot - DIVIDE_ROUND_UP(cur_tot, DISK_BLOCK_SIZE);
				util_out_print("Incompatible database sizes.  Output file !AD has!/  !UL (!XL hex) total blocks,",
					TRUE, DB_LEN_STR(gv_cur_region), totblks, totblks);
				totblks = inhead->db_total_blks - DIVIDE_ROUND_UP(inhead->db_total_blks, DISK_BLOCK_SIZE);
				util_out_print("while input !AD is from a database with!/  !UL (!XL hex) total blocks", TRUE,
							ptr->input_file.len, ptr->input_file.addr, totblks, totblks);
				free(inbuf);
				free(old_data);
				free(inhead);
				mupip_exit(ERR_MUPRESTERR);
			} else
			{	/* Although we are extending the file, we need not write any local bit maps like would occur
				 * with a regular extension of the db. This is because the backup process makes sure that any
				 * necessary bitmaps are part of the backup and will thus be properly restored.
				 */
				muxab = cc$rms_xabfhc;
				extrab = cc$rms_rab;
				extrab.rab$l_fab = &extfab;
				extfab = cc$rms_fab;
				extfab.fab$l_xab = &muxab;
				extfab.fab$l_fna = gv_cur_region->dyn.addr->fname;
				extfab.fab$b_fns = gv_cur_region->dyn.addr->fname_len;
				extfab.fab$b_fac = FAB$M_BIO | FAB$M_PUT;
				extfab.fab$l_fop = FAB$M_CBT;
				extfab.fab$b_shr = FAB$M_SHRPUT | FAB$M_UPI;
				if ((RMS$_NORMAL != (status = sys$open(&extfab))) ||
					(RMS$_NORMAL != (status = sys$connect(&extrab))))
				{
					util_out_print("Cannot extend output file.",TRUE);
					free(inbuf);
					free(old_data);
					free(inhead);
					mupip_exit(ERR_MUPRESTERR);
				}
				memset(buff, 0, DISK_BLOCK_SIZE);
				extrab.rab$l_rbf = buff;
				extrab.rab$w_rsz = DISK_BLOCK_SIZE;
				extrab.rab$l_bkt =
					old_data->start_vbn - 1 + (inhead->db_total_blks * (old_data->blk_size / DISK_BLOCK_SIZE));
				if (RMS$_NORMAL != (status = sys$write(&extrab)))
				{
					util_out_print("Cannot write to output file.",TRUE);
					free(inbuf);
					free(old_data);
					free(inhead);
					mupip_exit(ERR_MUPRESTERR);
				}
				sys$close(&extfab);
				/* --- initialize all new bitmaps, just in case they are not touched later --- */
				if (DIVIDE_ROUND_DOWN(inhead->db_total_blks, bplmap) > DIVIDE_ROUND_DOWN(cur_tot, bplmap))
				{	/* -- similar logic exist in bml_newmap.c, which need to pick up any new updates here -- */
					newmap = (char *)malloc(old_data->blk_size);
					((blk_hdr *)newmap)->bver = GDSVCURR;
					((blk_hdr *)newmap)->bsiz = BM_SIZE(bplmap);
					((blk_hdr *)newmap)->levl = LCL_MAP_LEVL;
					((blk_hdr *)newmap)->tn = curr_tn;
					newmap_bptr = newmap + SIZEOF(blk_hdr);
					*newmap_bptr++ = THREE_BLKS_FREE;
					memset(newmap_bptr, FOUR_BLKS_FREE, BM_SIZE(bplmap) - SIZEOF(blk_hdr) - 1);
					fc->op = FC_WRITE;
					fc->op_buff = newmap;
					for (ii = ROUND_UP(cur_tot, bplmap); ii < inhead->db_total_blks; ii += bplmap)
					{
						fc->op_pos = old_data->start_vbn
							+ ((gtm_int64_t)old_data->blk_size / DISK_BLOCK_SIZE * ii);
						dbfilop(fc);
					}
					free(newmap);
				}
				cur_tot = inhead->db_total_blks;
			}
		}
		fc->op = FC_WRITE;
		fc->op_buff = inbuf + SIZEOF(muinc_blk_hdr);
		sblkh_p = (muinc_blk_hdr_ptr_t)inbuf;
		size = SIZEOF(muinc_blk_hdr) + old_data->blk_size;
		for ( ; ;)
		{
			COMMON_READ(common, inbuf, size);
			if (0 == MEMCMP_LIT(inbuf, END_MSG))
				break;
			blk_num = ((muinc_blk_hdr_ptr_t)inbuf)->blkid;
			fc->op_pos = old_data->start_vbn + ((gtm_int64_t)old_data->blk_size / DISK_BLOCK_SIZE * blk_num);
			/* For blocks that were read during the main backup phase of stream backup, the blocks are
			 * recorded without version (there may even be some garbage blocks in the stream of
			 * indeterminate/invalid format if a bitmap was written out prior to the data blocks that
			 * were recently allocated in it). For these blocks, we just write out what we have as a
			 * full block. For blocks that were written out during the backup as part of the online
			 * image processing, these are always recorded in V5 mode. We will rewrite these in the mode
			 * they were oringally found on disk (potentially necessitating a downgrade of the block).
			 * This allows us to exactly match the blks_to_upgrade counter in the saved file-header without
			 * worrying about what blocks were converted (or not) in the interim.
			 */
			blk_ptr = inbuf + SIZEOF(muinc_blk_hdr);
			if (GDSNOVER != sblkh_p->use.bkup.ondsk_blkver)
			{	/* Specifically versioned blocks - Put them back in the version they were originally */
				if (GDSV4 == sblkh_p->use.bkup.ondsk_blkver)
				{
					gds_blk_downgrade((v15_blk_hdr_ptr_t)blk_ptr, (blk_hdr_ptr_t)blk_ptr);
					fc->op_len = (((v15_blk_hdr_ptr_t)blk_ptr)->bsiz + 1) & ~1;
				} else
					fc->op_len = (((blk_hdr_ptr_t)blk_ptr)->bsiz + 1) & ~1;
			} else
				fc->op_len = old_data->blk_size;
			rest_blks++;
			dbfilop(fc);
		}
		/* Next section is the file header which we need to restore. */
		ptr1 = inbuf;
		size1 = ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE);
		ptr1_top = ptr1 + size1;
		fc->op_len = size1;
		assert(size1 <= mubmaxblk);
		COMMON_READ(common, ptr1, size1);
		((sgmnt_data_ptr_t)inbuf)->start_vbn = old_data->start_vbn;
		((sgmnt_data_ptr_t)inbuf)->free_space = ((old_data->start_vbn - 1) * DISK_BLOCK_SIZE) - SIZEOF_FILE_HDR(inbuf);
		fc->op_buff = inbuf;	/* reset since no block_id for header */
		fc->op_pos = 1;
		dbfilop(fc);
		size1 = ROUND_UP(((sgmnt_data_ptr_t)inbuf)->master_map_len, DISK_BLOCK_SIZE);
		COMMON_READ(common, inbuf, SIZEOF(HDR_MSG));
		if (MEMCMP_LIT(inbuf, HDR_MSG))
		{	/* We didn't read the record we were supposed to. We just wrecked the db most likely */
			util_out_print("Invalid information in restore file !AD. Aborting restore.",
					TRUE, ptr->input_file.len,
					ptr->input_file.addr);
			assert(FALSE);
			free(inbuf);
			free(old_data);
			free(inhead);
			mupip_exit(ERR_MUPRESTERR);
		}
		/* Now for the master map. Use size gleened from master map length */
		ptr1 = inbuf;
		ptr1_top = ptr1 + size1;
		fc->op_len = size1;
		for (;ptr1 < ptr1_top ; ptr1 += size1)
		{
			if ((size1 = ptr1_top - ptr1) > mubmaxblk)
				size1 = (mubmaxblk / DISK_BLOCK_SIZE) * DISK_BLOCK_SIZE;
			COMMON_READ(common, ptr1, size1);
		}
		fc->op_buff = inbuf;	/* reset since no block_id for header */
		fc->op_pos = MM_BLOCK;
		dbfilop(fc);
		COMMON_READ(common, inbuf, SIZEOF(MAP_MSG));
		if (MEMCMP_LIT(inbuf, MAP_MSG))
		{	/* We didn't read the record we were supposed to. We just wrecked the db most likely */
			util_out_print("Invalid information in restore file !AD. Aborting restore.",
					TRUE, ptr->input_file.len,
					ptr->input_file.addr);
			assert(FALSE);
			free(inbuf);
			free(old_data);
			free(inhead);
			mupip_exit(ERR_MUPRESTERR);
		}
		curr_tn = inhead->end_tn;
		switch(type)
		{
			case backup_to_file:
				if (RMS$_NORMAL != (status = sys$close(&infab)))
				{
					gtm_putmsg(VARLSTCNT(1) status);
					util_out_print("WARNING:  DB file !AD restore aborted, file !AD not valid", TRUE,
						DB_LEN_STR(gv_cur_region),
						ptr->input_file.len, ptr->input_file.addr);;
					free(inbuf);
					free(old_data);
					free(inhead);
					mupip_exit(ERR_MUPRESTERR);
				}
				break;
			case backup_to_exec:
#			ifdef BACKUP_TO_EXEC
				close(in->fd);
				if ((pipe_child > 0) && (FALSE != is_proc_alive(pipe_child, 0)))
					 waitpid(pipe_child, &status, 0);	/* BYPASSOK */
#			endif
				break;
			case backup_to_tcp:
				tcp_routines.aa_close(backup_socket);
				break;
		}
	}
	util_out_print("!/RESTORE COMPLETED", TRUE);
	util_out_print("!UL blocks restored", TRUE, rest_blks);
	free(inbuf);
	free(old_data);
	free(inhead);
	mupip_exit(SS$_NORMAL);
}

static void record_read(char *temp, char *buf, int nbytes) /* *nbytes is what we are asking, normally, is what we get + 4 */
{
	struct RAB	*rab;
	int4		status;

	rab = (struct RAB *)(temp);
	rab->rab$w_usz = nbytes;
	rab->rab$l_ubf = buf;
	status = sys$get(rab);
	if (RMS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		util_out_print("Error accessing input file !AD. Aborting restore.", TRUE,
			rab->rab$l_fab->fab$b_fns, rab->rab$l_fab->fab$l_fna);
		sys$close(rab->rab$l_fab);
		mupip_exit(status);
	}
	assert(nbytes == (int)(rab->rab$w_rsz) || 0 == MEMCMP_LIT(buf, "GDS"));
	return;
}

static void tcp_read(char *temp, char *buf, int nbytes) /* asking for *nbytes, have to return *nbytes */
{
	int		socket, needed, status;
	char		*curr;
	fd_set		fs;
	ABS_TIME	nap;

	needed = nbytes;
	curr = buf;
	socket = *(int *)(temp);
	nap.at_sec = 1;
	nap.at_usec = 0;
	while (1)
	{
		FD_ZERO(&fs);
		FD_SET(socket, &fs);
		assert(0 != FD_ISSET(socket, &fs));
		status = tcp_routines.aa_select(socket + 1, (void *)(&fs), (void *)0, (void *)0, &nap);
		if (status > 0)
		{
			status = tcp_routines.aa_recv(socket, curr, needed, 0);
			if ((0 == status) || (needed == status))	/* lost connection or all set */
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
			gtm_putmsg(VARLSTCNT(1) errno);
			tcp_routines.aa_close(socket);
			restore_read_errno = errno;
			break;
		}
	}
	return;
}
