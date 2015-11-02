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

#define RC_NSPACE_DSID 768	/* 3 scaled by 256 */
#define RC_NSPACE_DSID_STR "768"
#define RC_DEF_SERV_ID 42
#define RC_DEF_SERV_ID_STR "42"
#define RC_NSPACE_PATH "$gtmdtndbd"
#define RC_NSPACE_GLOB "nspace"
#define RC_NSPACE_GLOB_LEN SIZEOF(RC_NSPACE_GLOB)
#define RC_NSPACE_DSI_SUB "dsi"

/* Location within ^%nspace("dsi",svid,dsi) of filespec */
#define RC_FILESPEC_PIECE 3
/* data delimiter */
#define RC_FILESPEC_DELIM '|'

typedef struct rc_dsid_list_struct {
short				dsid;
char				*fname;
gd_addr				*gda;
struct rc_dsid_list_struct	*next;
}rc_dsid_list;
