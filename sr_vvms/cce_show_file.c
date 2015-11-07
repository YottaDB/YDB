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
#include "gdsblk.h"
#include <rmsdef.h>
#include <fab.h>
#include <xab.h>
#include <ssdef.h>
#include <iodef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <efndef.h>

#include "filestruct.h"
#include "util.h"
#include "cli.h"

#define PUTTIM(L,T) (outbufidx = 0 , memset(outbuf, ' ', SIZEOF(outbuf)), \
    PUTLIT(L), sys$asctim(&timlen, &time_desc, (T), 1), outbufidx = time_desc.dsc$w_length + timlen, \
    util_out_write(outbuf, outbufidx))
#define PUTSTR(A,L) (memcpy(&outbuf[outbufidx], (A), (L)), outbufidx += (L))
#define PUTLIT(X) PUTSTR((X), SIZEOF(X) - 1)

/* NOTE: WHY BOTH old_data and dummy_data? */
cce_show_file()
{
	char			fn[256];
	struct FAB  		ccpfab;
	struct XABFHC		xab;
	sgmnt_data		*old_data, *dummy_data;
	sgmnt_addrs		*cs;
	short			iosb[4];
	unsigned short		fn_len;
	short unsigned		timlen;
	struct dsc$descriptor_s	time_desc;
	int4			status, size, cluster;
	unsigned char		*c;
	unsigned char 		outbuf[80];
	int			outbufidx;
	$DESCRIPTOR(output_qualifier, "OUTPUT");
	error_def(ERR_CCERDERR);
	error_def(ERR_CCEBADFN);
	error_def(ERR_DBOPNERR);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_BADDBVER);
	error_def(ERR_CCEBGONLY);

	fn_len = SIZEOF(fn);
	if (!cli_get_str("FILE",fn,&fn_len))
	{
		lib$signal(ERR_CCEBADFN);
		return;
	}
	ccpfab = cc$rms_fab;
	ccpfab.fab$l_fna = fn;
	ccpfab.fab$b_fns = fn_len;
	ccpfab.fab$b_fac = FAB$M_BIO | FAB$M_GET;
	ccpfab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	ccpfab.fab$l_fop = FAB$M_UFO;
	xab = cc$rms_xabfhc;
	ccpfab.fab$l_xab = &xab;
	status = sys$open(&ccpfab);
	if (status != RMS$_NORMAL)
	{
		lib$signal(ERR_DBOPNERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
		return;
	}
	dummy_data = malloc(512);
	status = sys$qiow(EFN$C_ENF, ccpfab.fab$l_stv, IO$_READVBLK, iosb, 0, 0, dummy_data, 512, 1, 0, 0, 0);
	if (status & 1)
		status = iosb[0];
	if ((status & 1) == 0)
	{
		lib$signal(ERR_CCERDERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
		sys$dassgn(ccpfab.fab$l_stv);
		return;
	}
	if (memcmp(&dummy_data->label[0], GDS_LABEL, 12))
	{
		if (memcmp(&dummy_data->label[0], GDS_LABEL, 9))
			lib$signal (ERR_DBNOTGDS, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna);
		else
			lib$signal (ERR_BADDBVER, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna);
		status = sys$dassgn(ccpfab.fab$l_stv);
		assert(status & 1);
		return;
	}
	if (dummy_data->acc_meth != dba_bg)
	{
		lib$signal(ERR_CCEBGONLY);
		status = sys$dassgn(ccpfab.fab$l_stv);
		assert(status & 1);
		return;
	}
	size = (((SIZEOF(sgmnt_data)) + 511)/512) * 512;
	old_data = malloc(size);
	status = sys$qiow(EFN$C_ENF, ccpfab.fab$l_stv, IO$_READVBLK, iosb, 0, 0, old_data, size, 1, 0, 0, 0);
	if (status & 1)
		status = iosb[0];
	if ((status & 1) == 0)
	{
		lib$signal(ERR_CCERDERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
		status = sys$dassgn(ccpfab.fab$l_stv);
		assert(status & 1);
		return;
	}
	outbufidx = 0;
	util_out_open(&output_qualifier);
	PUTLIT("Database file ");
	PUTSTR(fn, fn_len);
	PUTLIT(" is ");
	if (!old_data->clustered)
	{
		PUTLIT(" NOT ");
	}
	PUTLIT(" a cluster database");
	util_out_write(outbuf, outbufidx);
	time_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	time_desc.dsc$b_class = DSC$K_CLASS_S;
	time_desc.dsc$a_pointer = &outbuf[20];
	time_desc.dsc$w_length = 40;
	PUTTIM("STALE_INTERVAL", old_data->staleness);
	PUTTIM("RESPONSE_INTERVAL",old_data->ccp_response_interval);
	PUTTIM("QUANTUM_INTERVAL", old_data->ccp_quantum_interval);
	PUTTIM("TICK_INTERVAL", old_data->ccp_tick_interval);
	util_out_close();
	status = sys$dassgn(ccpfab.fab$l_stv);
	assert(status & 1);
	free(dummy_data);
	free(old_data);
	return;
}
