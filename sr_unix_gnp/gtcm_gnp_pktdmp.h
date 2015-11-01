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
#ifndef GTCM_GNP_PKTDMP_H_INCLUDED
#define GTCM_GNP_PKTDMP_H_INCLUDED

void gtcm_gnp_cpktdmp(FILE *fp, struct CLB *lnk, int sta, unsigned char *buf, size_t len, char *msg);
void gtcm_gnp_pktdmp(FILE *fp, struct CLB *lnk, int sta, unsigned char *buf, size_t len, char *msg);

#endif
