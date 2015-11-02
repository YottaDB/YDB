/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "la_encrypt.h"


static void la_encrypt_table (uint4 poly, uint4 tbl[]);
static uint4 la_encrypt_value (
uint4		tbl[],			/* polynomial coefficients 		*/
uint4		cs,			/* checksum initial value		*/
unsigned char	*c,			/* string to compute the check sum for	*/
int		len);			/* string length 			*/

bool la_encrypt (
short 		n  ,				/* encryption function number	*/
char	 	*q , 				/* input sequence 		*/
int 		len,				/* sequence length 		*/
uint4 		bcs[])				/* result, binary form 		*/
{
	static uint4	poly0[10] = {0xEDB88320, 0xA001A001, 0x00008408, 0x00000000, 0xA001A001, 1, 1, 1, 1, 1};
	static uint4	init0[10] = {0xFFFFFFFF, 0x00000000, 0x0000FFFF, 0xFFFFFFFF, 0x00000000, 0, 0, 0, 0, 0};
	static uint4	poly1[10] = {0xA001A001, 0x00008408, 0xEDB88320, 0xEDB88320, 0x0000A001, 1, 1, 1, 1, 1};
	static uint4	init1[10] = {0x00000000, 0x0000FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0, 0, 0, 0, 0};
	uint4 		crctbl[16];
	if (n<5)
	{
		la_encrypt_table(poly0[n],crctbl) ;
		bcs[0] = la_encrypt_value(crctbl,init0[n],(uchar_ptr_t)q,len) ;
		la_encrypt_table(poly1[n],crctbl) ;
		bcs[1] = la_encrypt_value(crctbl,init1[n],(uchar_ptr_t)q,len) ;
	}
	return n<5 ;
}


static void la_encrypt_table (uint4 poly, uint4 tbl[])
{
	uint4	k, t, x;
	int	i;
	for ( k= 0 ; k!=16 ; k++ )
	{
		t= k ;
		for ( i= 0 ; i!=4 ; i++ )
		{
			x= t & 1 ;
			t= t>>1 ;
			if (x==1)  t ^= poly ;
		}
		tbl[k]= t ;
	}
}


static uint4 la_encrypt_value (
uint4		tbl[],			/* polynomial coefficients 		*/
uint4		cs,			/* checksum initial value		*/
unsigned char	*c,			/* string to compute the check sum for	*/
int		len)			/* string length 			*/
{
	while ( len!=0 )
	{
		cs ^= (int4)*c ;   	/* least signif. byte of cs differred with *c  */
		cs = ( cs >> 4 ) ^ tbl[cs & 0xF] ;
		cs = ( cs >> 4 ) ^ tbl[cs & 0xF] ;
		c++ ; len-- ;
	}
	return cs ;
}
