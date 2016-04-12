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
#include "gtm_ctype.h"
#include "gtm_string.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_protocol.h"

static char *encode_cpu(void);
static char *encode_os(void);

LITDEF	gtcm_proto_cpu_info_t	gtcm_proto_cpu_info[] =
{
	LIT_AND_LEN("IA64"),			"IA64",
	LIT_AND_LEN("RS6000"),			"PPC",
	LIT_AND_LEN("AXP"),			"AXP",
	LIT_AND_LEN("HP-PA"),			"PAR",
	LIT_AND_LEN("x86"),			"X86",
	LIT_AND_LEN("x86_64"),			"X86_64",
	LIT_AND_LEN("S390"),			"390",
	LIT_AND_LEN("S390X"),			"390",
	LIT_AND_LEN("SPARC"),			"SPA",
	LIT_AND_LEN("VAX"),			"VAX",
	LIT_AND_LEN(GTCM_PROTO_BAD_CPU),	GTCM_PROTO_BAD_CPU
};

LITDEF	gtcm_proto_os_info_t	gtcm_proto_os_info[] =
{
	LIT_AND_LEN("AIX"),			"AIX",
	LIT_AND_LEN("OSF1"),			"OSF",
	LIT_AND_LEN("HP-UX"),			"HPX",
	LIT_AND_LEN("Linux"),			"LNX",
	LIT_AND_LEN("OS390"),			"zOS",
	LIT_AND_LEN("Solaris"),			"SOL",
	LIT_AND_LEN("VMS"),			"VMS",
	LIT_AND_LEN(GTCM_PROTO_BAD_OS),		GTCM_PROTO_BAD_OS
};

LITREF	char		gtm_release_name[];
LITREF	int4		gtm_release_name_len;
LITREF	char		gtm_version[];
LITREF	char 		cm_ver_name[];
LITREF	int4		cm_ver_len;

static	int		proto_built = FALSE;
static	protocol_msg	proto;

void gtcm_protocol(protocol_msg *pro)
{
	if (!proto_built)
	{
		memcpy(proto.msg + CM_CPU_OFFSET, encode_cpu(), 3);
		memcpy(proto.msg + CM_OS_OFFSET, encode_os(), 3);
		memcpy(proto.msg + CM_IMPLEMENTATION_OFFSET, "GTM", 3);
		/* gtm_version is of the form Vi.j where i and j are digits */
		assert('V' == gtm_version[0] && ISDIGIT_ASCII(gtm_version[1]) && '.' == gtm_version[2]
			&& ISDIGIT_ASCII(gtm_version[3]));
		proto.msg[CM_VERSION_OFFSET] = '0';
		proto.msg[CM_VERSION_OFFSET + 1] = gtm_version[1];
		proto.msg[CM_VERSION_OFFSET + 2] = gtm_version[3];
		memcpy(proto.msg + CM_TYPE_OFFSET, CMM_PROTOCOL_TYPE, 3);
		/* cm_ver_name is of the form Vijk where i, j, and k are digits */
		assert('V' == cm_ver_name[0] && ISDIGIT_ASCII(cm_ver_name[1]) && ISDIGIT_ASCII(cm_ver_name[2])
			&& ISDIGIT_ASCII(cm_ver_name[3]));
		memcpy(proto.msg + CM_LEVEL_OFFSET, &cm_ver_name[1], 3);
#ifdef BIGENDIAN
		proto.msg[CM_ENDIAN_OFFSET] = GTCM_BIG_ENDIAN_INDICATOR;
#else
		proto.msg[CM_ENDIAN_OFFSET] = ' ';
#endif
		memset(proto.msg + CM_ENDIAN_OFFSET + 1, ' ' , CM_FILLER_SIZE);
		proto_built = TRUE;
	}
	memcpy(pro->msg, proto.msg, S_PROTSIZE);
	/* memcpy(pro->msg, S_PROTOCOL, S_PROTSIZE); */
}

boolean_t gtcm_is_big_endian(protocol_msg *pro)
{
	return pro->msg[CM_ENDIAN_OFFSET] == GTCM_BIG_ENDIAN_INDICATOR;
}

boolean_t gtcm_protocol_match(protocol_msg *peer, protocol_msg *me)
{
	if (memcmp(peer->msg + CM_TYPE_OFFSET, me->msg + CM_TYPE_OFFSET, 3))
		return FALSE;
	assert(0 <= memcmp(me->msg, CMM_MIN_PEER_LEVEL, 3));
	if (0 > memcmp(peer->msg + CM_LEVEL_OFFSET, me->msg + CM_LEVEL_OFFSET, 3) && /* peer running older version of GNP */
	    0 > memcmp(peer->msg + CM_LEVEL_OFFSET, CMM_MIN_PEER_LEVEL, 3)) /* older than the oldest supported version with */
		return FALSE;						    /* our version */
	return TRUE;
}

static char *encode_cpu()
{
	unsigned char	*p;
	int		count, cpuidx;

	count = 0;
	p = (unsigned char *)gtm_release_name;
	/* fourth arg in release name string */
	while (*p && count < 3)
	{
		if (*p == ' ')
			count++;
		p++;
	}
	if (count == 3)
	{
		for (cpuidx = 0; cpuidx < SIZEOF(gtcm_proto_cpu_info)/SIZEOF(gtcm_proto_cpu_info_t) - 1; cpuidx++)
		{
			if (0 == memcmp(p, gtcm_proto_cpu_info[cpuidx].cpu_in_rel_str,
						gtcm_proto_cpu_info[cpuidx].size_of_cpu_in_rel_str))
				return gtcm_proto_cpu_info[cpuidx].proto_cpu;
		}
	}
	GTMASSERT;
	return NULL; /* Added to make compiler happy and not throw warning */
}

static char *encode_os()
{
	unsigned char	*p;
	int		count, osidx;

	count = 0;
	p = (unsigned char *)gtm_release_name;
	/* third arg in release name string */
	while (*p && count < 2)
	{
		if (*p == ' ')
			count++;
		p++;
	}
	if (count == 2)
	{
		for (osidx = 0; osidx < SIZEOF(gtcm_proto_os_info)/SIZEOF(gtcm_proto_os_info_t) - 1; osidx++)
		{
			if (0 == memcmp(p, gtcm_proto_os_info[osidx].os_in_rel_str,
						gtcm_proto_os_info[osidx].size_of_os_in_rel_str))
				return gtcm_proto_os_info[osidx].proto_os;
		}
	}
	GTMASSERT;
	return NULL; /* Added to make compiler happy and not throw warning */
}
