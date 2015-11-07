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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include <descrip.h>
#include "probe.h"


unsigned char *ccp_format_time(
date_time *tim,
unsigned char *inaddr,
unsigned short maxlen)
{
	globalvalue LIB$M_TIME_FIELDS;
	struct dsc$descriptor_s dx;
	short outlen;
	unsigned int flags = LIB$M_TIME_FIELDS;

	dx.dsc$w_length = maxlen;
	dx.dsc$b_dtype = DSC$K_DTYPE_T;
	dx.dsc$b_class = DSC$K_CLASS_S;
	dx.dsc$a_pointer = inaddr;
	lib$format_date_time(&dx, tim, 0, &outlen, &flags);
	return inaddr + outlen;
}


unsigned char *ccp_fqstr(
unsigned char *str,
unsigned char *cp,
unsigned int maxlen)
{
	int n;
	for (n = 0 ; n < maxlen && *cp ; n++)
		*str++ = *cp++;
	return str;
}


#define CCP_TABLE_ENTRY(A,B,C,D) "A",
static	const unsigned char	names[][16] =
{
#include "ccpact_tab.h"
};
#undef CCP_TABLE_ENTRY

#define CCP_TABLE_ENTRY(A,B,C,D) C,
static	const unsigned char	rectyp[] =
{
#include "ccpact_tab.h"
};
#undef CCP_TABLE_ENTRY


unsigned char *ccp_format_querec(
ccp_que_entry	*inrec,
unsigned char	*outbuf,
unsigned short	outbuflen)
{
	unsigned char		*out, *out1;
	ccp_action_code		act;
	ccp_action_record	*rec;
	ccp_db_header		*db;
	int			n;

	rec = &inrec->value;
	out = outbuf;
	act = rec->action;
	if (act < 0 || act >= CCPACTION_COUNT)
	{
		out = ccp_fqstr(out, "Action code not valid", outbuflen);
		/* should add more info here */
		return out;
	}
	out1 = out + 10;
	out = ccp_fqstr(out, &names[act][5], SIZEOF(names[act]));
	while (out < out1)
		*out++ = ' ';
	i2hex(rec->pid, out, SIZEOF(rec->pid) *2);
	out += 8;
	*out++ = ' ';
	out = ccp_format_time(&inrec->request_time, out, outbuf + outbuflen - out);
	*out++ = ' ';
	out = ccp_format_time(&inrec->process_time, out, outbuf + outbuflen - out);
	*out++ = ' ';
	switch(rectyp[act])
	{
/* add info here */
	case CCTVSTR:
		out = ccp_fqstr(out, rec->v.str.txt, rec->v.str.len);
		break;
	case CCTVMBX:
		break;
	case CCTVFIL:
		assert(rec->v.file_id.dvi[0] < SIZEOF(rec->v.file_id.dvi));
		out = ccp_fqstr(out, &rec->v.file_id.dvi[1], rec->v.file_id.dvi[0]);
		for (n = 0 ; n < SIZEOF(rec->v.file_id.did) / SIZEOF(rec->v.file_id.did[0]) ; n++)
		{
			*out++ = ' ';
			i2hex(rec->v.file_id.did[n], out, SIZEOF(rec->v.file_id.did[n]) * 2);
			out += SIZEOF(rec->v.file_id.did[n]) * 2;
			/* note: wouldn't hurt tomodify i2hex to return end of string */
		}
		for (n = 0 ; n < SIZEOF(rec->v.file_id.fid) / SIZEOF(rec->v.file_id.fid[0]) ; n++)
		{
			*out++ = ' ';
			i2hex(rec->v.file_id.fid[n], out, SIZEOF(rec->v.file_id.fid[n]) * 2);
			out += SIZEOF(rec->v.file_id.fid[n]) * 2;
		}
		break;
	case CCTVDBP:
		db = rec->v.h;
		if (probe(SIZEOF(*db),db,FALSE) && probe(SIZEOF(gd_region), db->greg, FALSE))
		{	if (probe(SIZEOF(sgmnt_data),db->glob_sec,FALSE) &&
				!memcmp(db->glob_sec->label, GDS_LABEL,GDS_LABEL_SZ -1))
			{
				assert(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.dvi[0]
					< SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.dvi));
				out = ccp_fqstr(out, &((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.dvi[1],
						((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.dvi[0]);
				for (n = 0 ; n <
					SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.did)
					/ SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.did[0]) ;
					n++)
				{
					*out++ = ' ';
					i2hex(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.did[n], out,
					 SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.did[n]) * 2);
					out +=
					 SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.did[n]) * 2;
					/* note: wouldn't hurt tomodify i2hex to return end of string */
				}
				for (n = 0 ;
					n <
					 SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.fid)
					 / SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.fid[0]) ;
					n++)
				{
					*out++ = ' ';
					i2hex(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.fid[n],
					 out,
					 SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.fid[n]) * 2);
					out +=
					 SIZEOF(((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id.fid[n]) * 2;
				}
			}
		}
		break;
	}
	return out;
}
