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
#include "gtm_inet.h"	/* Required for gtmsource.h */
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include <clidef.h>
#include <iodef.h>
#include <jpidef.h>
#include <rms.h>
#include <ssdef.h>
#include <prtdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <errno.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "error.h"
#include "mupipbckup.h"
#include "vmsdtype.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "iosp.h"
#include "gbldirnam.h"
#include "repl_sem.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "util.h"
#include "mu_rndwn_file.h"
#include "mu_rndwn_replpool.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#include "del_sec.h"
#include "fid_from_sec.h"
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "gtm_logicals.h"

#define SYS_EXC 0
#define MAILBOX_SIZE 512

GBLREF tp_region	*grlist;
GBLREF bool		in_backup;
GBLREF bool		error_mupip;
GBLREF gd_region	*gv_cur_region;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF boolean_t	mu_star_specified;
GBLREF boolean_t	mu_rndwn_process;
static readonly $DESCRIPTOR(d_pnam, "GTM$MURNDWNPRC");
static uint4		rndwn_pid;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MUDESTROYFAIL);
error_def(ERR_MUDESTROYSUC);
error_def(ERR_MUFILRNDWNFL);
error_def(ERR_MUFILRNDWNSUC);
error_def(ERR_MUJPOOLRNDWNFL);
error_def(ERR_MUJPOOLRNDWNSUC);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUNOTALLSEC);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUREPLSECDEL);
error_def(ERR_MUREPLSECNOTDEL);
error_def(ERR_MURPOOLRNDWNFL);
error_def(ERR_MURPOOLRNDWNSUC);
error_def(ERR_MUSECDEL);
error_def(ERR_MUSECNOTDEL);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(mupip_rundown_ch)
{
	START_CH;
	if ((0 != rndwn_pid) && !(SEVERITY & SUCCESS))
	{
		if (DUMPABLE)
		{
			sys$delprc(NULL, &d_pnam);
			if (!SUPPRESS_DUMP)
				TERMINATE;
		} else
			UNWIND(NULL, NULL);
	} else if (DUMPABLE && !SUPPRESS_DUMP)
		TERMINATE;
	NEXTCH;
}

