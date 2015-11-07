/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <starlet.h>
#include <efn.h>
#include <descrip.h>
#include <rms.h>
#include <dcdef.h>
#include <devdef.h>
#include <dvidef.h>
#include <acedef.h>
#include <ssdef.h>
#include <stddef.h>
#include "gtm_string.h"

#include "vmsdtype.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "min_max.h"

#define ESC		27
#define DFLT_FILE_EXT	".DAT"
#define VREC_HDR_LEN	4
#define VFC_MAX_RECLEN	(32767 - 2)	/* have to leave two bytes for the fixed control */

error_def(ERR_RMWIDTHTOOBIG);
error_def(ERR_TEXT);
error_def(ERR_BIGNOACL);
error_def(ERR_DEVOPENFAIL);
error_def(ERR_VARRECBLKSZ);

short iorm_open(io_log_name *iol, mval *pp, int fd, mval *mspace, int4 timeout)
{
	int4		status;
	io_desc		*iod;		/* local pointer to io_curr_device */
	d_rm_struct	*d_rm;
	struct	XABFHC	xabfhc;
	struct	XABPRO	xabpro;
	struct RAB	*r;
	struct FAB	*f;
	struct NAM	*nam;
	mstr		newtln;
	struct dsc$descriptor_s devname, outname;
	uint4		width;
	uint4		acebin[128];	/* needs to be big enough for any other ACLs on file */
	uint4		*acebinptr;
	struct acedef	*aceptr;
	char		*acetop;
	boolean_t	acefound = FALSE, created = FALSE, isdisk = FALSE, noacl = FALSE;
	unsigned int	devclass, devchar, devchar2, devtype, dvistat, iosb[2];
	short		devclassret, devcharret, devchar2ret, devtyperet;
	struct
	{
		item_list_3	item[4];
		int		terminator;
	} item_list;
	unsigned char resultant_name[255];
	unsigned char	tmpfdns;
	/** unsigned char resultant_name[MAX_TRANS_NAME_LEN]; THIS WOULD BE RIGHT BUT MAX_TRANS_NAME_LEN MUST BE <= 255 **/

	/* while sr_unix/iorm_open.c prefixes errors with ERR_DEVOPENFAIL and it might be nice to be consistent */
	/* changing VMS after all this time could break user programs */
	/* An exception is being made for the extremely unlikely problem creating a GTM ACE so it stands out */
	iod = iol->iod;
	assert(*(pp->str.addr) < n_iops);
	assert(iod);
	assert(iod->state >= 0 && iod->state < n_io_dev_states);
	assert(rm == iod->type);
	if (dev_never_opened == iod->state)
	{
		iod->dev_sp = (d_rm_struct *)(malloc(SIZEOF(d_rm_struct)));
		d_rm = (d_rm_struct *)iod->dev_sp;
		memset(d_rm, 0, SIZEOF(*d_rm));
		iod->width = DEF_RM_WIDTH;
		iod->length = DEF_RM_LENGTH;
		r  = &d_rm->r;
		f  = &d_rm->f;
		*r  = cc$rms_rab;
		*f  = cc$rms_fab;
		r->rab$l_fab = f;
		r->rab$w_usz = d_rm->l_usz = DEF_RM_WIDTH;
		f->fab$w_mrs = d_rm->l_mrs = DEF_RM_WIDTH;
		f->fab$b_rfm = d_rm->b_rfm = FAB$C_VAR;	/* default is variable record format */
		f->fab$l_fop = FAB$M_CIF | FAB$M_SQO | FAB$M_CBT | FAB$M_NAM;
		f->fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_TRN; /* TRN allows truncate option to be specified in RAB later */
		f->fab$b_rat = FAB$M_CR;

		f->fab$l_dna = DFLT_FILE_EXT;
		f->fab$b_dns = SIZEOF(DFLT_FILE_EXT) - 1;
		d_rm->f.fab$l_nam = nam = malloc(SIZEOF(*nam));
		*nam = cc$rms_nam;
		nam->nam$l_esa = resultant_name;
		nam->nam$b_ess = SIZEOF(resultant_name);
		nam->nam$b_nop = NAM$M_NOCONCEAL;

		r->rab$l_rop = RAB$M_TMO | RAB$M_WBH | RAB$M_RAH;
		d_rm->promask = 0xFFFF;
	} else
	{
		d_rm = (d_rm_struct *)iod->dev_sp;
		if (dev_closed == iod->state)
			d_rm->f.fab$w_bls = 0; /* Reset the block size to pass the block-record check below.
						* The FAB initialization sets the block size later so it's OK to zero it here.
						*/
		nam = d_rm->f.fab$l_nam;
		nam->nam$l_esa = 0;
		nam->nam$b_ess = 0;
		nam->nam$b_esl = 0;
	}
	iorm_use(iod, pp);
	if (dev_open != iod->state)
	{
		if (!d_rm->largerecord && (d_rm->f.fab$w_bls > 0) && (FAB$C_FIX != d_rm->f.fab$b_rfm)
			&& (d_rm->f.fab$w_bls < (d_rm->r.rab$w_usz + VREC_HDR_LEN)))
				rts_error(VARLSTCNT(1) ERR_VARRECBLKSZ);
		d_rm->r.rab$l_ctx = FAB$M_GET;
		d_rm->f.fab$l_fna = iol->dollar_io;
		d_rm->f.fab$b_fns = iol->len;
/* smw next overrides any xab set by iorm_use */
		xabpro = cc$rms_xabpro;
		d_rm->f.fab$l_xab = &xabpro;
		memset(acebin, 0, SIZEOF(acebin));
		status = sys$parse(&d_rm->f);	/* to get device for getdvi */
		if ((1 & status))
		{
			devname.dsc$w_length = nam->nam$b_dev;
			devname.dsc$a_pointer = nam->nam$l_dev;
			devname.dsc$b_dtype = DSC$K_DTYPE_T;
			devname.dsc$b_class = DSC$K_CLASS_S;
			item_list.item[0].item_code = DVI$_DEVCLASS;
			item_list.item[0].buffer_length = SIZEOF(devclass);
			item_list.item[0].buffer_address = &devclass;
			item_list.item[0].return_length_address = &devclassret;
			item_list.item[1].item_code = DVI$_DEVCHAR;
			item_list.item[1].buffer_length = SIZEOF(devchar);
			item_list.item[1].buffer_address = &devchar;
			item_list.item[1].return_length_address = &devcharret;
			item_list.item[2].item_code = DVI$_DEVCHAR2;
			item_list.item[2].buffer_length = SIZEOF(devchar2);
			item_list.item[2].buffer_address = &devchar2;
			item_list.item[2].return_length_address = &devchar2ret;
			item_list.item[3].item_code = DVI$_DEVTYPE;
			item_list.item[3].buffer_length = SIZEOF(devtype);
			item_list.item[3].buffer_address = &devtype;
			item_list.item[3].return_length_address = &devtyperet;
			item_list.terminator = 0;
			dvistat = sys$getdviw(EFN$C_ENF, NULL, &devname, &item_list, iosb, NULL, 0, 0);
			if (SS$_NORMAL == dvistat)
				dvistat = iosb[0];
			if (SS$_NONLOCAL == dvistat || (SS$_NORMAL == dvistat &&
				((DC$_DISK != devclass || (DEV$M_NET & devchar) ||
				(DEV$M_DAP | DEV$M_DFS) & devchar2) || /* UCX NFS sets DFS */
				(DT$_FD1 <= devtype && DT$_FD8 >= devtype) ))) /* but not tcpware so check foreign disk */
			{	/* if not disk, dfs/nfs, or non local, create gets BADATTRIB in stv if acl buf and siz set */
				noacl = TRUE;
			}
		} else /* let create/open report the problem */
			noacl = TRUE;
		if (DEV$M_NET & d_rm->f.fab$l_dev)
		{	/* need to release sys$parse channel if DECnet */
			tmpfdns = d_rm->f.fab$b_dns;
			d_rm->f.fab$b_dns = 0;
			assert(0 == nam->nam$l_rlf);
			nam->nam$l_rlf = 0;
			nam->nam$b_nop |= NAM$M_SYNCHK;
			status = sys$parse(&d_rm->f);	/* give up channel */
			d_rm->f.fab$b_dns = tmpfdns;	/* restore */
			nam->nam$b_nop &= ~NAM$M_SYNCHK;
		}
		if (noacl)
		{
			if (d_rm->largerecord && MAX_RMS_RECORDSIZE < d_rm->l_mrs)
				rts_error(VARLSTCNT(1) ERR_RMWIDTHTOOBIG);
			d_rm->largerecord = FALSE;
		}
		if (d_rm->largerecord && FAB$M_GET != d_rm->f.fab$b_fac)
		{	/* if readonly use format from existing file */
			aceptr = acebin;
			aceptr->ace$b_size = GTM_ACE_SIZE * SIZEOF(uint4);
			aceptr->ace$b_type = ACE$C_INFO;
			/* without NOPROPAGATE, new versions will get ACE,
			   PROTECTED prevents set acl /dele unless =all */
			aceptr->ace$w_flags = ACE$M_NOPROPAGATE | ACE$M_PROTECTED;
/*			if HIDDEN, dir/sec does not display which may make it harder to check if problems
			aceptr->ace$w_flags |= ACE$M_HIDDEN;
*/
			aceptr->ace$v_info_type = ACE$C_CUST;	/* must be after flags */
			aceptr->ace$w_application_facility = GTM_ACE_FAC;	/* GTM error fac */
			aceptr->ace$w_application_flags = GTM_ACE_BIGREC;
			assert(SIZEOF(uint4) * GTM_ACE_LAB_OFF == (&aceptr->ace$t_info_start - (char *)aceptr));
			acebin[GTM_ACE_LAB_OFF] = GTM_ACE_LABEL;
			acebin[GTM_ACE_RFM_OFF] = d_rm->b_rfm;
			acebin[GTM_ACE_MRS_OFF] = d_rm->l_mrs;
			acebin[GTM_ACE_SIZE] = 0;	/* terminate */
			d_rm->f.fab$b_rfm = FAB$C_UDF;
			d_rm->f.fab$w_mrs = 0;
		}
		if (!noacl)
		{	/* tape gets BADATTRIB in stv if acl buf and siz set */
			xabpro.xab$l_aclbuf = acebin;
			xabpro.xab$w_aclsiz = SIZEOF(acebin);
		}
		if (FAB$M_GET == d_rm->f.fab$b_fac)
		{
			xabfhc = cc$rms_xabfhc;
			xabpro.xab$l_nxt = &xabfhc;
			status = sys$open(&d_rm->f);
		} else
		{
			xabpro.xab$w_pro = d_rm->promask;
			status = sys$create(&d_rm->f);
		}
		nam->nam$l_esa = 0;
		nam->nam$b_ess = 0;
		nam->nam$b_esl = 0;
		d_rm->f.fab$l_xab = 0;
		switch (status)
		{
		case RMS$_NORMAL:
		if (d_rm->f.fab$l_fop & FAB$M_MXV)
			created = iod->dollar.zeof = TRUE;
		break;

		case RMS$_CRE_STM:
		case RMS$_CREATED:
		case RMS$_SUPERSEDE:
		case RMS$_FILEPURGED:
		if (d_rm->f.fab$l_dev & DEV$M_FOD)
			created = iod->dollar.zeof = TRUE;
		break;

		case RMS$_ACT:
		case RMS$_FLK:
			return(FALSE);

		default:
			rts_error(VARLSTCNT(2) status, d_rm->f.fab$l_stv);
		}
		if (!noacl && (DEV$M_RND & d_rm->f.fab$l_dev) && !(DEV$M_NET & d_rm->f.fab$l_dev))
			isdisk = TRUE;	/* local disk */
		else if (created && d_rm->largerecord && MAX_RMS_RECORDSIZE < d_rm->l_mrs)
			rts_error(VARLSTCNT(1) ERR_RMWIDTHTOOBIG);
		/* $create does not return the ACE:  if a new file is created aclsts is IVACL            */
		/*				     if CIF and existing file has no acl aclsts ACLEMPTY */
		/*				     if CIF and existing file has acl aclsts is NORMAL   */
		if (isdisk && ((created && SS$_IVACL == xabpro.xab$l_aclsts) || (0 != xabpro.xab$l_aclsts &&
			(FAB$M_GET != d_rm->f.fab$b_fac && ((1 & xabpro.xab$l_aclsts) || SS$_ACLEMPTY != xabpro.xab$l_aclsts)))))
		{
			xabpro.xab$l_aclctx = 0;        /* reset context */
			d_rm->f.fab$l_xab = &xabpro;
			status = sys$display(&d_rm->f);
			d_rm->f.fab$l_xab = 0;		/* prevent close error */
			if (!(1 & status))
				rts_error(VARLSTCNT(2) status, d_rm->f.fab$l_stv);
			if (0 != xabpro.xab$l_aclsts && !(1 & xabpro.xab$l_aclsts) && SS$_ACLEMPTY != xabpro.xab$l_aclsts)
				rts_error(VARLSTCNT(1) xabpro.xab$l_aclsts);
		}
		if (isdisk && (1 & status) && 0 != xabpro.xab$l_aclsts && !(1 & xabpro.xab$l_aclsts) &&
				SS$_ACLEMPTY != xabpro.xab$l_aclsts)
			rts_error(VARLSTCNT(1) xabpro.xab$l_aclsts);
		if (isdisk && 0 != xabpro.xab$w_acllen && (1 & status))	/* acl and success */
		{
			if (SIZEOF(acebin) < xabpro.xab$w_acllen)
			{	/* get a new buffer big enough */
				xabpro.xab$l_aclbuf = malloc(xabpro.xab$w_acllen);
				xabpro.xab$w_aclsiz = xabpro.xab$w_acllen;
				xabpro.xab$l_aclctx = 0;	/* reset context */
				d_rm->f.fab$l_xab = &xabpro;
				status = sys$display(&d_rm->f);
				d_rm->f.fab$l_xab = 0;
				if (!(1 & status))
					rts_error(VARLSTCNT(2) status, d_rm->f.fab$l_stv);
				if (!(1 & xabpro.xab$l_aclsts))
					rts_error(VARLSTCNT(1) xabpro.xab$l_aclsts);
			}
			acetop = (char *)xabpro.xab$l_aclbuf + xabpro.xab$w_acllen;
			for (aceptr = xabpro.xab$l_aclbuf; aceptr < acetop; aceptr = (char *)aceptr + aceptr->ace$b_size)
			{
				if (0 == aceptr->ace$b_size)
					break;
				if (ACE$C_INFO == aceptr->ace$b_type && ACE$C_CUST == aceptr->ace$v_info_type
					&& GTM_ACE_FAC == aceptr->ace$w_application_facility
					&& GTM_ACE_BIGREC == aceptr->ace$w_application_flags)
				{	/* info for large records */
					acebinptr = aceptr;
					assert(GTM_ACE_LABEL == acebinptr[GTM_ACE_LAB_OFF]);
					d_rm->largerecord = TRUE;
					d_rm->b_rfm = (unsigned char)acebinptr[GTM_ACE_RFM_OFF];
					d_rm->l_mrs = acebinptr[GTM_ACE_MRS_OFF];
					acefound = TRUE;
					break;
				}
			}
			if (acebin != xabpro.xab$l_aclbuf)
			{	/* free larger buffer now */
				free(xabpro.xab$l_aclbuf);
				xabpro.xab$l_aclbuf = acebin;
				xabpro.xab$w_aclsiz = SIZEOF(acebin);
			}
		}
		if (!acefound)
		{
			if (!created)
			{	/* copy from exisiting file */
				if (isdisk && d_rm->largerecord && FAB$C_UDF == d_rm->f.fab$b_rfm)
					rts_error(VARLSTCNT(1) ERR_BIGNOACL);	/* maybe lost in copy */
				d_rm->b_rfm = d_rm->f.fab$b_rfm;
				d_rm->l_mrs = d_rm->f.fab$w_mrs;
			} else if (isdisk && d_rm->largerecord)
				rts_error(VARLSTCNT(8) ERR_DEVOPENFAIL, 2, iol->len, iol->dollar_io,
					ERR_TEXT, 2, LEN_AND_LIT("GTM ACE on new file disappeared - possible VMS problem"));
			d_rm->largerecord = FALSE;
		}
/* smw does next overwriting of mrs make sense to RMS */
		/* if not largerecord, read only, sequential, not magtape ... */
		if (!d_rm->largerecord && (FAB$M_GET == d_rm->f.fab$b_fac) && (0 == d_rm->f.fab$w_mrs) && xabfhc.xab$w_lrl)
			d_rm->l_mrs = d_rm->l_usz = d_rm->r.rab$w_usz = d_rm->f.fab$w_mrs = xabfhc.xab$w_lrl;

		if (d_rm->largerecord)
		{ /* guess at a good blocks per IO */
			uint4 blocksperrec;
			blocksperrec = DIVIDE_ROUND_UP(d_rm->l_mrs, RMS_DISK_BLOCK);
			if (RMS_MAX_MBC <= blocksperrec * 2)
				d_rm->r.rab$b_mbc = RMS_MAX_MBC;
			else if (RMS_DEF_MBC < blocksperrec * 2)
				d_rm->r.rab$b_mbc = blocksperrec * 2;
		}
		status = sys$connect(&d_rm->r);
		if (RMS$_NORMAL != status)
			rts_error(VARLSTCNT(2) status, d_rm->r.rab$l_stv);
		if (d_rm->r.rab$l_rop & RAB$M_EOF)
			iod->dollar.zeof = TRUE;
		if (ESC == iod->trans_name->dollar_io[0])
		{
			/* process permanent file...get real name */
			status = sys$display(&d_rm->f);
			if (status & 1)
			{
				devname.dsc$w_length = nam->nam$t_dvi[0];
				devname.dsc$b_dtype = DSC$K_DTYPE_T;
				devname.dsc$b_class = DSC$K_CLASS_S;
				devname.dsc$a_pointer = &nam->nam$t_dvi[1];
				outname.dsc$w_length = SIZEOF(resultant_name);
				outname.dsc$b_dtype = DSC$K_DTYPE_T;
				outname.dsc$b_class = DSC$K_CLASS_S;
				outname.dsc$a_pointer = resultant_name;
				status = lib$fid_to_name(&devname, &nam->nam$w_fid, & outname, &newtln.len, 0, 0);
				if ((status & 1) && (0 != newtln.len))
				{
					newtln.addr = resultant_name;
					iod->trans_name = get_log_name(&newtln, INSERT);
					iod->trans_name->iod = iod;
				}
			}
		} else
		{	/* smw since esl zeroed above this is dead code since early days */
			if (nam->nam$b_esl && (iod->trans_name->len != nam->nam$b_esl ||
				memcmp(&iod->trans_name->dollar_io[0], resultant_name, nam->nam$b_esl)))
			{
				newtln.addr = resultant_name;
				newtln.len = nam->nam$b_esl;
				iod->trans_name = get_log_name(&newtln, INSERT);
				iod->trans_name->iod = iod;
			}
		}
		if (0 == d_rm->l_mrs)
			d_rm->l_mrs = iod->width;
		iod->width = d_rm->l_usz = d_rm->l_mrs;
		if (!d_rm->largerecord)
		{
			d_rm->r.rab$w_usz = d_rm->f.fab$w_mrs = d_rm->l_mrs;
			if (FAB$C_VFC == d_rm->f.fab$b_rfm) /* have to leave two bytes for the fixed control */
				iod->width = MIN(iod->width, VFC_MAX_RECLEN);
		}
		width = iod->width;
		if (d_rm->largerecord)
		{
			width = ROUND_UP(width, SIZEOF(uint4));
			if (FAB$C_VAR == d_rm->b_rfm)
				width += SIZEOF(uint4);	/* for count */
		}
		d_rm->bufsize = width + 1;
		d_rm->inbuf = (char*)malloc(width + 1);
		d_rm->outbuf_start = (char*)malloc(width + 1);
		d_rm->inbuf_pos = d_rm->inbuf;
		d_rm->inbuf_top = d_rm->inbuf + iod->width;
		d_rm->outbuf_pos = d_rm->outbuf = d_rm->outbuf_start + (d_rm->largerecord && FAB$C_VAR == d_rm->b_rfm
							? SIZEOF(uint4) : 0);
		d_rm->outbuf_top = d_rm->outbuf + iod->width;
		d_rm->promask = xabpro.xab$w_pro;
		iod->state = dev_open;
	}
	return TRUE;
}
