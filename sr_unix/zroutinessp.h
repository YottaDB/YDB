/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZROUTINESSP_H_INCLUDED
#define ZROUTINESSP_H_INCLUDED

/* zro_ent fields are interpreted based on entry type:
 * ZRO_TYPE_COUNT --> count indicates number of entries following
 *  	this entry representing a list of source or object directories.
 * ZRO_TYPE_OBJECT, ZRO_TYPE_SOURCE --> str stores the path of the appropriate
 * 	object/source directory.
 * ZRO_TYPE_OBJLIB --> str and shrlib store the shared library file name and
 * 	its handle respectively. (the shrsym field is used as a place holder
 * 	to keep dlsym() value available for incr_link during each $ZROUTINES search). */
typedef	struct zro_ent_type
{
	uint4		type;
	uint4		count;
	mstr		str;
	void_ptr_t	shrlib; /* used only on those platforms that generate shared images */
	void		*shrsym; /* used only on those platforms that generate shared images */
} zro_ent;

#define ZRO_EOL 	0
#define ZRO_IDN 	1
#define ZRO_DEL 	' '
#define ZRO_LBR 	'('
#define ZRO_RBR 	')'

int zro_gettok(char **lp, char *top, mstr *tok);
void zsrch_clr(int indx);
void zro_search (mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir, boolean_t skip);

#endif /* ZROUTINESSP_H_INCLUDED */
