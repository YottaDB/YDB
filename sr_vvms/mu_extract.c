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

#include <rms.h>
#include <descrip.h>
#include <ssdef.h>
#include <devdef.h>
#include <dvidef.h>
#include <climsgdef.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "cli.h"
#include "util.h"
#include "op.h"
#include "mupip_exit.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "mvalconv.h"

error_def(ERR_DBRDONLY);
error_def(ERR_EXTRACTCTRLY);
error_def(ERR_EXTRACTFILERR);
error_def(ERR_EXTRCLOSEERR);
error_def(ERR_EXTRFMT);
error_def(ERR_EXTRIOERR);
error_def(ERR_FREEZE);
error_def(ERR_GTMASSERT);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOSELECT);
error_def(ERR_NULLCOLLDIFF);
error_def(ERR_RECORDSTAT);
error_def(ERR_SELECTSYNTAX);

GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF spdesc		stringpool;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_addr		*gd_header;
GBLREF gv_namehead      *gv_target;

#define	RMS_MAX_RECORD_SIZE	((1 << 16) - 1)

#define	WRITE_NUMERIC(nmfield)						\
{									\
	MV_FORCE_MVAL(&val, nmfield);					\
	stringpool.free = stringpool.base;				\
	n2s(&val);							\
	if (val.mvtype & MV_NUM_APPROX)					\
		GTMASSERT;						\
	if (val.str.len > BIN_HEADER_NUMSZ)				\
		GTMASSERT;						\
	for (iter = val.str.len;  iter < BIN_HEADER_NUMSZ;  iter++)	\
		*outptr++ = '0';					\
	memcpy(outptr, val.str.addr, val.str.len);			\
	outptr += val.str.len;						\
}

LITDEF mval	mu_bin_datefmt	= DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(BIN_HEADER_DATEFMT) - 1, BIN_HEADER_DATEFMT, 0, 0);
GBLDEF struct FAB	mu_outfab;
GBLDEF struct RAB 	mu_outrab;

