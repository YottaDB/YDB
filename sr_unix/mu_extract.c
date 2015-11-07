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

#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_iconv.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "stp_parms.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "cli.h"
#include "io.h"
#include "iosp.h"
#include "gtmio.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mupip_exit.h"
#include "is_raw_dev.h"
#include "gv_select.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "mvalconv.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"
#include "filestruct.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */

GBLREF	int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF	bool		mu_ctrlc_occurred;
GBLREF	bool		mu_ctrly_occurred;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gd_addr		*gd_header;
GBLREF	io_pair          io_curr_device;
GBLREF	io_desc          *active_device;
GBLREF	gv_namehead	*gv_target;
GBLREF	boolean_t	jnlpool_init_needed;
GBLREF	mstr		sys_output;

error_def(ERR_EXTRACTCTRLY);
error_def(ERR_EXTRACTFILERR);
error_def(ERR_GTMASSERT);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOSELECT);
error_def(ERR_NULLCOLLDIFF);
error_def(ERR_RECORDSTAT);
error_def(ERR_EXTRFILEXISTS);

LITDEF mval	mu_bin_datefmt	= DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(BIN_HEADER_DATEFMT) - 1,
						      BIN_HEADER_DATEFMT, 0, 0);

LITREF mstr		chset_names[];

static readonly unsigned char	datefmt_txt[] = "DD-MON-YEAR  24:60:SS";
static readonly unsigned char	select_text[] = "SELECT";
static readonly mval		datefmt = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, SIZEOF(datefmt_txt) - 1, (char *)datefmt_txt, 0, 0);
static readonly mval		null_str = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, 0, 0, 0);
static char			outfilename[256];
static unsigned short		filename_len;
static unsigned char		ochset_set = FALSE;
static readonly unsigned char	open_params_list[] =
{
	(unsigned char)iop_recordsize,		/* 64K -1 - big enough for MAX_BLK_SZ */
#	ifdef BIGENDIAN
	(unsigned char)0, (unsigned char)0, (unsigned char)255, (unsigned char)255,
#	else
	(unsigned char)255, (unsigned char)255, (unsigned char)0, (unsigned char)0,
#	endif
	(unsigned char)iop_noreadonly,
	(unsigned char)iop_m,
	(unsigned char)iop_nowrap,
	(unsigned char)iop_stream,
	(unsigned char)iop_eol
};
static readonly unsigned char no_param = (unsigned char)iop_eol;

#define BINARY_FORMAT_STRING	"BINARY"
#define ZWR_FORMAT_STRING	"ZWR"
#define GO_FORMAT_STRING	"GO"

