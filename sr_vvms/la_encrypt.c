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

/* la_encrypt.c : for given encryption function number, and an input sequence of bytes,
		  the program computes the checksum of the sequence.
   used in      : la_create.c,lp_licensed.c,lm_verify
*/
#include "mdef.h"
#include <ssdef.h>
#include "ladef.h"
#include <descrip.h>
#include "la_encrypt.h"

bool la_encrypt (
short 	n  ,				/* encryption function number	*/
char 	*q , 				/* input sequence 		*/
int 	len,				/* sequence length 		*/
uint4 	bcs[])				/* result, binary form 		*/
{
	uint4	poly0[10] = { 0xEDB88320,0xA001A001,0x00008408,0x00000000,0xA001A001,1,1,1,1,1} ;
	int4		init0[10] = { 0xFFFFFFFF,0x00000000,0x0000FFFF,0xFFFFFFFF,0x00000000,0,0,0,0,0} ;
	uint4	poly1[10] = { 0xA001A001,0x00008408,0xEDB88320,0xEDB88320,0x0000A001,1,1,1,1,1} ;
	int4		init1[10] = { 0x00000000,0x0000FFFF,0xFFFFFFFF,0xFFFFFFFF,0x00000000,0,0,0,0,0} ;
	bool		status ;
	int4 		crctbl[16] ;
	$DESCRIPTOR	(dq,q) ;

	dq.dsc$w_length = len ;
	if (n>=5)
	{
		status = FALSE;
	}
	else
	{
		lib$crc_table(poly0+n,crctbl) ;
		bcs[0] = lib$crc(crctbl,init0+n,&dq) ;

		lib$crc_table(poly1+n,crctbl) ;
		bcs[1] = lib$crc(crctbl,init1+n,&dq) ;
		status = TRUE ;
	}
	return status ;
}
