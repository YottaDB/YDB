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
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "copy.h"
#include "gtm_string.h"
#include "gvcmz.h"

#define FORWARD_REC(x)	buffptr += x->len + SIZEOF(*x) - 1; CM_GET_USHORT(tmp_short, buffptr, usr->convert_byteorder);\
	buffptr += tmp_short + SIZEOF(unsigned short); x = (bunch_rec *)buffptr;

GBLDEF bool		zdefactive;
GBLDEF unsigned short	zdefbufsiz;
GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF spdesc		stringpool;

typedef struct bunch_rec_struct {
	unsigned char	rn;
	unsigned short	len;
	unsigned short	cc;
	unsigned short	prv;
	unsigned char	base[1];
} bunch_rec;

void gvcmz_bunch(mval *v)
{
	struct CLB	*lnk;
	link_info	*usr;
	unsigned char	*buffptr, *bufftop, *insert_record, *msgptr;
	unsigned char	*new_key, *rec_key;
	unsigned short	cc, len, i;
	signed char	is_gt;
	bool		overlay;
	unsigned short	newrec_len, oldrec_len, tmp_short;
	short		new_space;
	bunch_rec 	*brec, nrec;
	int4		status;
	error_def(ERR_ZDEFOFLOW);
	error_def(ERR_BADSRVRNETMSG);

	assert(zdefactive);
	lnk = gv_cur_region->dyn.addr->cm_blk;
	usr = (link_info *)lnk->usr;

	if (zdefbufsiz > usr->buffer_size)
	{
		if (!usr->buffer)
		{
			ENSURE_STP_FREE_SPACE(lnk->mbl);
			msgptr = lnk->mbf = stringpool.free;
		} else
			msgptr = lnk->mbf = usr->buffer;

		*msgptr++ = CMMS_B_BUFRESIZE;
		CM_PUT_USHORT(msgptr, zdefbufsiz, usr->convert_byteorder);
		msgptr += SIZEOF(unsigned short);
		lnk->ast = 0;
		lnk->cbl = S_HDRSIZE + SIZEOF(unsigned short);
		status = cmi_write(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			gvcmz_error(CMMS_B_BUFRESIZE, status);
			return;
		}
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			gvcmz_error(CMMS_B_BUFRESIZE, status);
			return;
		}
		if (*lnk->mbf != CMMS_C_BUFRESIZE)
		{
			if (*lnk->mbf != CMMS_E_ERROR)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			gvcmz_errmsg(lnk,FALSE);
			return;
		}
		usr->buffer_size = zdefbufsiz;
		if (usr->buffer)
		{
			free(usr->buffer);
			usr->buffer = 0;
		}
	}

	if (!usr->buffer)
		usr->buffer = malloc(usr->buffer_size);

	if (!usr->buffer_used)
	{
		buffptr = usr->buffer;
		*buffptr++ = CMMS_B_BUFFLUSH;
		buffptr += SIZEOF(short);
		usr->buffer_used = (unsigned char *)buffptr - usr->buffer;
	} else
		buffptr = usr->buffer + 3;	/* trancode + # of transactions */
	bufftop = usr->buffer + usr->buffer_used;

	cc = 0;
	overlay = FALSE;
	brec = (bunch_rec *)buffptr;
	for (; buffptr < bufftop ;)
	{
		if (cc < brec->cc)
		{
			FORWARD_REC(brec);
			continue;
		}
		if (cc > brec->cc)
			break;
		new_key = &gv_currkey->base[cc];
		len = (brec->len > gv_currkey->end - cc + 1 ? gv_currkey->end - cc + 1 : brec->len);
		rec_key = &brec->base[0];
		for (i = 0; i < len; i++)
		{
			is_gt = *new_key++ - *rec_key++;
			if (is_gt)
				break;
		}
		if (i == len)
		{
			len = brec->len + brec->cc - 1;
			if (gv_currkey->end > len)
			{
				cc += i;
				FORWARD_REC(brec);
				continue;
			}
			if( gv_currkey->end == len)
			{
				if (brec->rn != gv_cur_region->cmx_regnum)
				{
					cc += i;
					FORWARD_REC(brec);
					continue;
				} else
					overlay = TRUE;
			}
			break;
		} else if (is_gt > 0)
		{
			cc += i;
			FORWARD_REC(brec);
			continue;
		} else
			break;
	}
	nrec.rn = gv_cur_region->cmx_regnum;
	nrec.len = gv_currkey->end - cc + 1;
	nrec.prv = gv_currkey->prev;
	nrec.cc = cc;
	newrec_len = SIZEOF(nrec) - 1 + nrec.len + SIZEOF(unsigned short) + v->str.len;
	if (overlay)
	{
		oldrec_len = SIZEOF(*brec) - 1 + brec->len;
		CM_GET_USHORT(tmp_short, buffptr + oldrec_len, usr->convert_byteorder);
		oldrec_len += tmp_short + SIZEOF(unsigned short);
		new_space = newrec_len - oldrec_len;
		insert_record = buffptr;
		FORWARD_REC(brec);
	} else
	{	insert_record = buffptr;
		new_space = newrec_len;
	}
	if (usr->buffer_used + new_space >= usr->buffer_size)
		VMS_ONLY(rts_error(VARLSTCNT(4) ERR_ZDEFOFLOW, 2, lnk->nod.dsc$w_length, lnk->nod.dsc$a_pointer);)
		UNIX_ONLY(rts_error(VARLSTCNT(4) ERR_ZDEFOFLOW, 2, lnk->nod.len, lnk->nod.addr);)

	memcpy(buffptr + new_space, buffptr, bufftop - buffptr);	/* shuffle buffer to make room for new record */
	memcpy(insert_record, &nrec, SIZEOF(nrec) - 1);
	insert_record += SIZEOF(nrec) - 1;
	memcpy(insert_record, &gv_currkey->base[cc], nrec.len);
	insert_record += nrec.len;
	tmp_short = (unsigned short)v->str.len;
	assert((int4)tmp_short == v->str.len); /* ushort <- int4 assignment lossy? */
	CM_PUT_USHORT(insert_record, tmp_short, usr->convert_byteorder);
	insert_record += SIZEOF(unsigned short);
	memcpy(insert_record, v->str.addr, v->str.len);
	usr->buffer_used += new_space;
	if (!overlay)
		usr->buffered_count++;
}
