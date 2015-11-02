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

#ifndef GTMIMAGENAME_DEF
#define GTMIMAGENAME_DEF

typedef struct
{
	char	*imageName;
	int	imageNameLen;
} gtmImageName;

enum gtmImageTypes
{
#define IMAGE_TABLE_ENTRY(A,B)	A,
#include "gtmimagetable.h"
#undef IMAGE_TABLE_ENTRY
	n_image_types
};

#define GTMIMAGENAMETXT(x) gtmImageNames[x].imageNameLen, gtmImageNames[x].imageName

GBLREF	enum gtmImageTypes	image_type;	/* needed by IS_MUMPS_IMAGE and IS_GTM_IMAGE macros */
GBLREF	boolean_t		run_time;	/* needed by IS_MCODE_RUNNING macro */

#define IS_MCODE_RUNNING	(run_time)

#define IS_DSE_IMAGE			(DSE_IMAGE == image_type)
#define IS_GTCM_GNP_SERVER_IMAGE	(GTCM_GNP_SERVER_IMAGE == image_type)
#define IS_GTMSECSHR_IMAGE		(GTMSECSHR_IMAGE == image_type)
#define IS_GTM_IMAGE			IS_MUMPS_IMAGE
#define IS_GTM_SVC_DAL_IMAGE		(GTM_SVC_DAL_IMAGE == image_type)
#define IS_LKE_IMAGE			(LKE_IMAGE == image_type)
#define IS_MUMPS_IMAGE			(GTM_IMAGE == image_type)
#define IS_MUPIP_IMAGE			(MUPIP_IMAGE == image_type)
#define	IS_VALID_IMAGE			(INVALID_IMAGE != image_type)

#endif