void mu_extract(void)
{
	int				reg_max_rec, reg_max_key, reg_max_blk, max_extract_rec_len, status,len, i, format;
	int				reg_std_null_coll, iter;
	unsigned char                  	cli_buff[MAX_LINE];
	boolean_t			logqualifier, freeze = FALSE, success;
	mval				val;
	char				format_buffer[FORMAT_STR_MAX_SIZE];
	char				gbl_name_buff[MAX_MIDENT_LEN + 2]; /* 2 for null and '^' */
	glist				gl_head, *gl_ptr;
	gd_region			*reg, *region_top;
	mu_extr_stats			global_total,grand_total;
	uint4			        item_code, devbufsiz, maxfield;
	unsigned char			outfilename[256];
	unsigned short			label_len, n_len;
	static readonly unsigned char	datefmt_txt[] = "DD-MON-YEAR  24:60:SS";
	static readonly unsigned char	label_text[] = "LABEL";
	static readonly unsigned char	select_text[] = "SELECT";
	static readonly unsigned char	log_text[] = "LOG";
	static readonly mval		datefmt = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(datefmt_txt) - 1,
							(char *)datefmt_txt, 0, 0);
	static readonly mval		null_str = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, 0, 0, 0);
	unsigned char			*outbuf, *outptr;
	static readonly $DESCRIPTOR(label_str,label_text);
	static readonly $DESCRIPTOR(log_str,log_text);
	struct dsc$descriptor_s label_buff;
	$DESCRIPTOR(dir, "");
	coll_hdr        extr_collhdr;

	mu_outofband_setup();

	logqualifier = (CLI$PRESENT(&log_str) != CLI$_NEGATED);
	if (cli_present("FREEZE") == CLI_PRESENT)
		freeze = TRUE;
	n_len = SIZEOF(format_buffer);
	if (cli_get_str("FORMAT", format_buffer, &n_len) == FALSE)
	{
		n_len = SIZEOF("ZWR") - 1;
		MEMCPY_LIT(format_buffer, "ZWR");
	}
	if (memcmp(format_buffer, "ZWR", n_len) == 0)
		format = MU_FMT_ZWR;
	else if (memcmp(format_buffer, "GO", n_len) == 0)
		format = MU_FMT_GO;
	else if (memcmp(format_buffer, "BINARY", n_len) == 0)
		format = MU_FMT_BINARY;
	else
	{
		util_out_print("Extract error: bad format type",TRUE);
		mupip_exit (ERR_EXTRFMT);
	}

	n_len = 0;
	memset(cli_buff, 0, SIZEOF(cli_buff));
	if (FALSE == CLI_GET_STR_ALL(select_text, cli_buff, &n_len))
	{
		cli_buff[0] = '*';
		n_len = 1;
	}
	grand_total.recknt = grand_total.reclen = grand_total.keylen = grand_total.datalen = 0;
	global_total.recknt = global_total.reclen = global_total.keylen = global_total.datalen = 0;
	/* gv_select will select globals */
	gv_select(cli_buff, n_len, freeze, select_text, &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, FALSE);
	if (!gl_head.next)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSELECT);
		mupip_exit(ERR_MUNOACTION);
	}
	/* For binary format, check whether all regions have same null collation order */
	if (MU_FMT_BINARY == format)
	{
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions, reg_std_null_coll = -1;
			reg < region_top ; reg++)
		{
			if (reg->open)
			{
				if (reg_std_null_coll != reg->std_null_coll)
				{
					if (reg_std_null_coll == -1)
						reg_std_null_coll = reg->std_null_coll;
					else
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NULLCOLLDIFF);
						mupip_exit(ERR_NULLCOLLDIFF);
					}
				}
			}
		}
		assert(-1 != reg_std_null_coll);
	}
	n_len = SIZEOF(outfilename);
	mu_outfab = cc$rms_fab;
	mu_outrab = cc$rms_rab;
	mu_outrab.rab$l_fab = &mu_outfab;
	mu_outrab.rab$l_rop = RAB$M_WBH;
	mu_outfab.fab$l_fna = &outfilename;
	if (cli_get_str("FILE", outfilename, &n_len) == FALSE) /* should be gtmassert */
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		mupip_exit(ERR_MUNOACTION);
	}
	if (format == MU_FMT_BINARY)
		max_extract_rec_len = reg_max_blk;
	else
		max_extract_rec_len = ZWR_EXP_RATIO(reg_max_rec);
	if (max_extract_rec_len > RMS_MAX_RECORD_SIZE)
		max_extract_rec_len = RMS_MAX_RECORD_SIZE;
	mu_outfab.fab$w_mrs = max_extract_rec_len;
	mu_outfab.fab$b_fns = n_len;
	mu_outfab.fab$b_rat = FAB$M_CR;
	mu_outfab.fab$l_fop = FAB$M_CBT | FAB$M_MXV | FAB$M_TEF;  /* contig best try - max version - trunc at close */
	mu_outfab.fab$b_fac = FAB$M_PUT;
	mu_outfab.fab$l_alq = 1000; /* initial allocation */
	mu_outfab.fab$w_deq = 1000; /* def extend quant */

	status = sys$create(&mu_outfab);
	switch (status)
	{
	case RMS$_NORMAL:
	case RMS$_CRE_STM:
	case RMS$_CREATED:
	case RMS$_SUPERSEDE:
	case RMS$_FILEPURGED:
		break;
	default:
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRACTFILERR, 2, mu_outfab.fab$b_fns, mu_outfab.fab$l_fna,
			       status, 0, mu_outfab.fab$l_stv, 0);
		mupip_exit(ERR_MUNOACTION);
	}
	status = sys$connect(&mu_outrab);
	if (status != RMS$_NORMAL)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRACTFILERR, 2, mu_outfab.fab$b_fns, mu_outfab.fab$l_fna,
			       status, 0, mu_outfab.fab$l_stv, 0);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mu_outfab.fab$l_dev & DEV$M_SQD)
	{
		if (format == MU_FMT_BINARY)
			maxfield = reg_max_blk;
		else if (format == MU_FMT_ZWR)
			maxfield = reg_max_rec*7 + reg_max_key + 1;
		else
			maxfield = reg_max_rec > reg_max_key ? reg_max_rec : reg_max_key;
		item_code = DVI$_DEVBUFSIZ;
		dir.dsc$a_pointer = mu_outfab.fab$l_fna;
		dir.dsc$w_length =  n_len;
		devbufsiz = 0;
		lib$getdvi(&item_code, 0, &dir, &devbufsiz, 0, 0);
		if (devbufsiz < maxfield + 8)
		{
			util_out_print("!/Buffer size !UL may not accomodate maximum field size of !UL.",
				FALSE, devbufsiz, maxfield);
			util_out_print("!/8 bytes/tape block overhead required for device.",
				TRUE);
			sys$close(&mu_outfab);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	label_buff.dsc$a_pointer = malloc(128);
	label_buff.dsc$w_length = 128;
	label_buff.dsc$b_dtype = DSC$K_DTYPE_T;
	label_buff.dsc$b_class = DSC$K_CLASS_S;
	if (format == MU_FMT_BINARY)
	{
		/* binary header label format:
		 *	fixed length text, fixed length date & time,
		 *	fixed length max blk size, fixed length max rec size, fixed length max key size,
		 *	fixed length reg_std_null_coll,
		 *	32-byte padded user-supplied string
		 */
		outbuf = malloc(SIZEOF(BIN_HEADER_LABEL) - 1 + SIZEOF(BIN_HEADER_DATEFMT) - 1 + 4 * BIN_HEADER_NUMSZ
				+ BIN_HEADER_LABELSZ);
		outptr = outbuf;
		MEMCPY_LIT(outptr, BIN_HEADER_LABEL);
		outptr += SIZEOF(BIN_HEADER_LABEL) - 1;
		stringpool.free = stringpool.base;
		op_horolog (&val);
		stringpool.free = stringpool.base;
		op_fnzdate (&val, &mu_bin_datefmt, &null_str, &null_str, &val);
		memcpy (outptr, val.str.addr, val.str.len);
		outptr += val.str.len;

		WRITE_NUMERIC(reg_max_blk);
		WRITE_NUMERIC(reg_max_rec);
		WRITE_NUMERIC(reg_max_key);
		WRITE_NUMERIC(reg_std_null_coll);

		CLI$GET_VALUE (&label_str, &label_buff, &label_len);
		memcpy (outptr, label_buff.dsc$a_pointer, BIN_HEADER_LABELSZ);
		if (label_len < BIN_HEADER_LABELSZ)
		{
			outptr += label_len;
			for (i = label_len; i < BIN_HEADER_LABELSZ; i++)
				*outptr++ = ' ';
		}
		else
			outptr += BIN_HEADER_LABELSZ;

		mu_outrab.rab$w_rsz = outptr - outbuf;
		mu_outrab.rab$l_rbf = outbuf;
		status = sys$put (&mu_outrab);
		if (RMS$_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRIOERR, 2, mu_outfab.fab$b_fns, mu_outfab.fab$l_fna,
				       status, 0, mu_outrab.rab$l_stv, 0);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	else
	{
		assert ((MU_FMT_GO == format) || (MU_FMT_ZWR == format));
		CLI$GET_VALUE(&label_str, &label_buff, &mu_outrab.rab$w_rsz);
		mu_outrab.rab$l_rbf = label_buff.dsc$a_pointer;
		status = sys$put(&mu_outrab);
		if (RMS$_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRIOERR, 2, mu_outfab.fab$b_fns, mu_outfab.fab$l_fna,
				       status, 0, mu_outrab.rab$l_stv, 0);
			mupip_exit(ERR_MUNOACTION);
		}
		stringpool.free = stringpool.base;
		op_horolog(&val);
		stringpool.free = stringpool.base;
		op_fnzdate(&val, &datefmt, &null_str, &null_str, &val);
		if (MU_FMT_ZWR == format)
		{
			memcpy(val.str.addr + val.str.len, " ZWR", 4);
			val.str.len += 4;
		}
		mu_outrab.rab$l_rbf = val.str.addr;
		mu_outrab.rab$w_rsz = val.str.len;
		status = sys$put(&mu_outrab);
		if (RMS$_NORMAL != status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRIOERR, 2, mu_outfab.fab$b_fns, mu_outfab.fab$l_fna,
				       status, 0, mu_outrab.rab$l_stv, 0);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	success = TRUE;
	for (gl_ptr = gl_head.next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			gbl_name_buff[0]='^';
			memcpy(&gbl_name_buff[1], gl_ptr->name.str.addr, gl_ptr->name.str.len);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RECORDSTAT, 6, gl_ptr->name.str.len + 1, gbl_name_buff,
				global_total.recknt, global_total.keylen, global_total.datalen, global_total.reclen);
			mu_ctrlc_occurred = FALSE;
		}
		GV_BIND_NAME_AND_ROOT_SEARCH(gd_header,&gl_ptr->name.str);
                if (MU_FMT_BINARY == format)
                {
                       	extr_collhdr.act = gv_target->act;
                        extr_collhdr.nct = gv_target->nct;
                        extr_collhdr.ver = gv_target->ver;
			mu_outrab.rab$l_rbf = (char *)(&extr_collhdr);
			mu_outrab.rab$w_rsz = SIZEOF(extr_collhdr);
			status = sys$put(&mu_outrab);
			if (RMS$_NORMAL != status)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRIOERR, 2,
					mu_outfab.fab$b_fns, mu_outfab.fab$l_fna, status,
					0, mu_outrab.rab$l_stv, 0);
				mupip_exit(ERR_MUNOACTION);
			}
		}
		/* Note: Do not change the order of the expression below.
		 * Otherwise if success is FALSE, mu_extr_gblout() will not be called at all.
		 * We want mu_extr_gblout() to be called irrespective of the value of success */
		success = mu_extr_gblout(&gl_ptr->name, &mu_outrab, &global_total, format) && success;
		if (logqualifier)
		{
			gbl_name_buff[0]='^';
			memcpy(&gbl_name_buff[1], gl_ptr->name.str.addr, gl_ptr->name.str.len);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RECORDSTAT, 6,  gl_ptr->name.str.len + 1, gbl_name_buff,
				global_total.recknt, global_total.keylen, global_total.datalen, global_total.reclen);
			mu_ctrlc_occurred = FALSE;
		}
		grand_total.recknt += global_total.recknt;
		if (grand_total.reclen < global_total.reclen)
			grand_total.reclen = global_total.reclen;
		if (grand_total.keylen < global_total.keylen)
			grand_total.keylen = global_total.keylen;
		if (grand_total.datalen < global_total.datalen)
			grand_total.datalen = global_total.datalen;

	}
	status = sys$close(&mu_outfab);
	if (status != RMS$_NORMAL)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_EXTRCLOSEERR, 2,
			mu_outfab.fab$b_fns, mu_outfab.fab$l_fna, status,
				0, mu_outfab.fab$l_stv, 0);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXTRACTCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RECORDSTAT, 6, LEN_AND_LIT("TOTAL"),
		grand_total.recknt, grand_total.keylen, grand_total.datalen, grand_total.reclen);
	mupip_exit(success ? SS$_NORMAL : ERR_MUNOFINISH);
}
