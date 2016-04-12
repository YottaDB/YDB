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

#include "gtm_string.h"

#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "movtc.h"

static readonly char vol1_lab[] = "VOL1MUMPS1                                                                     3";
static readonly char hdr1_lab[] = "HDR1MUMPS.SRC        MUMPS100010001000100 00000 00000 000000GTCMUMPS            ";
static readonly char hdr2_lab[] = "                                   00                            ";
static readonly char eof1_lab[] = "EOF1MUMPS.SRC        MUMPS100010001000100 00000 00000 000000GTCMUMPS            ";

LITREF unsigned char LIB_AB_ASC_EBC[];
LITREF unsigned char LIB_AB_EBC_ASC[];

void iomt_wtansilab(io_desc *dv, uint4 labs)
{
	iosb		io_status_blk;
	uint4	status, status1, mask;
	unsigned char	*outcp, buff[ANSI_LAB_LENGTH], *ptr, asc_buf[12];
	d_mt_struct     *mt_ptr;
	error_def(ERR_MTIS);

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	mask = 0;
#ifdef UNIX
	if (mt_ptr->mode != MT_M_WRITE)
	{
		uint4  status;
		status = iomt_reopen (dv, MT_M_WRITE, FALSE);
	}
#endif
	if (labs & MTL_VOL1)
	{	outcp = (unsigned char*) vol1_lab;
		if (mt_ptr->ebcdic)
		{	movtc(ANSI_LAB_LENGTH, outcp, LIB_AB_ASC_EBC, buff);
			status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						buff, ANSI_LAB_LENGTH);
		}
		else
		{	status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						outcp, ANSI_LAB_LENGTH);
		}
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if ( status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;
	}
	if (labs & MTL_HDR1)
	{	outcp = (unsigned char *) hdr1_lab;
		mask = 0;
		if (mt_ptr->ebcdic)
		{	movtc(ANSI_LAB_LENGTH, outcp, LIB_AB_ASC_EBC, buff);
			status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						buff, ANSI_LAB_LENGTH);
		}
		else
		{	status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						outcp, ANSI_LAB_LENGTH);
		}
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if ( status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;
	}
	if (labs & MTL_HDR2)
	{	outcp = buff;
		memcpy(outcp, mt_ptr->fixed ? "HDR2F" : "HDR2D" , 5);
		*(outcp+5) = '0';
		i2asc(asc_buf,mt_ptr->block_sz);
		*(outcp+6) = asc_buf[0];
		*(outcp+7) = asc_buf[1];
		*(outcp+8) = asc_buf[2];
		*(outcp+9) = asc_buf[3];
		*(outcp+10) = '0';
		i2asc(asc_buf,mt_ptr->record_sz);
		*(outcp+11) = asc_buf[0];
		*(outcp+12) = asc_buf[1];
		*(outcp+13) = asc_buf[2];
		*(outcp+14) = asc_buf[3];
		memcpy(outcp+15, hdr2_lab, SIZEOF(hdr2_lab) - 1);
		mask = 0;
		if (mt_ptr->ebcdic)
		{	movtc(ANSI_LAB_LENGTH, outcp, LIB_AB_ASC_EBC, buff);
			status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						buff, ANSI_LAB_LENGTH);
		}
		else
		{	status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						outcp, ANSI_LAB_LENGTH);
		}
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if ( status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;
	}
	if (labs & MTL_EOF1)
	{	outcp = (unsigned char *) eof1_lab;
		mask = 0;
		if (mt_ptr->ebcdic)
		{	movtc(ANSI_LAB_LENGTH, outcp, LIB_AB_ASC_EBC, buff);
			status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						buff, ANSI_LAB_LENGTH);
		}
		else
		{	status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						outcp, ANSI_LAB_LENGTH);
		}
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if (status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;
	}
	if (labs & MTL_EOF2)
	{	outcp = buff;
		memcpy(outcp, mt_ptr->fixed ? "EOF2F" : "EOF2D" , 5);
		*(outcp+5) = '0';
		i2asc(asc_buf,mt_ptr->block_sz);
		*(outcp+6) = asc_buf[0];
		*(outcp+7) = asc_buf[1];
		*(outcp+8) = asc_buf[2];
		*(outcp+9) = asc_buf[3];
		*(outcp+10) = '0';
		i2asc(asc_buf,mt_ptr->record_sz);
		*(outcp+11) = asc_buf[0];
		*(outcp+12) = asc_buf[1];
		*(outcp+13) = asc_buf[2];
		*(outcp+14) = asc_buf[3];
		memcpy(outcp+15, hdr2_lab, SIZEOF(hdr2_lab) - 1);
		if (mt_ptr->ebcdic)
		{	movtc(ANSI_LAB_LENGTH, outcp, LIB_AB_ASC_EBC, buff);
			status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						buff, ANSI_LAB_LENGTH);
		}
		else
		{	status = iomt_wtlblk(mt_ptr->access_id, mask, &io_status_blk,
						outcp, ANSI_LAB_LENGTH);
		}
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if (status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;
	}
	return;
}

