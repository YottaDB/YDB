/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JNL_TYPEDEF_H_INCLUDED
#define JNL_TYPEDEF_H_INCLUDED

#define NA 			0
#define SETREC			0x00000001
#define KILLREC			0x00000002
#define ZKILLREC		0x00000004
#define ZTWORMREC		0x00000008
#define ZTRIGREC		0x00000800

#define SET_KILL_ZKILL_MASK			(SETREC | KILLREC | ZKILLREC)
#define SET_KILL_ZKILL_ZTWORM_MASK		(SETREC | KILLREC | ZKILLREC | ZTWORMREC)
#define SET_KILL_ZKILL_ZTWORM_ZTRIG_MASK	(SETREC | KILLREC | ZKILLREC | ZTWORMREC | ZTRIGREC)

#define TUPDREC			0x00000010
#define UUPDREC			0x00000020
#define TCOMREC			0x00000040
#define TPREC_MASK		(TUPDREC | UUPDREC | TCOMREC)

#define FUPDREC			0x00000100
#define GUPDREC			0x00000200
#define ZTCOMREC		0x00000400
#define ZTPREC_MASK		(FUPDREC | GUPDREC | ZTCOMREC)

#define FENCE_MASK		(TPREC_MASK | ZTPREC_MASK)

/* the following LITREFs are needed by the below macros */
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_fixed_size[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];

#define IS_VALID_RECTYPES_RANGE(rectype)	((JRT_BAD < rectype) && (JRT_RECTYPES > rectype))
#define IS_REPLICATED(rectype)			(jrt_is_replicated[rectype])
#define IS_FIXED_SIZE(rectype)			(jrt_fixed_size[rectype])
#define IS_SET(rectype)				(jrt_update[rectype] & SETREC)
#define IS_KILL(rectype)			(jrt_update[rectype] & KILLREC)
#define IS_ZKILL(rectype)			(jrt_update[rectype] & ZKILLREC)
#define IS_ZTWORM(rectype)			(jrt_update[rectype] & ZTWORMREC)
#define IS_ZTRIG(rectype)			(jrt_update[rectype] & ZTRIGREC)
#define IS_KILL_ZKILL(rectype)			(jrt_update[rectype] & (KILLREC | ZKILLREC))
#define IS_KILL_ZKILL_ZTRIG(rectype)		(jrt_update[rectype] & (KILLREC | ZKILLREC | ZTRIGREC))
#define IS_SET_KILL_ZKILL_ZTRIG(rectype)	(jrt_update[rectype] & (SET_KILL_ZKILL_MASK | ZTRIGREC))
#define IS_SET_KILL_ZKILL_ZTWORM(rectype)	(jrt_update[rectype] & SET_KILL_ZKILL_ZTWORM_MASK)
#define IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype)	(jrt_update[rectype] & SET_KILL_ZKILL_ZTWORM_ZTRIG_MASK)
#define IS_FENCED(rectype)			(jrt_update[rectype] & FENCE_MASK)
#define IS_TP(rectype)				(jrt_update[rectype] & TPREC_MASK)
#define IS_ZTP(rectype)				(jrt_update[rectype] & ZTPREC_MASK)
#define IS_COM(rectype)				(jrt_update[rectype] & (TCOMREC | ZTCOMREC))
#define IS_FUPD(rectype)			(jrt_update[rectype] & FUPDREC)
#define IS_GUPD(rectype)			(jrt_update[rectype] & GUPDREC)
#define IS_TUPD(rectype)			(jrt_update[rectype] & TUPDREC)
#define IS_UUPD(rectype)			(jrt_update[rectype] & UUPDREC)
#define IS_FUPD_TUPD(rectype)			(jrt_update[rectype] & (FUPDREC | TUPDREC))
#define IS_GUPD_UUPD(rectype)			(jrt_update[rectype] & (GUPDREC | UUPDREC))

#ifdef GTM_TRIGGER
# define IS_VALID_JRECTYPE(rectype)	IS_VALID_RECTYPES_RANGE(rectype)
#else /* On trigger non-supporting platforms, it is an error if a ZTWORM or ZTRIG rectype is seen. */
# define IS_VALID_JRECTYPE(rectype)	(IS_VALID_RECTYPES_RANGE(rectype) && !IS_ZTWORM(rectype) && !IS_ZTRIG(rectype))
#endif

#define GET_REC_FENCE_TYPE(rectype)		(!IS_FENCED(rectype)) ? NOFENCE : (IS_TP(rectype)) ? TPFENCE : ZTPFENCE
#define REC_HAS_TOKEN_SEQ(rectype)		(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || IS_COM(rectype)		\
							|| (JRT_EPOCH == (rectype)) || (JRT_EOF == (rectype))		\
							|| (JRT_NULL == (rectype)))

#endif /* JNL_TYPEDEF_H_INCLUDED */
