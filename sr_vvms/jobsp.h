/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBSP_H_INCLUDED
#define JOBSP_H_INCLUDED

#define MAX_JOBPAR_LEN		255
#define MAX_FILSPC_LEN		255
#define MAX_PIDSTR_LEN		10
#define MAX_MBXNAM_LEN		16
#define MAX_PRCNAM_LEN		15

#define JP_NO_BASPRI		-1

typedef struct
	{
		int4		lo;
		int4		hi;
	} quadword; /* date_time; */

typedef	struct
	{
		unsigned	unused;
		unsigned	finalsts;
	} pmsg_type;

typedef	struct
	{
		mident_fixed	label;
		int4		offset;
		mident_fixed	routine;
		quadword	schedule;
	} isd_type;

typedef	struct
	{
		unsigned short	status;
		unsigned short	byte_count;
		uint4	pid;
	} mbx_iosb;

typedef enum
{
	jpdt_nul,
	jpdt_num,
	jpdt_str
} jp_datatype;

#define JPDEF(a,b) a
typedef enum
{
#include "jobparams.h"
} jp_type;

#include <descrip.h>

void ojmbxio(int4 func, short chan, mstr *msg, short *iosb, bool now);
unsigned short ojunit_to_mba(char *targ, uint4 n);
uint4 ojmba_to_unit(char *src);
void ojerrcleanup(void);
void ojastread (int expected);
bool ojchkbytcnt(int4 cmaxmsg);
void ojcleanup(void);
bool ojcrembxs(uint4 *punit, struct dsc$descriptor_s *cmbx, int4 cmaxmsg, bool timed);
void ojparams(unsigned char *p, mval *routine, bool *defprcnam, int4 *cmaxmsg, mstr *image,
	mstr *input, mstr *output, mstr *error, struct dsc$descriptor_s *prcnam, int4 *baspri,
	int4 *stsflg, mstr *gbldir, mstr *startup, struct dsc$descriptor_s *logfile, mstr *deffs,
	quadword *schedule);
void ojsetattn(int msg);
void ojtmrinit(int4 *timeout);
void ojdefbaspri(int4 *baspri);
void ojdefdeffs(mstr *deffs);
void ojdefimage(mstr *image);
void ojdefprcnam(struct dsc$descriptor_s *prcnam);
void ojtmrrtn(void);
unsigned short ojhex_to_str(uint4 s, char *t);

#endif /* JOBSP_H_INCLUDED */