void iomt_rdansistart(io_desc *dv)
{
	uint4	status, status1, mask;
	iosb		io_status_blk;
	d_mt_struct     *mt_ptr;
	int		inlen, i;
	unsigned char	*incp, *ptr;
	error_def (ERR_MTIS);
	error_def(ERR_MTANSILAB);

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	inlen = ANSI_LAB_LENGTH;
	incp = (unsigned char*) malloc(mt_ptr->block_sz);
	for (i = 0; i < 4; i++)
	{	io_status_blk.status = 0;
		mask = 0;
		status = iomt_rdlblk(mt_ptr, mask, &io_status_blk,
				incp, mt_ptr->block_sz);
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if ( status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			free(incp);
			if (status == SS_NORMAL && status1 == SS_ENDOFFILE)
				return;
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;

		if (mt_ptr->ebcdic)
			movtc(ANSI_LAB_LENGTH, incp, LIB_AB_EBC_ASC, incp);

		switch(i)
		{
		case 0:
			if (io_status_blk.char_ct != ANSI_LAB_LENGTH || memcmp(incp, vol1_lab, 4))
			{
				dv->dollar.za = 9;
				free (incp);
				rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len,
					dv->trans_name->dollar_io);
			}
			break;
		case 1:
			if (io_status_blk.char_ct != ANSI_LAB_LENGTH || memcmp(incp, hdr1_lab, 4))
			{
				dv->dollar.za = 9;
				free (incp);
				rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len,
					dv->trans_name->dollar_io);
			}
			break;
		case 2:
			if (!(io_status_blk.char_ct != ANSI_LAB_LENGTH || memcmp(incp, "HDR2", 4)))
			{	ptr = incp + 4;
				if (*ptr == 'D' || *ptr == 'F')
				{	break;
				}
			}
			dv->dollar.za = 9;
			free (incp);
			rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		default:
			dv->dollar.za = 9;
			free (incp);
			rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
	}
}

void iomt_rdansiend(io_desc *dv)
{
	uint4	status, status1, mask;
	iosb		io_status_blk;
	int		inlen, i;
	d_mt_struct     *mt_ptr;
	unsigned char	*incp, *ptr;
	error_def (ERR_MTIS);
	error_def(ERR_MTANSILAB);

	mt_ptr = (d_mt_struct *) dv->dev_sp;
	inlen = ANSI_LAB_LENGTH;
	incp = (unsigned char *) malloc(mt_ptr->block_sz);
	for (i = 0; i < 3; i++)
	{	io_status_blk.status = 0;
		mask = 0;
		status = iomt_rdlblk(mt_ptr, mask, &io_status_blk,
				incp, mt_ptr->block_sz);
		if (status != SS_NORMAL || (status1 = io_status_blk.status) != SS_NORMAL)
		{	if (status1 == SS_ENDOFTAPE)
			{
				dv->dollar.za = 1;
			}
			else
			{
				dv->dollar.za = 9;
			}
			free (incp);
			if (status == SS_NORMAL && (status1 == SS_ENDOFFILE))
				return;
			rts_error(VARLSTCNT(4) ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
		else
			dv->dollar.za = 0;

		if (mt_ptr->ebcdic)
			movtc(ANSI_LAB_LENGTH, incp, LIB_AB_EBC_ASC, incp);

		switch(i)
		{
		case 0:
			if (io_status_blk.char_ct != ANSI_LAB_LENGTH || memcmp(incp, eof1_lab, 4))
			{
				dv->dollar.za = 9;
				free (incp);
				rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len,
					dv->trans_name->dollar_io);
			}
			break;
		case 1:
			if (!(io_status_blk.char_ct != ANSI_LAB_LENGTH || memcmp(incp, "EOF2", 4)))
			{	ptr = incp + 4;
				if (*ptr == 'D' || *ptr == 'F')
				{	break;
				}
			}
			dv->dollar.za = 9;
			free (incp);
			rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		default:
			dv->dollar.za = 9;
			free (incp);
			rts_error(VARLSTCNT(6) ERR_MTANSILAB, 0, ERR_MTIS, 2, dv->trans_name->len, dv->trans_name->dollar_io);
		}
	}
	free (incp);
	return;
}