void mupip_rundown(void)
{
	uint4		channel, exit_status, flags, status;
	unsigned int	full_len;
	unsigned short	iosb[4];
	unsigned char	*c, mbuff[MAILBOX_SIZE];
	boolean_t	region, file, arg_present;
	file_control	*fc;
	tp_region	*rptr;
	char            name_buff[GLO_NAME_MAXLEN], res_name[MAX_NAME_LEN + 2]; /* +1 for the terminating null and another +1 for
										the length stored in [0] by global_name() */
	boolean_t	sgmnt_found;
	mstr		gbldir_mstr, *tran_name;
	gds_file_id	file_id;
	replpool_identifier	replpool_id;
	struct dsc$descriptor_s name_dsc;
	$DESCRIPTOR(d_sec, mbuff);
	static readonly $DESCRIPTOR(d_cmd, "install lis/glo");
	static readonly $DESCRIPTOR(d_mnam, "GTM$MURNDWNMBX");

	exit_status = SS$_NORMAL;
	mu_rndwn_process = TRUE;
	mu_outofband_setup();

	file = (CLI_PRESENT == cli_present("FILE"));
	region = (CLI_PRESENT == cli_present("REGION"));
	arg_present = (CLI_PRESENT == cli_present("DBFILE"));
	if (arg_present && !file && !region)
	{
		util_out_print("MUPIP RUNDOWN only accepts a parameter when -FILE or -REGION is specified.", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	if (!arg_present)
	{
		mu_gv_cur_reg_init();
		status = sys$crembx(0, &channel, SIZEOF(mbuff), 0, 0, PSL$C_USER, &d_mnam);
		if (SS$_NORMAL != status)
			mupip_exit(status);
		flags = CLI$M_NOWAIT | CLI$M_NOLOGNAM;
		ESTABLISH(mupip_rundown_ch);
		status = lib$spawn(&d_cmd, 0, &d_mnam, &flags, &d_pnam, &rndwn_pid);
		if (SS$_NORMAL != status)
		{
			if (SS$_DUPLNAM == status)
				util_out_print("Spawned process GTM$MURNDWNPRC already exists, cannot continue rundown", TRUE);
				util_out_print("If the prior RUNDOWN ended abnormally, STOP GTM$MURNDWNPRC and retry", TRUE);
			mupip_exit(status);
		}
		for (; ;)
		{
			status = sys$qiow(EFN$C_ENF, channel, IO$_READVBLK, &iosb, 0, 0, mbuff, SIZEOF(mbuff), 0, 0, 0, 0);
			if (SS$_NORMAL != status)
			{
				mupip_exit(status);
				break;
			}
			if (SS$_ENDOFFILE == iosb[0])
				break;
			if (SS$_NORMAL != iosb[0])
			{
				mupip_exit(iosb[0]);
				break;
			}
			if ((FALSE == mu_ctrly_occurred) && (FALSE == mu_ctrlc_occurred))
			{
				if (0 == memcmp("GT$S", mbuff, SIZEOF("GT$S") - 1))
				{
					for (c = mbuff; *c > 32; c++)
						;
					d_sec.dsc$w_length = c - mbuff;
					fid_from_sec(&d_sec, &FILE_INFO(gv_cur_region)->file_id);
					status = mu_rndwn_file(FALSE);
					if (gv_cur_region->read_only)
						status = RMS$_PRV;
					if (SS$_NORMAL == status)
					{
						sys$dassgn(FILE_INFO(gv_cur_region)->fab->fab$l_stv);
						gv_cur_region->open = FALSE;
					} else
					{
						if (RMS$_FNF == status)
							status = del_sec(SEC$M_SYSGBL, &d_sec, 0);
					}
					if (status & 1)
						rts_error(VARLSTCNT(4) ERR_MUSECDEL, 2, d_sec.dsc$w_length, d_sec.dsc$a_pointer);
					else
					{
						if (status)
							gtm_putmsg(VARLSTCNT(1) status);
						rts_error(VARLSTCNT(4) ERR_MUSECNOTDEL, 2, d_sec.dsc$w_length, d_sec.dsc$a_pointer);
						exit_status = ERR_MUNOTALLSEC;
					}
				} else if ((0 == memcmp("GT$P", mbuff, SIZEOF("GT$P") - 1)) ||
						(0 == memcmp("GT$R", mbuff, SIZEOF("GT$R") - 1)))
				{
					for (c = mbuff; *c > 32; c++)
                                                ;
                                        mbuff[c - mbuff] = '\0';
					strcpy(replpool_id.repl_pool_key, mbuff);
					if (!memcmp("GT$P", mbuff, SIZEOF("GT$P") - 1))
						replpool_id.pool_type = JNLPOOL_SEGMENT;
					else
						replpool_id.pool_type = RECVPOOL_SEGMENT;
					sgmnt_found = FALSE;
					if (mu_rndwn_replpool(&replpool_id, TRUE, &sgmnt_found) && sgmnt_found)
						rts_error(VARLSTCNT(4) ERR_MUREPLSECDEL, 2, LEN_AND_STR(mbuff));
					else if (sgmnt_found)
					{
						rts_error(VARLSTCNT(4) ERR_MUREPLSECNOTDEL, 2, LEN_AND_STR(mbuff));
						exit_status = ERR_MUNOTALLSEC;
					}
				}
			}
		}
		rndwn_pid = 0;
		REVERT;
		mupip_exit(exit_status);
	} else
	{
		if (region)
		{
			gvinit();
			region = TRUE;
			mu_getlst("DBFILE", SIZEOF(tp_region));
			rptr = grlist;
			if (error_mupip)
				exit_status = ERR_MUNOTALLSEC;
		} else
		{
			region = FALSE;
			mu_gv_cur_reg_init();
			gv_cur_region->dyn.addr->fname_len = SIZEOF(gv_cur_region->dyn.addr->fname);
			if (0 == cli_get_str("DBFILE", (char *)&gv_cur_region->dyn.addr->fname,
					&gv_cur_region->dyn.addr->fname_len))
				mupip_exit(ERR_MUNODBNAME);
		}
		for (; ; rptr = rptr->fPtr)
		{
			if (region)
			{
				if (NULL == rptr)
					break;
				if (dba_usr == rptr->reg->dyn.addr->acc_meth)
				{
					util_out_print("!/Can't RUNDOWN region !AD; not GDS format", TRUE, REG_LEN_STR(rptr->reg));
					continue;
				}
				if (!mupfndfil(rptr->reg, NULL))
				{
					exit_status = ERR_MUNOTALLSEC;
					continue;
				}
				gv_cur_region = rptr->reg;
				if (NULL == gv_cur_region->dyn.addr->file_cntl)
				{
					gv_cur_region->dyn.addr->acc_meth = dba_bg;
					gv_cur_region->dyn.addr->file_cntl =
						(file_control *)malloc(SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					memset(gv_cur_region->dyn.addr->file_cntl, 0, SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
					gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
					gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(SIZEOF(GDS_INFO));
					memset(gv_cur_region->dyn.addr->file_cntl->file_info, 0, SIZEOF(GDS_INFO));
				}
			}
			status = mu_rndwn_file(FALSE);
			if (SS$_NORMAL == status)
			{
#ifdef	IPCRM_FOR_SANCHEZ_ONLY
				global_name("GT$S", &FILE_INFO(gv_cur_region)->file_id, name_buff);
				name_dsc.dsc$a_pointer = &name_buff[1];
				name_dsc.dsc$w_length = (short)name_buff[0];
				name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
				name_dsc.dsc$b_class = DSC$K_CLASS_S;
				status = del_sec(SEC$M_SYSGBL, &name_dsc, 0);
#endif
				sys$dassgn(FILE_INFO(gv_cur_region)->fab->fab$l_stv);
			}
			if (status & 1)
			{
#ifdef	IPCRM_FOR_SANCHEZ_ONLY
				rts_error(VARLSTCNT(6) ERR_MUDESTROYSUC, 4,
					name_dsc.dsc$w_length, name_dsc.dsc$a_pointer, DB_LEN_STR(gv_cur_region));
#else
				rts_error(VARLSTCNT(4) ERR_MUFILRNDWNSUC, 2, DB_LEN_STR(gv_cur_region));
#endif
			}
			else
			{
				if (status)
					gtm_putmsg(VARLSTCNT(1) status);
#ifdef	IPCRM_FOR_SANCHEZ_ONLY
				rts_error(VARLSTCNT(6) ERR_MUDESTROYFAIL, 4,
					name_dsc.dsc$w_length, name_dsc.dsc$a_pointer, DB_LEN_STR(gv_cur_region));
				exit_status = ERR_MUNOACTION;
#else
				gtm_putmsg(VARLSTCNT(4) ERR_MUFILRNDWNFL, 2, DB_LEN_STR(gv_cur_region));
				exit_status = ERR_MUNOTALLSEC;
#endif
			}
			if ((FALSE == region) || (TRUE == mu_ctrly_occurred) || (TRUE == mu_ctrlc_occurred))
				break;
		}
		if (region && mu_star_specified)
		{
                        gbldir_mstr.addr = GTM_GBLDIR;
                        gbldir_mstr.len = SIZEOF(GTM_GBLDIR) - 1;
                        tran_name = get_name(&gbldir_mstr);
			memcpy(replpool_id.gtmgbldir, tran_name->addr, tran_name->len);
                        replpool_id.gtmgbldir[tran_name->len] = '\0';
			full_len = tran_name->len;
			if (!get_full_path(replpool_id.gtmgbldir, tran_name->len,
					replpool_id.gtmgbldir, &full_len, SIZEOF(replpool_id.gtmgbldir), &status))
			{
				util_out_print("Failed to get full path for gtmgbldir, !AD", TRUE, tran_name->len, tran_name->addr);
				gtm_putmsg(VARLSTCNT(1) status);
				exit_status = ERR_MUNOTALLSEC;
			} else
			{
				tran_name->len = full_len;	/* since on vax, mstr.len is a 'short' */
				set_gdid_from_file((gd_id_ptr_t)&file_id, replpool_id.gtmgbldir, tran_name->len);
				global_name("GT$P", &file_id, res_name); /* P - Stands for Journal Pool */
				res_name[res_name[0] + 1] = '\0';
				strcpy(replpool_id.repl_pool_key, &res_name[1]);
				replpool_id.pool_type = JNLPOOL_SEGMENT;
				sgmnt_found = FALSE;
				if (mu_rndwn_replpool(&replpool_id, FALSE, &sgmnt_found) && sgmnt_found)
					rts_error(VARLSTCNT(6) ERR_MUJPOOLRNDWNSUC, 4, res_name[0], &res_name[1],
							tran_name->len, replpool_id.gtmgbldir);
				else if (sgmnt_found)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNFL, 4, res_name[0], &res_name[1],
							tran_name->len, replpool_id.gtmgbldir);
					exit_status = ERR_MUNOTALLSEC;
				}
				global_name("GT$R", &file_id, res_name); /* R - Stands for Recv Pool */
				res_name[res_name[0] + 1] = '\0';
				strcpy(replpool_id.repl_pool_key, &res_name[1]);
				replpool_id.pool_type = RECVPOOL_SEGMENT;
				sgmnt_found = FALSE;
				if (mu_rndwn_replpool(&replpool_id, FALSE, &sgmnt_found) && sgmnt_found)
					rts_error(VARLSTCNT(6) ERR_MURPOOLRNDWNSUC, 4, res_name[0], &res_name[1],
							tran_name->len, replpool_id.gtmgbldir);
				else if (sgmnt_found)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_MURPOOLRNDWNFL, 4, res_name[0], &res_name[1],
							tran_name->len, replpool_id.gtmgbldir);
					exit_status = ERR_MUNOTALLSEC;
				}

			}
		}
	}
	mupip_exit(exit_status);
}