#define	WRITE_NUMERIC(nmfield)						\
{									\
	MV_FORCE_MVAL(&val, nmfield);					\
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

#define GET_BIN_HEADER_SIZE(LABEL) (SIZEOF(LABEL) + SIZEOF(BIN_HEADER_DATEFMT) - 1 + 4 * BIN_HEADER_NUMSZ + BIN_HEADER_LABELSZ)
CONDITION_HANDLER(mu_extract_handler)
{
	mval				op_val, op_pars;
	unsigned char			delete_params[2] = { (unsigned char)iop_delete, (unsigned char)iop_eol };

	START_CH;
	op_val.mvtype = op_pars.mvtype = MV_STR;
	op_val.str.addr = (char *)outfilename;
	op_val.str.len = filename_len;
	op_pars.str.len = SIZEOF(delete_params);
	op_pars.str.addr = (char *)delete_params;
	op_close(&op_val, &op_pars);

	util_out_print("!/MUPIP is not able to create extract file !AD due to the above error!/",
			TRUE, filename_len, outfilename);
	NEXTCH;
}

CONDITION_HANDLER(mu_extract_handler1)
{
	START_CH;
	util_out_print("!/MUPIP is not able to complete the extract due the the above error!/", TRUE);
	util_out_print("!/WARNING!!!!!! Extract file !AD is incomplete!!!/",
			TRUE, filename_len, outfilename);
	NEXTCH;
}

void mu_extract(void)
{
	int 				stat_res, truncate_res;
	int				reg_max_rec, reg_max_key, reg_max_blk, reg_std_null_coll;
	int				iter, format, local_errno, int_nlen;
	boolean_t			freeze = FALSE, logqualifier, success;
	char				format_buffer[FORMAT_STR_MAX_SIZE],  ch_set_name[MAX_CHSET_NAME], cli_buff[MAX_LINE],
					label_buff[LABEL_STR_MAX_SIZE], gbl_name_buff[MAX_MIDENT_LEN + 2]; /* 2 for null and '^' */
	glist				gl_head, *gl_ptr;
	gd_region			*reg, *region_top;
	mu_extr_stats			global_total, grand_total;
	uint4				item_code, devbufsiz, maxfield;
	unsigned short			label_len, n_len, ch_set_len, buflen;
	unsigned char			*outbuf, *outptr, *chptr, *leadptr;
	struct stat                     statbuf;
	mval				val, curr_gbl_name, op_val, op_pars;
	mstr				chset_mstr;
	gtm_chset_t 			saved_out_set;
	coll_hdr			extr_collhdr;
	int				bin_header_size;
	int	 			reg_no;
	boolean_t			is_any_file_encrypted = FALSE;
#	ifdef GTM_CRYPT
	unsigned short			hash_buff_len;
	sgmnt_data_ptr_t		csd;
	sgmnt_addrs			*csa;
	muext_hash_hdr_ptr_t		hash_array;
#	endif

	/* Initialize all local character arrays to zero before using */
	memset(cli_buff, 0, SIZEOF(cli_buff));
	memset(outfilename, 0, SIZEOF(outfilename));
	memset(label_buff, 0, SIZEOF(label_buff));
	memset(format_buffer, 0, SIZEOF(format_buffer));
	active_device = io_curr_device.out;
	mu_outofband_setup();
	if (CLI_PRESENT == cli_present("OCHSET"))
	{
		ch_set_len = SIZEOF(ch_set_name);
		if (cli_get_str("OCHSET", ch_set_name, &ch_set_len))
		{
			if (0 == ch_set_len)
				mupip_exit(ERR_MUNOACTION);	/* need to change to OPCHSET error when added */
			ch_set_name[ch_set_len] = '\0';
#			ifdef KEEP_zOS_EBCDIC
   			if ( (iconv_t)0 != active_device->output_conv_cd)
   			        ICONV_CLOSE_CD(active_device->output_conv_cd);
   			if (DEFAULT_CODE_SET != active_device->out_code_set)
   				ICONV_OPEN_CD(active_device->output_conv_cd, INSIDE_CH_SET, ch_set_name);
#			else
			chset_mstr.addr = ch_set_name;
			chset_mstr.len = ch_set_len;
			SET_ENCODING(active_device->ochset, &chset_mstr);
			get_chset_desc(&chset_names[active_device->ochset]);
#			endif
			ochset_set = TRUE;
		}
	}
	logqualifier = (CLI_NEGATED != cli_present("LOG"));
	if (CLI_PRESENT == cli_present("FREEZE"))
		freeze = TRUE;
	n_len = SIZEOF(format_buffer);
	if (FALSE == cli_get_str("FORMAT", format_buffer, &n_len))
	{
		n_len = SIZEOF(ZWR_FORMAT_STRING) - 1;
		memcpy(format_buffer, ZWR_FORMAT_STRING, n_len);
	}
	int_nlen = n_len;
	lower_to_upper((uchar_ptr_t)format_buffer, (uchar_ptr_t)format_buffer, int_nlen);
	if (0 == memcmp(format_buffer, ZWR_FORMAT_STRING, n_len))
	        format = MU_FMT_ZWR;
	else if (0 == memcmp(format_buffer, GO_FORMAT_STRING, n_len))
	{
		if (gtm_utf8_mode)
		{
			util_out_print("Extract error: GO format is not supported in UTF-8 mode. Use ZWR format.", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		format = MU_FMT_GO;
	} else if (0 == memcmp(format_buffer, BINARY_FORMAT_STRING, n_len))
		format = MU_FMT_BINARY;
	else
	{
		util_out_print("Extract error: bad format type", TRUE);
		mupip_exit(ERR_MUPCLIERR);
	}
	n_len = SIZEOF(cli_buff);
	if (FALSE == cli_get_str((char *)select_text, cli_buff, &n_len))
	{
		n_len = 1;
		cli_buff[0] = '*';
	}
	/* gv_select will select globals */
	jnlpool_init_needed = TRUE;
        gv_select(cli_buff, n_len, freeze, (char *)select_text, &gl_head, &reg_max_rec, &reg_max_key, &reg_max_blk, FALSE);
 	if (!gl_head.next)
        {
                rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSELECT);
                mupip_exit(ERR_NOSELECT);
        }
	/* For binary format, check whether all regions have same null collation order */
	if (MU_FMT_BINARY == format)
	{
#		ifdef GTM_CRYPT
		hash_buff_len = (SIZEOF(muext_hash_hdr) * gd_header->n_regions);
		hash_array = (muext_hash_hdr *)malloc(hash_buff_len);
		memset(hash_array, 0, hash_buff_len);
#		endif
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions,
				reg_std_null_coll = -1, reg_no = 0;
					reg < region_top ; reg++, reg_no++)
		{
			if (reg->open)
			{
				if (reg_std_null_coll != reg->std_null_coll)
				{
					if (reg_std_null_coll == -1)
						reg_std_null_coll = reg->std_null_coll;
					else
					{
						rts_error_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(1) ERR_NULLCOLLDIFF);
						mupip_exit(ERR_NULLCOLLDIFF);
					}
				}
#				ifdef GTM_CRYPT
				csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
				csd = csa->hdr;
				memcpy(hash_array[reg_no].gtmcrypt_hash, csd->encryption_hash, GTMCRYPT_HASH_LEN);
				if (csd->is_encrypted)
					is_any_file_encrypted = TRUE;
#				endif
			}
		}
		assert(-1 != reg_std_null_coll);
	}
	grand_total.recknt = grand_total.reclen = grand_total.keylen = grand_total.datalen = 0;
	global_total.recknt = global_total.reclen = global_total.keylen = global_total.datalen = 0;
	n_len = SIZEOF(outfilename);
	if (CLI_PRESENT == cli_present("STDOUT"))
		/* Redirect to standard output */
		op_val.str = sys_output;
	else if (FALSE == cli_get_str("FILE", outfilename, &n_len))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		mupip_exit(ERR_MUPCLIERR);
	} else if (-1 == Stat((char *)outfilename, &statbuf))
	{	/* Redirect to file */
		if (ENOENT != errno)
		{
			local_errno = errno;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_EXTRACTFILERR, 2, LEN_AND_STR(outfilename), local_errno);
			mupip_exit(local_errno);
		}
		op_val.str.len = filename_len = n_len;
		op_val.str.addr = (char *)outfilename;
	} else
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_EXTRFILEXISTS, 2, LEN_AND_STR(outfilename));
		mupip_exit(ERR_MUNOACTION);
	}
	op_pars.mvtype = MV_STR;
	op_pars.str.len = SIZEOF(open_params_list);
	op_pars.str.addr = (char *)open_params_list;
	op_val.mvtype = MV_STR;
	(*op_open_ptr)(&op_val, &op_pars, 0, 0);
	ESTABLISH(mu_extract_handler);
	op_pars.str.len = SIZEOF(no_param);
	op_pars.str.addr = (char *)&no_param;
	op_use(&op_val, &op_pars);
	if (MU_FMT_BINARY == format)
	{	/* binary header label format:
		 * fixed length text, fixed length date & time,
		 * fixed length max blk size, fixed length max rec size, fixed length max key size, fixed length std_null_coll
		 * 32-byte padded user-supplied string
		 */
		outbuf = (unsigned char *)malloc(SIZEOF(BIN_HEADER_LABEL) + SIZEOF(BIN_HEADER_DATEFMT) - 1 +
				4 * BIN_HEADER_NUMSZ + BIN_HEADER_LABELSZ);
		outptr = outbuf;
		if (is_any_file_encrypted)
		{
			MEMCPY_LIT(outptr, BIN_HEADER_LABEL_ENCR);
			outptr += STR_LIT_LEN(BIN_HEADER_LABEL_ENCR);
		} else
		{
			MEMCPY_LIT(outptr, BIN_HEADER_LABEL);
			outptr += STR_LIT_LEN(BIN_HEADER_LABEL);
		}
		op_horolog(&val);
		op_fnzdate(&val, (mval *)&mu_bin_datefmt, &null_str, &null_str, &val);
		memcpy(outptr, val.str.addr, val.str.len);
		outptr += val.str.len;
		WRITE_NUMERIC(reg_max_blk);
		WRITE_NUMERIC(reg_max_rec);
		WRITE_NUMERIC(reg_max_key);
		WRITE_NUMERIC(reg_std_null_coll);
		if (gtm_utf8_mode)
		{
			MEMCPY_LIT(outptr, UTF8_NAME);
			label_len = STR_LIT_LEN(UTF8_NAME);
			outptr[label_len++] = ' ';
		} else
			label_len = 0;
		buflen = SIZEOF(label_buff);
		if (FALSE == cli_get_str("LABEL", label_buff, &buflen))
		{
			MEMCPY_LIT(&outptr[label_len], EXTR_DEFAULT_LABEL);
			buflen = STR_LIT_LEN(EXTR_DEFAULT_LABEL);
		} else
			memcpy(&outptr[label_len], label_buff, buflen);
		label_len += buflen;
		if (label_len > BIN_HEADER_LABELSZ)
		{	/* Label size exceeds the space, so truncate the label and back off to the valid beginning
			 * (i.e. to the leading byte) of the last character that can entirely fit in the space
			 */
			label_len = BIN_HEADER_LABELSZ;
			chptr = &outptr[BIN_HEADER_LABELSZ];
			UTF8_LEADING_BYTE(chptr, outptr, leadptr);
			assert(chptr - leadptr < 4);
			if (leadptr < chptr)
				label_len -= (chptr - leadptr);
		}
		outptr += label_len;
		for (iter = label_len;  iter < BIN_HEADER_LABELSZ;  iter++)
			*outptr++ = ' ';
		label_len = outptr - outbuf;
		if (!ochset_set)
		{
#			ifdef KEEP_zOS_EBCDIC
			/* extract ascii header for binary by default */
			/* Do we need to restore it somewhere? */
			saved_out_set = (io_curr_device.out)->out_code_set;
			(io_curr_device.out)->out_code_set = DEFAULT_CODE_SET;
#			else
			saved_out_set = (io_curr_device.out)->ochset;
			(io_curr_device.out)->ochset = CHSET_M;
#			endif
		}
		op_val.str.addr = (char *)(&label_len);
		op_val.str.len = SIZEOF(label_len);
		op_write(&op_val);
		op_val.str.addr = (char *)outbuf;
		op_val.str.len = label_len;
		op_write(&op_val);
#		ifdef GTM_CRYPT
		if (is_any_file_encrypted)
		{
			op_val.str.addr = (char *)(&hash_buff_len);
			op_val.str.len = SIZEOF(hash_buff_len);
			op_write(&op_val);
			op_val.str.addr = (char *)hash_array;
			op_val.str.len = hash_buff_len;
			op_write(&op_val);
		}
#		endif
	} else
	{
		assert((MU_FMT_GO == format) || (MU_FMT_ZWR == format));
		label_len = SIZEOF(label_buff);
		if (FALSE == cli_get_str("LABEL", label_buff, &label_len))
		{
			MEMCPY_LIT(label_buff, EXTR_DEFAULT_LABEL);
			label_len = STR_LIT_LEN(EXTR_DEFAULT_LABEL);
		}
		if (gtm_utf8_mode)
		{
			label_buff[label_len++] = ' ';
			MEMCPY_LIT(&label_buff[label_len], UTF8_NAME);
			label_len += STR_LIT_LEN(UTF8_NAME);
		}
		label_buff[label_len++] = '\n';
		op_val.mvtype = MV_STR;
		op_val.str.len = label_len;
		op_val.str.addr = label_buff;
		op_write(&op_val);
		op_horolog(&val);
		op_fnzdate(&val, &datefmt, &null_str, &null_str, &val);
		op_val = val;
		op_val.mvtype = MV_STR;
		op_write(&op_val);
		if (MU_FMT_ZWR == format)
		{
			op_val.str.addr = " ZWR";
			op_val.str.len = SIZEOF(" ZWR") - 1;
			op_write(&op_val);
		}
		op_wteol(1);
	}
	REVERT;
	ESTABLISH(mu_extract_handler1);
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
		GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &gl_ptr->name.str);
		if (MU_FMT_BINARY == format)
		{
			label_len = SIZEOF(extr_collhdr);
			op_val.mvtype = MV_STR;
			op_val.str.addr = (char *)(&label_len);
			op_val.str.len = SIZEOF(label_len);
			op_write(&op_val);
			extr_collhdr.act = gv_target->act;
			extr_collhdr.nct = gv_target->nct;
			extr_collhdr.ver = gv_target->ver;
			op_val.str.addr = (char *)(&extr_collhdr);
			op_val.str.len = SIZEOF(extr_collhdr);
			op_write(&op_val);
		}
