/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRACE_TABLE_INCLUDED
#define TRACE_TABLE_INCLUDED

/* Enum of possible trace groups and types created from trace types table. The objective is to form
 * enums of the form group_type with a value of the group # in the high-order 16 bits and the trace
 * type for that group in the low-order 16 bits. This generates 3 enums given groups and types:
 *
 *   TRACE_GROUP_<group> is the enum for a given group (1, 2, 3, etc).
 *   TRCGRP_<group>      is the same as TRACE_GROUP_<group> except is is shifted into the high order 16 bits
 *                       and is used to create the value for <group>_<type>
 *   <group>_<type>      is the actual trace value used for a given group/type.
 */

/* First segment defines the trace group ids. These enums are TRCGRP_<type> and are used in the 2nd
 * section to generate proper values for the full group_type enum types.
 */
#define TRACEGROUP(group) TRACE_GROUP##_##group,
#define TRACETYPE(group, type, int, addr1, addr2, addr3)
enum trace_groups
{
	TRACE_GROUP_NOT_USED = 0,
#	include "trace_table_types.h"
	LAST_TRACE_GROUP
};
#undef TRACEGROUP
#undef TRACETYPE

/* Now generate the shifted group and group_type vars with correct values */
#define TRACEGROUP(group) TRCGRP##_##group = (TRACE_GROUP##_##group << 16),	/* Reset the value for successive group members */
#define TRACETYPE(group, type, int, addr1, addr2, addr3) group##_##type,
enum trace_types
{
	TRACE_TYPE_NOT_USED = 0,
#	include "trace_table_types.h"
	LAST_TRACE_TYPE		/* Not really useful as a value but terminates the enums without leaving a dangling "," */
};
#undef TRACEGROUP
#undef TRACETYPE

#define TRACE_TABLE_SIZE_DEFAULT 500

#define TRCTBL_ENTRY(typeval, intval, addrval1, addrval2, addrval3)						\
{														\
	/* If tracing in general, verify this group is being traced */						\
	if ((NULL != TREF(gtm_trctbl_start)) && (0 != (TREF(gtm_trctbl_groups) & (1 << (typeval >> 16)))))	\
	{	/* Trace only if we are tracing */								\
		(TREF(gtm_trctbl_cur))++;	/* Next entry */						\
		if (TREF(gtm_trctbl_cur) >= TREF(gtm_trctbl_end))						\
			TREF(gtm_trctbl_cur) = TREF(gtm_trctbl_start);						\
		(TREF(gtm_trctbl_cur))->type = (int4)(typeval);							\
		(TREF(gtm_trctbl_cur))->intfld = (int4)(intval);						\
		(TREF(gtm_trctbl_cur))->addrfld1 = (void *)(addrval1);						\
		(TREF(gtm_trctbl_cur))->addrfld2 = (void *)(addrval2);						\
		(TREF(gtm_trctbl_cur))->addrfld3 = (void *)(addrval3);						\
	}													\
}

/* Structure defining the size of a single trace entry */
typedef struct trctbl_entry_struct
{
	int4	type;
	int4	intfld;
	void	*addrfld1;
	void	*addrfld2;
	void	*addrfld3;
} trctbl_entry;

#endif
