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

#define MAX_COLLTYPE	255
#define MIN_COLLTYPE	0

#ifdef UNIX					/* environment variable syntax is OS dependent */
#	define	CT_PREFIX	"$gtm_collate_"
#	define LCT_PREFIX	"$gtm_local_collate"
#elif defined VMS
#	define	CT_PREFIX	"GTM_COLLATE_"
#	define LCT_PREFIX	"GTM_LOCAL_COLLATE"
#else
#error UNSUPPORTED PLATFORM
#endif

typedef struct collseq_struct {
	struct collseq_struct	*flink;
	int			act;
	int4			(*xform)();
	int4			(*xback)();
	int4			(*version)();
	int4			(*verify)();
} collseq;

boolean_t map_collseq(mstr *fspec, collseq *ret_collseq);
collseq *ready_collseq(int act);
int4 do_verify(collseq *csp, unsigned char type, unsigned char ver);
int find_local_colltype(void);
void act_in_gvt(void);

