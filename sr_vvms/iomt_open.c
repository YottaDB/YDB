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

#include "gtm_string.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "io_params.h"
#include "nametabtyp.h"
#include "toktyp.h"
#include "stringpool.h"
#include "namelook.h"
#include "copy.h"

LITDEF unsigned char LIB_AB_ASC_EBC[256] =
{
	0, 1, 2, 3, 55, 45, 46, 47, 22, 5, 37, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30,
	31, 64, 79, 127, 123, 91, 108, 80, 125, 77, 93, 92, 78, 107, 96, 75,
	97, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 122, 94, 76, 126, 110,
	111, 124, 193, 194, 195, 196, 197, 198, 199, 200, 201, 209, 210, 211, 212, 213,
	214, 215, 216, 217, 226, 227, 228, 229, 230, 231, 232, 233, 74, 224, 90, 95,
	109, 121, 129, 130, 131, 132, 133, 134, 135, 136, 137, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 162, 163, 164, 165, 166, 167, 168, 169, 192, 106, 208, 161,
	7, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 255
};
LITDEF unsigned char LIB_AB_EBC_ASC[256] =
{
	0, 1, 2, 3, 92, 9, 92, 127, 92, 92, 92, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 92, 92, 8, 92, 24, 25, 92, 92, 28, 29, 30,
	31, 92, 92, 92, 92, 92, 10, 23, 27, 92, 92, 92, 92, 92, 5, 6,
	7, 92, 92, 22, 92, 92, 92, 92, 4, 92, 92, 92, 92, 20, 21, 92,
	26, 32, 92, 92, 92, 92, 92, 92, 92, 92, 92, 91, 46, 60, 40, 43,
	33, 38, 92, 92, 92, 92, 92, 92, 92, 92, 92, 93, 36, 42, 41, 59,
	94, 45, 47, 92, 92, 92, 92, 92, 92, 92, 92, 124, 44, 37, 95, 62,
	63, 92, 92, 92, 92, 92, 92, 92, 92, 92, 96, 58, 35, 64, 39, 61,
	34, 92, 97, 98, 99, 100, 101, 102, 103, 104, 105, 92, 92, 92, 92, 92,
	92, 92, 106, 107, 108, 109, 110, 111, 112, 113, 114, 92, 92, 92, 92, 92,
	92, 92, 126, 115, 116, 117, 118, 119, 120, 121, 122, 92, 92, 92, 92, 92,
	92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
	92, 123, 65, 66, 67, 68, 69, 70, 71, 72, 73, 92, 92, 92, 92, 92,
	92, 125, 74, 75, 76, 77, 78, 79, 80, 81, 82, 92, 92, 92, 92, 92,
	92, 92, 92, 83, 84, 85, 86, 87, 88, 89, 90, 92, 92, 92, 92, 92,
	92, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 92, 92, 92, 92, 92, 255
};
LITREF unsigned char io_params_size[];

static readonly nametabent mtlab_names[] =
{
	 {3, "ANS"}, {4,"ANSI"}, { 3, "DOS"}, { 5, "DOS11"}
};
static readonly unsigned char mtlab_index[27] =
{
	0, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4
};
static readonly char mtlab_type[]={ MTLAB_ANSI, MTLAB_ANSI, MTLAB_DOS11, MTLAB_DOS11  };

error_def(ERR_DEVPARMNEG);
error_def(ERR_MTRECTOOBIG);
error_def(ERR_MTRECGTRBLK);
error_def(ERR_MTBLKTOOBIG);
error_def(ERR_MTBLKTOOSM);
error_def(ERR_MTFIXRECSZ);
error_def(ERR_MTRECTOOSM);
error_def(ERR_MTINVLAB);
error_def(ERR_MTDOSFOR);
error_def(ERR_MTANSIFOR);
error_def(ERR_MTIS);
error_def(ERR_VARRECBLKSZ);

#define VREC_HDR_LEN	4

