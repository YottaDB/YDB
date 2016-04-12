/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTCM_PROTOCOL_H_INCLUDED
#define GTCM_PROTOCOL_H_INCLUDED

/* returns the local protocol into the pointer provided */
void gtcm_protocol(protocol_msg *pro);

/* returns a TRUE when the indicated protocol is big endian */
boolean_t gtcm_is_big_endian(protocol_msg *pro);

/* returns a TRUE when the two protocols are compatible */
boolean_t gtcm_protocol_match(protocol_msg *peer, protocol_msg *me);

typedef	struct
{
	char	*cpu_in_rel_str;
	int	size_of_cpu_in_rel_str;
	char	*proto_cpu;
} gtcm_proto_cpu_info_t;

typedef struct
{
	char	*os_in_rel_str;
	int	size_of_os_in_rel_str;
	char	*proto_os;
} gtcm_proto_os_info_t;

#define GTCM_PROTO_BAD_CPU		"***"
#define GTCM_PROTO_BAD_OS		"***"

#define GTCM_BIG_ENDIAN_INDICATOR	'B'
#endif
