/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZROUTINESSP_H_INCLUDED
#define ZROUTINESSP_H_INCLUDED

/* Parse token types returned by zro_gettok() used when parsing $ZROUTINES values */
#define ZRO_EOL 	0	/* End of line */
#define ZRO_IDN 	1	/* Identifier/name - directory or file name */
#define ZRO_DEL 	' '	/* Delimiter (space) */
#define ZRO_LBR 	'('	/* Left parenthesis denoting source directory list start */
#define ZRO_RBR 	')'	/* Right parenthesis denoting source directory list end */
#define ZRO_ALF		'*'	/* Auto-relink flag/indicator */

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
	mstr		str;			/* Path name; str.addr is malloc'd TODO: why malloc'd?*/
	void_ptr_t	shrlib; 		/* Result of dlopen(), if a shared library */
	void_ptr_t	shrsym; 		/* Placeholder for result of fgn_getrtn(), which we pass from zro_search() to
						 * incr_link().
						 */
	void_ptr_t	relinkctl_sgmaddr;	/* Shared memory control structure associated with this $ZRO entry */
} zro_ent;

int zro_gettok(char **lp, char *top, mstr *tok);
void zsrch_clr(int indx);
void zro_search(mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir, boolean_t skip);

#endif /* ZROUTINESSP_H_INCLUDED */
