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

#include "gtm_string.h"
#include "gtm_iconv.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "cli.h"
#include "filestruct.h"
#include "jnl.h"
#include "io.h"
#include "iosp.h"
#include "util.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "ebc_xlat.h"
#include "t_abort.h"
#include "stringpool.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"

GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region        *gv_cur_region;
GBLREF uint4		update_array_size;
GBLREF gd_addr		*gd_header;
GBLREF srch_hist	dummy_hist;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF block_id		patch_curr_blk;
GBLREF gtm_chset_t	dse_over_chset;
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
GBLREF iconv_t		dse_over_cvtcd;
#endif
GBLREF cw_set_element   cw_set[];
GBLREF UConverter	*chset_desc[];
LITREF mstr		chset_names[];
GBLREF spdesc		stringpool;

void dse_over(void)
{
	static char 	*data = NULL;
	static int	data_size;
        block_id        blk;
	uchar_ptr_t	lbp;
        uint4		offset;
	int		data_len, size;
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size;
        unsigned char   *cvt_src_ptr, *cvt_dst_ptr;
        unsigned int    insize, outsize;
        char            chset_name[MAX_CHSET_NAME];
	mstr		chset_mstr;
	unsigned short	name_len = 0;
	srch_blk_status	blkhist;
	mstr		cvt_src;
	int		cvt_len;

	error_def(ERR_DBRDONLY);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	blk_size = cs_addrs->hdr->blk_size;
        if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.",TRUE);
			return;
		}
		patch_curr_blk = blk;
	} else
		blk = patch_curr_blk;
	if (CLI_PRESENT == cli_present("OCHSET"))
	{
		name_len = SIZEOF(chset_name);
		if (cli_get_str("OCHSET", chset_name, &name_len) && 0 != name_len)
		{
			chset_name[name_len] = 0;
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
			if ( (iconv_t)0 != dse_over_cvtcd )
			{
				ICONV_CLOSE_CD(dse_over_cvtcd);
			}
			if (!strcmp(chset_name, INSIDE_CH_SET))
				dse_over_cvtcd = (iconv_t)0;
			else
				ICONV_OPEN_CD(dse_over_cvtcd, INSIDE_CH_SET, chset_name);
#else
			chset_mstr.addr = chset_name;
			chset_mstr.len = name_len;
			SET_ENCODING(dse_over_chset, &chset_mstr);
			get_chset_desc(&chset_names[dse_over_chset]);
#endif
		}
	} else
	{
#ifdef KEEP_zOS_EBCDIC
		if ((iconv_t)0 != dse_over_cvtcd )
		{
			ICONV_CLOSE_CD(dse_over_cvtcd);
			dse_over_cvtcd = (iconv_t)0;		/* default ASCII, no conversion	*/
		}
#else
		dse_over_chset = CHSET_M;
#endif
	}
	if (cli_present("OFFSET") != CLI_PRESENT)
	{
		util_out_print("Error:  offset must be specified.", TRUE);
		return;
	}
	if (!cli_get_hex("OFFSET", &offset))
		return;
	if (offset < SIZEOF(blk_hdr))
	{
		util_out_print("Error:  offset too small.", TRUE);
		return;
	}
	if (cli_present("DATA") != CLI_PRESENT)
	{
		util_out_print("Error:  data must be specified.", TRUE);
		return;
	}
	t_begin_crit(ERR_DSEFAIL);
	blkhist.blk_num = blk;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);

	size = ((blk_hdr_ptr_t)blkhist.buffaddr)->bsiz;
	if (size < SIZEOF(blk_hdr))
		size = SIZEOF(blk_hdr);
	else if (size >= blk_size)
		size = blk_size;

	if (offset >= size)
	{
		util_out_print("Error:  offset too large.", TRUE);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	if (NULL == data)
	{
		data = malloc(MAX_LINE);
		data_size = MAX_LINE;
	}
	if (FALSE == dse_data(&data[0], &data_len))
	{
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
	cvt_src_ptr = cvt_dst_ptr = (unsigned char *)data;
	insize = outsize = (unsigned int)data_len;
	if ((iconv_t)0 != dse_over_cvtcd)
		ICONVERT(dse_over_cvtcd, &cvt_src_ptr, &insize, &cvt_dst_ptr, &outsize);         /*      in-place conversion     */
#else
	cvt_src.len = (unsigned int)data_len;
	cvt_src.addr = data;
	if (CHSET_M != dse_over_chset)
	{
		cvt_len = gtm_conv(chset_desc[dse_over_chset], chset_desc[CHSET_UTF8], &cvt_src, NULL, NULL);
		if (cvt_len > data_size)
		{
			free(data);
			data = malloc(cvt_len);
			data_size = cvt_len;
		}
		memcpy(data, stringpool.free, cvt_len);
		data_len = cvt_len;
	}
#endif
	if (offset + data_len > size)
	{
		util_out_print("Error:  data will not fit in block at given offset.", TRUE);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	lbp = (uchar_ptr_t)malloc(blk_size);
	memcpy (lbp, blkhist.buffaddr, blk_size);
	memcpy(lbp + offset, &data[0], data_len);

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		free(lbp);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
	t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	return;
}