short iomt_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	bool		do_rewind, do_erase;
	int		lab_type;
	unsigned char	*buff, ch, len;
	int4		length, blocksize, recordsize;
	uint4		status;
	d_mt_struct	*mt, newmt;
	iosb		io_status_blk;
	io_desc		*ioptr;
	char		*tab;
	int		p_offset;

	ioptr = dev_name->iod;
	buff = 0;
	memset(&newmt, 0, SIZEOF(newmt));		/* zero structure to start */
	if (ioptr->state == dev_never_opened)
		ioptr->dev_sp =(void *)(malloc(SIZEOF(d_mt_struct)));
	mt = (d_mt_struct *)dev_name->iod->dev_sp;
	if (ioptr->state == dev_open && mt->buffer)
	{
		if (mt->bufftoggle < 0)
			buff = (mt->buffer + mt->bufftoggle);
		else
			buff = (mt->buffer);
	}
	do_rewind = do_erase = FALSE;
	if (ioptr->state == dev_never_opened)
	{
		length = DEF_MT_LENGTH;
		newmt.read_mask = IO_READLBLK;
		newmt.write_mask = IO_WRITELBLK;
		newmt.block_sz = MTDEF_BUF_SZ;
		newmt.record_sz = MTDEF_REC_SZ;
		newmt.ebcdic = FALSE;
		newmt.labeled = FALSE;
		newmt.fixed = FALSE;
		newmt.stream = FALSE;
		newmt.read_only = FALSE;
		newmt.newversion = FALSE;
		newmt.last_op = mt_null;
		newmt.wrap = TRUE;
	}
	else
	{
		length = ioptr->length;
		newmt = *mt;
	}
	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (ch = *(pp->str.addr + p_offset++))
		{
		case iop_blocksize:
			GET_LONG(blocksize, pp->str.addr + p_offset);
			if (blocksize < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			else if (blocksize > MAX_BLK_SZ)
				rts_error(VARLSTCNT(1) ERR_MTBLKTOOBIG);
			else if (blocksize < MIN_BLK_SZ)
				rts_error(VARLSTCNT(3) ERR_MTBLKTOOSM, 1, MIN_BLK_SZ);
			newmt.block_sz = blocksize;
			break;
		case iop_recordsize:
			GET_LONG(recordsize, (pp->str.addr + p_offset));
			if (recordsize < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			if (recordsize > MAX_REC_SZ)
				rts_error(VARLSTCNT(1) ERR_MTRECTOOBIG);
			newmt.record_sz = recordsize;

			break;
		case iop_rewind:
			do_rewind = TRUE;
			break;
		case iop_erasetape:
			do_erase = TRUE;
			break;
		case iop_newversion:
			newmt.newversion = TRUE;
			break;
		case iop_readonly:
			newmt.read_only = TRUE;
			break;
		case iop_noreadonly:
			newmt.read_only = FALSE;
			break;
		case iop_ebcdic:
			newmt.ebcdic = TRUE;
			break;
		case iop_noebcdic:
			newmt.ebcdic = FALSE;
			break;
		case iop_nolabel:
			newmt.labeled = FALSE;
			break;
		case iop_label:
			len = *(pp->str.addr + p_offset);
			tab = pp->str.addr + p_offset + 1;
			if ((lab_type = namelook(mtlab_index, mtlab_names, tab, len)) < 0)
			{
				rts_error(VARLSTCNT(1) ERR_MTINVLAB);
				return FALSE;
			}
			newmt.labeled = mtlab_type[lab_type];
			break;
		case iop_fixed:
			newmt.fixed = TRUE;
			break;
		case iop_nofixed:
			newmt.fixed = FALSE;
			break;
		case iop_rdcheckdata:
			newmt.read_mask |= IO_M_DATACHECK;
			break;
		case iop_nordcheckdata:
			newmt.read_mask &= (~(IO_M_DATACHECK));
			break;
		case iop_wtcheckdata:
			newmt.write_mask |= IO_M_DATACHECK;
			break;
		case iop_nowtcheckdata:
			newmt.write_mask &= (~(IO_M_DATACHECK));
			break;
		case iop_inhretry:
			newmt.write_mask |= IO_M_INHRETRY;
			newmt.read_mask |= IO_M_INHRETRY;
			break;
		case iop_retry:
			newmt.write_mask &= ~IO_M_INHRETRY;
			newmt.read_mask &= ~IO_M_INHRETRY;
			break;
		case iop_inhextgap:
			newmt.write_mask |= IO_M_INHEXTGAP;
			break;
		case iop_extgap:
			newmt.write_mask &= ~IO_M_INHEXTGAP;
			break;
		case iop_stream:
			newmt.stream = TRUE;
			break;
		case iop_nostream:
			newmt.stream = FALSE;
			break;
		case iop_wrap:
			newmt.wrap = TRUE;
			break;
		case iop_nowrap:
			newmt.wrap = FALSE;
			break;
		case iop_length:
			GET_LONG(length, pp->str.addr + p_offset);
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			break;
		case iop_exception:
			ioptr->error_handler.len = *(pp->str.addr + p_offset);
			ioptr->error_handler.addr = pp->str.addr + p_offset + 1;
			s2pool(&ioptr->error_handler);
			break;
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	if (newmt.labeled == MTLAB_DOS11)
		if (!newmt.stream)
		{
			rts_error(VARLSTCNT(6) ERR_MTDOSFOR, 0 ,ERR_MTIS, 2,
				ioptr->trans_name->len, ioptr->trans_name->dollar_io);
		}

	if (newmt.labeled == MTLAB_ANSI)
		if (newmt.stream)
		{	rts_error(VARLSTCNT(6) ERR_MTANSIFOR, 0 ,ERR_MTIS, 2,
				ioptr->trans_name->len, ioptr->trans_name->dollar_io);
		}
	if (newmt.stream)
	{	newmt.wrap = TRUE;
		newmt.fixed = FALSE;
	}
	if (newmt.fixed)
	{
		if (newmt.record_sz < MIN_FIXREC_SZ)
			rts_error(VARLSTCNT(1) ERR_MTRECTOOSM);
		if (newmt.block_sz / newmt.record_sz * newmt.record_sz != newmt.block_sz)
			rts_error(VARLSTCNT(4) ERR_MTFIXRECSZ, 2, newmt.block_sz, newmt.record_sz);
	}
	else
	{	if (newmt.record_sz > MAX_VARREC_SZ)
			rts_error(VARLSTCNT(1) ERR_MTRECTOOBIG);
		if (newmt.record_sz < MIN_VARREC_SZ)
			rts_error(VARLSTCNT(1) ERR_MTRECTOOSM);
		if (newmt.block_sz < newmt.record_sz + (newmt.stream ? 2 : VREC_HDR_LEN))
			rts_error(VARLSTCNT(1) ERR_VARRECBLKSZ);
	}
	if (newmt.record_sz > newmt.block_sz)
		rts_error(VARLSTCNT(1) ERR_MTRECGTRBLK);
	if (ioptr->state != dev_open)
	{
		status = iomt_opensp(dev_name, &newmt);
		if (status == MT_BUSY)
			return FALSE;
		if (status == MT_TAPERROR)
			rts_error(VARLSTCNT(1) newmt.access_id);
	}
	if (newmt.fixed)
	{
		newmt.buffer = (unsigned char *)malloc(newmt.block_sz);
		newmt.bufftoggle = 0;
	} else
	{
		newmt.buffer = (unsigned char *)malloc(newmt.block_sz * 2);
		newmt.bufftoggle = newmt.block_sz;
	}
	if (buff)
		free(buff);
	newmt.bufftop = newmt.buffptr = newmt.buffer;
	newmt.last_op = mt_null;
	ioptr->state = dev_open;
	*mt = newmt;
	ioptr->width = newmt.record_sz;
	ioptr->length = length;
	ioptr->dollar.zeof = FALSE;
	ioptr->dollar.x = 0;
	ioptr->dollar.y = 0;
	if (do_erase)
		iomt_erase(ioptr);
	if (do_rewind)
		iomt_rewind(ioptr);
	if (mt->labeled)
	{
		status = iomt_sense(mt, &io_status_blk);
		if (status != SS_NORMAL)
		{
			ioptr->dollar.za = 9;
			rts_error(VARLSTCNT(5)  status,
				ERR_MTIS, 2, ioptr->trans_name->len, ioptr->trans_name->dollar_io);
		}
		if (io_status_blk.dev_dep_info & MT_M_BOT)
			mt->last_op = mt_rewind;
	}
	return TRUE;
}
