/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include <unistd.h>
#include "gtm_stat.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "gtmio.h"
#include "cli.h"
#include "sleep.h"
#include "io_params.h"
#include "io.h"
#include "gtmsecshr.h"
#include "util.h"
#include "op.h"
#include "tp_change_reg.h"
#include "gtmrecv.h"
#include "gds_rundown.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "ftok_sems.h"

GBLREF	int4		mur_wrn_count;
GBLREF	mur_opt_struct	mur_options;
GBLREF	gd_region	*gv_cur_region;
GBLREF	seq_num		consist_jnl_seqno;
GBLREF	ctl_list	*jnl_files;
GBLREF	boolean_t	set_resync_to_region;
GBLREF	seq_num		seq_num_zero;
GBLREF	void            (*call_on_signal)();

void	mur_close_files(void)
{
	ctl_list	*ctl_ptr;
	int		fd, i;
	bool		go;
	struct shmid_ds	shm_buf;
	sgmnt_data	csd;
	static char	jnl_file_error[]="Error turning journaling back on for database !AD\n";
	char		*db_fn;
	int		db_fn_len, status;
	mval		val, pars;
	unsigned char	no_param;
	unsigned char	*ptr, qwstring[100];
	unsigned char	*ptr1, qwstring1[100];
	unsigned char	*ptr2, qwstring2[100];
	unsigned char	*ptr3, qwstring3[100];

	error_def(ERR_DBFILERR);
	error_def(ERR_TEXT);

	call_on_signal = NULL;	/* Do not recurs via call_on_signal if there is an error */
        db_fn_len = 0;

	if (mur_options.rollback && mur_options.fetchresync && CLI_PRESENT != cli_present("RESYNC"))
		gtmrecv_wait_for_detach();

	for (ctl_ptr = jnl_files;  ctl_ptr;  ctl_ptr = ctl_ptr->next)
	{
		if (ctl_ptr->next != NULL)
		{
			if (ctl_ptr->gd == ctl_ptr->next->gd)
			{
				if (ctl_ptr->rab)
				{
					if (0 != (status = mur_close(ctl_ptr->rab))) /* Close Journal file descriptor after
											rundown */
					{
						util_out_print(" Close failed for  journal file !AD\n", TRUE,
									ctl_ptr->jnl_fn_len, ctl_ptr->jnl_fn);
						mur_output_status(status);
						mur_wrn_count++;
					}
					ctl_ptr->rab = 0;
				}
				continue;
			}
		}
		if (ctl_ptr->gd && ctl_ptr->gd->dyn.addr->fname_len)
		{
			db_fn_len = ctl_ptr->gd->dyn.addr->fname_len;
			db_fn = (char *)ctl_ptr->gd->dyn.addr->fname;
			gv_cur_region = ctl_ptr->gd;
			if (gv_cur_region->open)	/* Cannot close what is not open */
			{
				tp_change_reg();
				gds_rundown();
				if (-1 != (fd = OPEN(db_fn, O_RDWR)))
				{
					for (go = FALSE, i = 0;  i < 10;  i++)
					{
						if (-1 == shmctl(
							((unix_db_info *)(ctl_ptr->gd->dyn.addr->file_cntl->file_info))->shmid,
							 IPC_STAT,
							 &shm_buf))
						{
							if (errno == EINVAL || errno == EIDRM) /* EIDRM is a valid case on LINUX */
							{
								go = TRUE;
								break;
							} else
							{
								status = errno;
								mur_output_status(status);
								mur_wrn_count++;
								m_sleep(1);
							}
						}
					}
					if (go)
					{
						LSEEKREAD(fd, 0, &csd, sizeof(csd), status);
						if (0 == status)
						{
							csd.jnl_state = ctl_ptr->jnl_state;
							csd.repl_state = ctl_ptr->repl_state;
							if(mur_options.update)
								csd.file_corrupt = FALSE;
							if (QWNE(seq_num_zero, consist_jnl_seqno))
							{
								if (set_resync_to_region)
								{
									QWASSIGN(csd.resync_seqno, csd.reg_seqno);
									ptr = i2ascl(qwstring, csd.resync_seqno);
									ptr1 = i2asclx(qwstring1, csd.resync_seqno);
									util_out_print(
									 "Setting Resync Seqno !AD [0x!AD] for Region !AD -->",
									 TRUE, ptr-qwstring, qwstring, ptr1 - qwstring1,
									 qwstring1, db_fn_len, db_fn);
									ptr2 = i2ascl(qwstring2, csd.reg_seqno);
									ptr3 = i2asclx(qwstring3, csd.reg_seqno);
									util_out_print(" to Region Seqno !AD [0x!AD]", TRUE,
										        ptr2-qwstring2, qwstring2,
											ptr3 - qwstring3, qwstring3);
								}
								QWASSIGN(csd.reg_seqno, consist_jnl_seqno);
								if (QWGT(csd.resync_seqno, consist_jnl_seqno))
									QWASSIGN(csd.resync_seqno, consist_jnl_seqno);
							}
							LSEEKWRITE(fd, 0, &csd, sizeof(csd), status);
							if (0 != status)
							{
								util_out_print("Write Failed for Database file", TRUE,
									db_fn_len, db_fn);
								mur_wrn_count++;
							}
						} else
						{
							util_out_print("Read Failed for Database file", TRUE, db_fn_len, db_fn);
							mur_wrn_count++;
						}
					} else
					{
						mur_output_status(errno);
						util_out_print(jnl_file_error, TRUE, db_fn_len, db_fn);
						mur_wrn_count++;
					}
					close(fd);
				} else
				{
					util_out_print("journal file close failed for !AD\n", TRUE, db_fn_len, db_fn);
					mur_wrn_count++;
				}
				if (!db_ipcs_reset(ctl_ptr->gd, FALSE))
				{
					gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, db_fn_len, db_fn,
						   ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to do db_ipcs_reset"));
					mur_wrn_count++;
				}
			} else
			{
				util_out_print("journal file close failed for !AD\n", TRUE, db_fn_len, db_fn);
				mur_wrn_count++;
				continue;
			}
		}
		if (ctl_ptr->rab)
		{
			if (0 != (status = mur_close(ctl_ptr->rab))) /* Close Journal file descriptor after rundown */
			{
				util_out_print(" Close failed for  journal file !AD\n", TRUE, ctl_ptr->jnl_fn_len, ctl_ptr->jnl_fn);
				mur_output_status(status);
				mur_wrn_count++;
			}
			ctl_ptr->rab = 0;
		}
	}
	if (mur_options.extr_file_info && (NULL != ((unix_file_info *)(mur_options.extr_file_info))->fn ))
	{
		no_param = (unsigned char)iop_eol;
		pars.mvtype = MV_STR;
		pars.str.len = sizeof(no_param);
		pars.str.addr = (char *)&no_param;
		val.mvtype = MV_STR;
		val.str.len = ((unix_file_info *)mur_options.extr_file_info)->fn_len;
		val.str.addr = (char *) (((unix_file_info *)(mur_options.extr_file_info))->fn);
		op_close(&val, &pars);
	}

	if (mur_options.losttrans_file_info && (NULL != ((unix_file_info *)(mur_options.losttrans_file_info))->fn ))
	{
		no_param = (unsigned char)iop_eol;
		pars.mvtype = MV_STR;
		pars.str.len = sizeof(no_param);
		pars.str.addr = (char *)&no_param;
		val.mvtype = MV_STR;
		val.str.len = ((unix_file_info *)mur_options.losttrans_file_info)->fn_len;
		val.str.addr = (char *) (((unix_file_info *)(mur_options.losttrans_file_info))->fn);
		op_close(&val, &pars);
	}

	if (mur_options.brktrans_file_info && (NULL != ((unix_file_info *)(mur_options.brktrans_file_info))->fn ))
	{
		no_param = (unsigned char)iop_eol;
		pars.mvtype = MV_STR;
		pars.str.len = sizeof(no_param);
		pars.str.addr = (char *)&no_param;
		val.mvtype = MV_STR;
		val.str.len = ((unix_file_info *)mur_options.brktrans_file_info)->fn_len;
		val.str.addr = (char *) (((unix_file_info *)(mur_options.brktrans_file_info))->fn);
		op_close(&val, &pars);
	}
	return;
}
