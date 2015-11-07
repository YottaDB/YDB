/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ladef.h - license administration structures */
#include "mdef.h"
#include <rms.h>

#define DBID 0x0000FFFF		/* data base file ID		*/
#define LADB "GTM$LADB"		/* license adm. data base name  */
#define LMDB "GTM$CNFDB"	/* license man. data base name 	*/
#define LAHP "GTM$HELP:LA"	/* license adm. help file	*/
#define LMHP "GTM$HELP:LMU"	/* license man. help file	*/
#define LMLK "GTM$LM"		/* license n. lock resource	*/
#define LAFILE "GTM$ROOT:[CUS]LA.DAT"	/* default adm. db file */
#define LMFILE "GTM$DIST:GTC.CNF"	/* default man. db file	*/
#define BLKS 512		/* db file record size		*/
#define ALOC  32		/* initial allocation in blocks */

#define HWLEN 11		/* moved here from obsolete vaxmodel.h */

#define FNAM  64		/* max file name size		*/
#define PROD  16		/* max lenght of product	*/
#define VERS  16		/* max length of version	*/
#define ADDR  64		/* max addr line length		*/
#define NSYS  32		/* max number of systems	*/
#define NCRY  10		/* ecrypt. func.# < NCRY	*/
#define CSLN  21		/* expanded checksum length 	*/
#define PBUF  (SIZEOF(pak)+8*NSYS+9*ADDR)  /* max size of pak   */

typedef int4 date[2] ;		/* date/time quadword		*/

typedef struct
{
	int4  id ; 		/* id == DBID		      	*/
	int4  N  ;		/* number of paks in the file 	*/
	int4  len  ;		/* file length in bytes       	*/
	int4  lastid ;		/* last license ID
	int4  unused[12] ;	/* not used 		      	*/
} la_prolog ;			/* prolog to license adm file 	*/

typedef struct
{
	short n ;            	/* encryption function number 	*/
	char cs[18] ;        	/* check sum                  	*/
	short l[14] ;         	/* offsets within pak         	*/
} phead ;

typedef struct
{
	date std ;		/* data/time pak created      	*/
	char oid[16] ;		/* operator "USERNAME"        	*/
} pfldr ;

typedef struct
{
	short L ;		/* number of systems   		*/
	char nam[PROD] ;        /* product name        		*/
	char ver[VERS] ;        /* version name        		*/
	short x	;		/* license value      		*/
	date t0 ;            	/* date available      		*/
	date t1 ;		/* date expires        		*/
	int4 lid ;		/* license ID	       		*/
} pdata ;

typedef struct
{
	phead ph ; 		/* pak header                	*/
 	pfldr pf ;		/* pak folder                	*/
	pdata pd ;		/* license data              	*/
} pak ;				/* pak structure is followed 	*/
                                /* by variable length arrays 	*/
				/* sid,nid,adr,com           	*/


typedef enum			/* variable type		*/
{
	str,sho,lon,hex,dat,lst,csm,mdl,list,emp
} vtyp ;

typedef enum			/* variables identification	*/
{
	v_n,v_cs,v_l,v_std,v_oid,v_L,v_nam,v_ver,v_x,v_t0,v_t1,v_lid,v_sid,v_nid,v_adr,v_com,eovar
} varid ;

#define VARTYP(t) vtyp const t[17]= \
{	sho,str ,sho,dat  ,str  ,sho,str  ,str  ,sho,dat ,dat ,lon  ,lst  ,lst  ,str  ,str  ,emp	}

bool la_getcli(int v_arr[], pak *pak_ptr);
bool la_match(pak *pak_ptr0, pak *pak_ptr1, int v_arr[]);
bool la_nam2mdl(int4 *mdl, short w, char nam[]);
bool la_uniqlid(char *h, int4 lid);
bool la_validate(int4 code, char *val);
char * la_getdb(char *fn);
char *la_getdb(char *fn);
short la_getstr(uint4 kid, int4 code, char *res, int lo, int hi);
short la_mdl2nam(char nam[], int4 mdl);
void la_convert(int4 bcs[], char *cs);
void la_edit(uint4 kid, char *h, pak *p);
void la_freedb(char *h);
void la_getdat(uint4 kid, int4 code, date *date_ptr, uint4 lo, uint4 hi);
void la_getnum(uint4 kid, int4 code, int4 *num_ptr, int4 lo, int4 hi);
void la_initpak(int4 llid, pak *p);
void la_listpak(char *q);
void la_putdb(char *fn, char *h);
void la_putfldr(pfldr *pf);
void la_puthead(pak *p);
 void la_putmsgs(int c);
void la_putmsgs(int c);
void la_putmsgu(int4 c, int4 fao[], short n);
void la_showpak(char *q);
void la_writepak(struct FAB *f, pak *p);
int la_create (void);
int la_maint (void);
int la_store(void);

#include <descrip.h>

int4 lp_acquire(pak *p, int4 lval, int4 lid, int4 *lkid);
int4 lp_confirm(int4 lid, uint4 lkid);
int4 lp_licensed(char *h, struct dsc$descriptor *prd, struct dsc$descriptor *ver,
	int4 mdl, int4 nid,int4 *lid, int4 *x, int4 *days, pak *p);
int4 lp_licensed(char *h, struct dsc$descriptor *prd, struct dsc$descriptor *ver, int4 mdl, int4 nid,
	int4 *lid, int4 *x, int4 *days, pak *p);
uint4 lp_id(uint4 *lkid);

