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

#ifdef VMS
#  include <descrip.h>
#  include <ssdef.h>
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmidef.h"
#include "cmmdef.h"
#include "stringpool.h"
#include "gtm_string.h"
#include "gvcmy_close.h"
#include "gvcmz.h"
#include "gt_timer.h"
#include "copy.h"
#include "iosp.h"
#include "gtcm_protocol.h"
#include "gtcm_is_query_queryget.h"
#include "gtcm_err_compat.h"
#include "error.h"

GBLREF boolean_t		gtcm_connection;
GBLREF jnl_process_vector	*prc_vec;
GBLREF spdesc			stringpool;
GBLREF struct NTD		*ntd_root;

static volatile boolean_t	second_attempt = FALSE;
static protocol_msg		myproto;
static struct CLB		*clb;

int		v010_jnl_process_vector_size(void);
void		v010_jnl_prc_vector(void *);

CONDITION_HANDLER(gvcmz_netopen_ch)
{
	error_def(CMERR_INVPROT);

	START_CH;
	if (SIGNAL != CMERR_INVPROT || second_attempt)
	{
		second_attempt = FALSE;
		assert(clb);
		gvcmy_close(clb);
		NEXTCH;
	}
	second_attempt = TRUE;
	UNWIND(NULL, NULL);
}

