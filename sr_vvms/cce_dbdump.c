/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <clidef.h>
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <descrip.h>
#include <ssdef.h>
#include <secdef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "util.h"
#include "mem_list.h"
#include "cce_sec_size.h"
#include "init_sec.h"
#include "fid_from_sec.h"

OS_PAGE_SIZE_DECLARE
#define RECORD_SIZE	1024
#define EXTRA_SPACE	256

typedef struct
{	unsigned short status;
	unsigned short char_ct;
	uint4 pid;
}m_iosb;

void cce_dbdump(void)
{
	uint4	channel, status, flags, pid, real_size, req_size, size, addrs[2], sec_addrs[2];
	int		i, j, k, l;
	gds_file_id	file;
	m_iosb		stat_blk;
	sgmnt_data	*sd;
	unsigned char	mbuff[512], *c, *cptr, *ctop;
	$DESCRIPTOR(d_sec,mbuff);
	static readonly $DESCRIPTOR(d_cmd,"install lis/glo");
	static readonly $DESCRIPTOR(d_mnam,"CCE$DBDUMPMBX");
	static readonly $DESCRIPTOR(d_pnam,"CCE$DBDUMPPRC");
	char		filename[]="SYS$LOGIN:CCE_DBDUMP.DMP", buff[RECORD_SIZE], id_lab[]=" FILE ID:";
	struct FAB	fab;
	struct RAB	rab;
	error_def(ERR_CCEDBDUMP);
	error_def(ERR_CCEDBNODUMP);


	util_out_open(0);
	status = sys$crembx(0, &channel, 512, 0, 0, PSL$C_USER, &d_mnam);
	if (status != SS$_NORMAL)
		sys$exit(status);
	flags = CLI$M_NOWAIT | CLI$M_NOLOGNAM;
	status = lib$spawn(&d_cmd, 0, &d_mnam, &flags, &d_pnam, &pid);
	if (status != SS$_NORMAL)
	{	if (status == SS$_DUPLNAM)
		{	util_out_print("Spawned process CCE$DBDUMPPRC already exists, cannot continue rundown",TRUE);
		}
		sys$exit(status);
	}
	/* the following guess at the dump file size is modeled on the calculation for a section */
	size = DIVIDE_ROUND_UP((SIZEOF(sgmnt_data) + (WC_MAX_BUFFS + getprime(WC_MAX_BUFFS) + 1) * SIZEOF(bt_rec)
				+ (DEF_LOCK_SIZE / OS_PAGELET_SIZE)
				 + (WC_MAX_BUFFS + getprime(WC_MAX_BUFFS)) * SIZEOF(cache_rec) + SIZEOF(cache_que_heads)),
				OS_PAGELET_SIZE);
	size += EXTRA_SPACE;

	fab = cc$rms_fab;
	fab.fab$b_fac = FAB$M_PUT;
	fab.fab$l_fop = FAB$M_CBT | FAB$M_MXV | FAB$M_TEF;
	fab.fab$l_fna = filename;
	fab.fab$b_fns = SIZEOF(filename);
	fab.fab$b_rfm = FAB$C_FIX;
	fab.fab$w_mrs = RECORD_SIZE;
	fab.fab$w_deq = size;
	fab.fab$l_alq = size;
	switch (status = sys$create(&fab))
	{
	case RMS$_NORMAL:
	case RMS$_CREATED:
	case RMS$_SUPERSEDE:
	case RMS$_FILEPURGED:
		break;
	default:
		util_out_print("Error: Cannot create dump file !AD.",TRUE,fab.fab$b_fns,fab.fab$l_fna);
		sys$exit(status);
	}

	rab = cc$rms_rab;
	rab.rab$l_fab = &fab;
	status = sys$connect(&rab);
	if (status != RMS$_NORMAL)
	{	util_out_print("Error: Cannot connect to dump file !AD.",TRUE,fab.fab$b_fns,fab.fab$l_fna);
		sys$exit(status);
	}
	rab.rab$w_rsz = SIZEOF(buff);

	for (; ;)
	{	status = sys$qiow (0, channel,IO$_READVBLK ,&stat_blk, 0, 0, mbuff, 512,0,0,0,0);
		if (status != SS$_NORMAL)
		{	sys$exit(status);
			break;
		}
		if (stat_blk.status == SS$_ENDOFFILE)
			break;
		if (stat_blk.status != SS$_NORMAL)
		{	sys$exit(stat_blk.status);
			break;
		}
		if (!memcmp("GT$S",mbuff,4))
		{	for ( c = mbuff; *c > 32 ; c++)
				;
			d_sec.dsc$w_length = c - mbuff;
			flags = SEC$M_GBL | SEC$M_WRT | SEC$M_SYSGBL | SEC$M_PAGFIL | SEC$M_DZRO | SEC$M_PERM;
			addrs[0] = addrs[1] = 0;
			fid_from_sec(&d_sec,&file);

			real_size = cce_sec_size(&file);
			if (real_size == 0)
				real_size = size;
			real_size += 1;

			assert(OS_PAGE_SIZE % OS_PAGELET_SIZE == 0);

			/* Request enough pagelets to ensure enough contiguous pages to contain desired number of pagelets. */
			req_size = ROUND_UP(real_size, OS_PAGE_SIZE);
			lib$get_vm_page(&req_size, &addrs[0]);

			/* addrs will hold addresses of start and end of contiguous block of pagelets for use by $deltva. */
			assert((addrs[0] + (req_size * OS_PAGELET_SIZE) - 1) == addrs[1]);

			/* $get_vm_page returns pagelets; we must align to integral page boundary. */
			/* sec_addrs will contain the starting and ending addresses of the mapped section. */
			sec_addrs[0] = ROUND_UP(addrs[0], OS_PAGE_SIZE);	/* align to first integral page boundary */
			sec_addrs[1] = addrs[0] + (real_size * OS_PAGELET_SIZE);
			sec_addrs[1] = ROUND_UP(addrs[1], OS_PAGE_SIZE) - 1;  /* A(last byte of last page) */

			status = init_sec(sec_addrs, &d_sec, 0, real_size, flags);
			if (status & 1)
			{
				sd = sec_addrs[0];
				memset(buff, 0, RECORD_SIZE);
				memcpy(buff, d_sec.dsc$a_pointer, d_sec.dsc$w_length);
				cptr = &buff[0] + d_sec.dsc$w_length;
				memcpy(cptr,id_lab,SIZEOF(id_lab));
				cptr += SIZEOF(id_lab);
				memcpy(cptr,&file,SIZEOF(file));
				rab.rab$l_rbf = buff;
				status = sys$put(&rab);
				if (status != RMS$_NORMAL)
				{	util_out_print("Error writing to dump file !AD.",TRUE,fab.fab$b_fns,fab.fab$l_fna);
					util_out_print("Status code is !UL.",TRUE,status);
					break;
				}
				for (c = sd, i = 0; (real_size + EXTRA_SPACE) >= i;
					c += RECORD_SIZE, i += (RECORD_SIZE / DISK_BLOCK_SIZE))
				{
					rab.rab$l_rbf = c;
					status = sys$put(&rab);
					if (status != RMS$_NORMAL)
					{	util_out_print("Error writing to dump file !AD.",TRUE,fab.fab$b_fns,fab.fab$l_fna);
						util_out_print("Status code is !UL.",TRUE,status);
						break;
					}
				}
				lib$signal(ERR_CCEDBDUMP,2,d_sec.dsc$w_length,d_sec.dsc$a_pointer);
			}else
			{	lib$signal(ERR_CCEDBNODUMP,2,d_sec.dsc$w_length,d_sec.dsc$a_pointer,status,0);
			}
			gtm_deltva(&addrs[0],0,0);
			lib$free_vm_page(&real_size,&addrs[0]);
		}
	}
	sys$exit(SS$_NORMAL);
}
