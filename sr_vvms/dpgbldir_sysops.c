/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <fab.h>
#include <iodef.h>
#include <nam.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gbldirnam.h"
#include "filestruct.h"
#include "io.h"
#include "stringpool.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "trans_log_name.h"
#include "gtm_logicals.h"

GBLREF mval		dollar_zgbldir;

char LITDEF gde_labels[GDE_LABEL_NUM][GDE_LABEL_SIZE] =
{
	GDE_LABEL_LITERAL
};

/************************THESE ROUTINES ARE OS SPECIFIC AND SO WILL HAVE TO RESIDE IN ANOTHER MODULE*********************/

#define DVI_LEN		(SIZEOF(cc$rms_nam.nam$t_dvi))
#define DID_LEN		(SIZEOF(cc$rms_nam.nam$w_did))
#define FID_LEN		(SIZEOF(cc$rms_nam.nam$w_fid))

bool comp_gd_addr(gd_addr *gd_ptr, struct FAB *file_ptr)
{
	if ((memcmp(&file_ptr->fab$l_nam->nam$w_fid, gd_ptr->id->fid, FID_LEN) == 0) &&
	    (memcmp(&file_ptr->fab$l_nam->nam$t_dvi, gd_ptr->id->dvi, DVI_LEN) == 0))

		return TRUE;
	return FALSE;
}

void fill_gd_addr_id(gd_addr *gd_ptr, struct FAB *file_ptr)
{
	gd_ptr->id = malloc(SIZEOF(*gd_ptr->id));
	memcpy(&gd_ptr->id->dvi, &file_ptr->fab$l_nam->nam$t_dvi, DVI_LEN);
	memcpy(&gd_ptr->id->did, &file_ptr->fab$l_nam->nam$w_did, DID_LEN);
	memcpy(&gd_ptr->id->fid, &file_ptr->fab$l_nam->nam$w_fid, FID_LEN);

	return;
}
/********************************THESE ROUTINES ARE STUBS PROVIDED FOR TESTING PURPOSES************************************/
#define DOTGLD ".GLD"
void *open_gd_file(mstr *v)
{
	struct FAB	*fab;
	int4		status;

	error_def(ERR_ZGBLDIRACC);

	fab = malloc(SIZEOF(struct FAB));
	*fab = cc$rms_fab;
	fab->fab$l_fna = v->addr;
	fab->fab$b_fns = v->len;
	fab->fab$l_fop = FAB$M_UFO ;
	fab->fab$b_fac = FAB$M_GET | FAB$M_BIO;
	fab->fab$l_nam = malloc(SIZEOF(struct NAM));
	fab->fab$l_dna = DOTGLD;
	fab->fab$b_dns = SIZEOF(DOTGLD) - 1;
	*fab->fab$l_nam = cc$rms_nam;
	status = sys$open(fab);
	if (status != RMS$_NORMAL)
	{
		if (!dollar_zgbldir.str.len || ((dollar_zgbldir.str.len == v->len)
							&& !memcmp(dollar_zgbldir.str.addr, v->addr, v->len)))
		{
			rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, v->len, v->addr,
				LEN_AND_LIT(".  Cannot continue"), LEN_AND_LIT(""), status);
			assert(FALSE);
		}
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, v->len, v->addr, LEN_AND_LIT(".  Retaining "),
			dollar_zgbldir.str.len, dollar_zgbldir.str.addr, status);
	}
	return fab;
}

void file_read(struct FAB *file_ptr, int4 size, char *buff, int4 pos)
{
	int4		status;
	short		iosb[4];

	error_def(ERR_ZGBLDIRACC);

	status = sys$qiow(EFN$C_ENF,file_ptr->fab$l_stv, IO$_READVBLK, &iosb[0], 0, 0, buff, size, pos, 0, 0, 0);
	if (status == SS$_NORMAL)
		status = iosb[0];
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->fab$b_fns,file_ptr->fab$l_fna,
			LEN_AND_LIT(""), LEN_AND_LIT(""), status);
	return;
}

void close_gd_file(struct FAB *file_ptr)
{
	sys$dassgn(file_ptr->fab$l_stv);
	if (file_ptr->fab$l_nam)
		free(file_ptr->fab$l_nam);
	free(file_ptr);
	return;
}


void dpzgbini(void)
{
	mstr	temp_mstr;
	char	temp_buff[MAX_TRANS_NAME_LEN];

	dollar_zgbldir.mvtype = MV_STR;
	dollar_zgbldir.str.addr = GTM_GBLDIR;
	dollar_zgbldir.str.len = SIZEOF(GTM_GBLDIR) - 1;
	if (SS$_NORMAL == trans_log_name(&dollar_zgbldir.str, &temp_mstr, &temp_buff[0]))
	{
		dollar_zgbldir.str.len = temp_mstr.len;
		dollar_zgbldir.str.addr = temp_mstr.addr;
	}
	s2pool(&dollar_zgbldir.str);
}

mstr *get_name(mstr *ms)
{
	int4	status;
	char	c[MAX_TRANS_NAME_LEN];
	mstr	ms1, *new;

	error_def(ERR_ZGBLDIRACC);

	if ((status = trans_log_name(ms,&ms1,&c[0])) == SS$_NORMAL)
		ms = &ms1;
	else if (status != SS$_NOLOGNAM)
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, ms->len, ms->addr, LEN_AND_LIT(""), LEN_AND_LIT(""), status);
	new = malloc(SIZEOF(mstr));
	new->len = ms->len;
	new->addr = malloc(ms->len);
	memcpy(new->addr,ms->addr,ms->len);
	return new;
}