#		ifdef GTM_CRYPT
		success = mu_extr_gblout(&gl_ptr->name,
					 &global_total,
					 format,
					 hash_array,
					 is_any_file_encrypted) && success;
#		else
		/* Note: Do not change the order of the expression below.
		 * Otherwise if success is FALSE, mu_extr_gblout() will not be called at all.
		 * We want mu_extr_gblout() to be called irrespective of the value of success */
		success = mu_extr_gblout(&gl_ptr->name, &global_total, format) && success;
#		endif
		if (logqualifier)
		{
			gbl_name_buff[0]='^';
			memcpy(&gbl_name_buff[1], gl_ptr->name.str.addr, gl_ptr->name.str.len);
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_RECORDSTAT, 6, gl_ptr->name.str.len + 1, gbl_name_buff,
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
	assert((MV_STR == op_val.mvtype) && (MV_STR == op_pars.mvtype));
	op_val.str.addr = (char *)outfilename;;
	op_val.str.len = filename_len;
	op_pars.str.len = SIZEOF(no_param);
	op_pars.str.addr = (char *)&no_param;
	op_close(&op_val, &op_pars);
	REVERT;
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_EXTRACTCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RECORDSTAT, 6, LEN_AND_LIT("TOTAL"),
		grand_total.recknt, grand_total.keylen, grand_total.datalen, grand_total.reclen);
		if (MU_FMT_BINARY == format)
	{	/*      truncate the last newline charactor flushed by op_close */
		GTMCRYPT_ONLY(
			if (hash_array)
				free(hash_array);
		)
	}
	mupip_exit(success ? SS_NORMAL : ERR_MUNOFINISH);
}