void gvcmz_netopen_attempt(struct CLB *c)
{
	unsigned char	*ptr, *proto_str;
	int		prc_vec_size;
	int		status;
#ifdef BIGENDIAN
	jnl_process_vector	temp_vect;
#endif

	error_def(ERR_BADSRVRNETMSG);
	error_def(ERR_NETDBOPNERR);

	VMS_ONLY(
		ESTABLISH(gvcmz_netopen_ch); /* our old servers run only on VMS; no need for retry on other OSs */
		clb = c; /* need this assignment since we can't pass c to gvcmz_netopen_ch */
	)
	c->mbf = stringpool.free;
	c->mbl = CM_MINBUFSIZE;
	ptr = c->mbf;
	*ptr++ = CMMS_S_INITPROC;
	if (!second_attempt)
	{
		proto_str = (unsigned char *)&myproto;
		if (!prc_vec)
		{
			prc_vec = malloc(SIZEOF(*prc_vec));
			jnl_prc_vector(prc_vec);
		}
		prc_vec_size = SIZEOF(*prc_vec);
#ifdef BIGENDIAN
		memcpy((unsigned char *)&temp_vect, (unsigned char *)prc_vec, SIZEOF(jnl_process_vector));
		temp_vect.jpv_pid =  GTM_BYTESWAP_32(temp_vect.jpv_pid);
		temp_vect.jpv_image_count =  GTM_BYTESWAP_32(temp_vect.jpv_image_count);
		temp_vect.jpv_time =  GTM_BYTESWAP_64(temp_vect.jpv_time);
		temp_vect.jpv_login_time =  GTM_BYTESWAP_64(temp_vect.jpv_login_time);
		memcpy(ptr + S_PROTSIZE, (unsigned char *)&temp_vect, SIZEOF(jnl_process_vector));
#else
		memcpy(ptr + S_PROTSIZE, (unsigned char *)prc_vec, prc_vec_size);
#endif
	} else
	{
#ifdef VMS
		/* We connected with V010 server; let's behave like a V010 client. */
		proto_str = (unsigned char *)S_PROTOCOL;
		assert(prc_vec);
		assert(SIZEOF(*prc_vec) > v010_jnl_process_vector_size());
		v010_jnl_prc_vector(prc_vec);
		prc_vec_size = v010_jnl_process_vector_size();
		memcpy(ptr + S_PROTSIZE, (unsigned char *)prc_vec, prc_vec_size);
#elif defined(UNIX)
		GTMASSERT;
#else
#error Unsupported platform
#endif
	}
	memcpy(ptr, proto_str, S_PROTSIZE);
	ptr += S_PROTSIZE;
	c->cbl = S_HDRSIZE + S_PROTSIZE + prc_vec_size;
	status = cmi_write(c);	/* INITPROC */
	if (CMI_ERROR(status))
	{
		gvcmy_close(c);
		rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	status = cmi_read(c);	/* return message should be same size */
	if (CMI_ERROR(status))
	{
		gvcmy_close(c);
		rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	if (CMMS_T_INITPROC != *c->mbf)
	{
		if (CMMS_E_ERROR != *c->mbf)
		{
			gvcmy_close(c);
			rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, ERR_BADSRVRNETMSG);
		}
		gvcmz_errmsg(c, FALSE);
	}
	VMS_ONLY(
		REVERT;
		clb = NULL; /* defensive programming */
		second_attempt = FALSE; /* we succesfully connected; no need for one more try */
	)
}

struct CLB *gvcmz_netopen(struct CLB *c, cmi_descriptor *node, cmi_descriptor *task)
{
	static readonly int4	wait[2] = {-100000, -1};
	static readonly int4	reptim[2] = {-10000, -1};
	unsigned char		*ptr;
	link_info		*li;
	int			len, i;
	uint4			status;
	protocol_msg		*server_proto;

	error_def(ERR_BADSRVRNETMSG);
	error_def(ERR_NETDBOPNERR);
	error_def(ERR_TEXT);
	error_def(CMERR_INVPROT);

	c = UNIX_ONLY(cmi_alloc_clb())VMS_ONLY(cmu_makclb());
	c->usr = malloc(SIZEOF(link_info));
	li = c->usr;
	memset(li, 0, SIZEOF(*li));
	c->err = gvcmz_neterr_set;
#ifdef VMS
	c->nod.dsc$b_dtype = c->tnd.dsc$b_dtype = DSC$K_DTYPE_T;
	c->nod.dsc$b_class = c->tnd.dsc$b_class = DSC$K_CLASS_S;
	c->nod.dsc$w_length = node->dsc$w_length;
	c->nod.dsc$a_pointer = malloc(c->nod.dsc$w_length);
	memcpy(c->nod.dsc$a_pointer, node->dsc$a_pointer, node->dsc$w_length);
	c->tnd.dsc$w_length = task->dsc$w_length;
	c->tnd.dsc$a_pointer = malloc(c->tnd.dsc$w_length);
	memcpy(c->tnd.dsc$a_pointer, task->dsc$a_pointer, task->dsc$w_length);
	for (i = 0; i < 2; i++) /* This retry should really be pushed down into cmi_open */
	{
		status = cmi_open(c);
		if (!CMI_ERROR(status))
			break;
		hiber_start_wait_any(10);
	}
#elif defined(UNIX)
	c->nod.addr = malloc(node->len);
	c->nod.len = node->len;
	memcpy(c->nod.addr, node->addr, node->len);
	c->tnd.len = task->len;
	c->tnd.addr = malloc(c->tnd.len);
	memcpy(c->tnd.addr, task->addr, task->len);
	status = cmi_open(c);
#else
#error Unsupported platform
#endif
	if (CMI_ERROR(status))
	{
		free(c->usr);
		free(VMS_ONLY(c->nod.dsc$a_pointer) UNIX_ONLY(c->nod.addr));
		free(VMS_ONLY(c->tnd.dsc$a_pointer) UNIX_ONLY(c->tnd.addr));
		VMS_ONLY(lib$free_vm(&SIZEOF(*c), &c, 0);)
		UNIX_ONLY(cmi_free_clb(c));
		rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, status);
	}
	if (0 == ntd_root)
		ntd_root = cmu_ntdroot();
	gtcm_protocol(&myproto);
	li->lnk_active = TRUE;
	ENSURE_STP_FREE_SPACE(CM_MINBUFSIZE);
	do
	{
		gvcmz_netopen_attempt(c);
	} while (second_attempt);
	if (S_HDRSIZE + S_PROTSIZE + 2 != c->cbl)
	{
		gvcmy_close(c);
		rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, ERR_BADSRVRNETMSG);
	}
	server_proto = (protocol_msg *)(c->mbf + 1);
	if (!gtcm_protocol_match(server_proto, &myproto))
	{
		gvcmy_close(c);
		rts_error(VARLSTCNT(3) ERR_NETDBOPNERR, 0, CMERR_INVPROT);
	}
	li->convert_byteorder = (gtcm_is_big_endian(&myproto) != gtcm_is_big_endian(server_proto));
	li->query_is_queryget = gtcm_is_query_queryget(server_proto, &myproto);
	li->server_supports_dollar_incr = (0 <= memcmp(server_proto->msg + CM_LEVEL_OFFSET, CMM_INCREMENT_MIN_LEVEL, 3));
	li->server_supports_std_null_coll = (0 <= memcmp(server_proto->msg + CM_LEVEL_OFFSET, CMM_STDNULLCOLL_MIN_LEVEL, 3));
	li->server_supports_long_names = (0 <= memcmp(server_proto->msg + CM_LEVEL_OFFSET, CMM_LONGNAMES_MIN_LEVEL, 3));
	if (!(li->err_compat = gtcm_err_compat((protocol_msg *)(c->mbf + 1), &myproto)))
	{
		gvcmy_close(c);
		rts_error(VARLSTCNT(6) ERR_NETDBOPNERR, 0, ERR_TEXT, 2,
				LEN_AND_LIT("GTCM functionality not implemented between UNIX and VMS yet"));
	}
	gtcm_connection = TRUE;
	CM_GET_USHORT(li->procnum, (c->mbf + S_HDRSIZE + S_PROTSIZE), li->convert_byteorder);
	return c;
}
