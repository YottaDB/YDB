/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef COPY_H_INCLUDED
#define COPY_H_INCLUDED
#include "gtm_string.h"
#include "mdef.h"

/* Previously, the macros at the bottom were either bytewise copies on platforms that
 * did not support unaligned memory access or cast-and-assign operationss on those that did.
 * But unaligned access is undefined behavior in C, and even on platforms where some instructions
 * permit undefined operands, others (such as vector instructions), may fault in that case. Now we
 * always do bytewise copies, but through a call to memcpy which is strictly better than explicit
 * byte copying since it allows the compiler to do more optimization. The old macros are redefined
 * to use new static inline functions that wrap the memcpy calls, and the static inlines have been
 * slightly renamed to remove legacy cruft (long->int, llong->long). Finally, the macros universally
 * cast the source pointer to unsigned char * since such a cast does not in itself violate aliasing or
 * alignment rules. It is likely that there are some or many callers of the GET_* and PUT_* macros which
 * are relying on undefined behavior in creating pointers to higher-precision objects from pointers to
 * char, but this change is sufficient to protect any accesses that go through these macros.
 */
static inline gtm_int8 get_long(const unsigned char *src)
{
	gtm_int8 res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline gtm_uint8 get_ulong(const unsigned char *src)
{
	gtm_uint8 res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline int4 get_int(const unsigned char *src)
{
	int4 res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline uint4 get_uint(const unsigned char *src)
{
	uint4 res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline short get_short(const unsigned char *src)
{
	short res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline unsigned short get_ushort(const unsigned char *src)
{
	unsigned short res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline unsigned char get_char(const unsigned char *src)
{
	unsigned char res;
	memcpy(&res, src, SIZEOF(res));
	return res;
}

static inline gtm_int8 put_long(unsigned char *dst, gtm_int8 val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

static inline gtm_uint8 put_ulong(unsigned char *dst, gtm_uint8 val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

static inline int4 put_int(unsigned char *dst, int4 val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

static inline uint4 put_uint(unsigned char *dst, uint4 val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

static inline short put_short(unsigned char *dst, short val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

static inline unsigned short put_ushort(unsigned char *dst, unsigned short val)
{
	memcpy(dst, &val, SIZEOF(val));
	return val;
}

#define GET_LLONG(X,Y)	((X) = get_long((unsigned char *)(Y)))
#define GET_ULLONG(X,Y)	((X) = get_ulong((unsigned char *)(Y)))
#define GET_LONG(X,Y)	((X) = get_int((unsigned char *)(Y)))
#define GET_ULONG(X,Y)	((X) = get_uint((unsigned char *)(Y)))
#define GET_SHORT(X,Y)	((X) = get_short((unsigned char *)(Y)))
#define GET_USHORT(X,Y)	((X) = get_ushort((unsigned char *)(Y)))
#define GET_CHAR(X,Y)	((X) = get_char((unsigned char *)(Y)))
#define PUT_LLONG(X,Y)	(put_long((unsigned char *)(X), (Y)))
#define PUT_ULLONG(X,Y)	(put_ulong((unsigned char *)(X), (Y)))
#define PUT_LONG(X,Y)	(put_int((unsigned char *)(X), (Y)))
#define PUT_ULONG(X,Y)	(put_uint((unsigned char *)(X), (Y)))
#define PUT_SHORT(X,Y)	(put_short((unsigned char *)(X), (Y)))
#define PUT_USHORT(X,Y)	(put_ushort((unsigned char *)(X), (Y)))
#define PUT_CHAR(X,Y)	(*(unsigned char *)(X) = (Y))
#endif /* COPY_H_INCLUDED */
