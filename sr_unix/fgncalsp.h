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

#ifndef __FGNCALSP_H__
#define  __FGNCALSP_H__

/* fgncalsp.h - UNIX foreign calls (d &package.label) */

#define MAXIMUM_PARAMETERS	32	/* maximum number of parameters allowed to be passed between C and M */
#define MAX_NAME_LENGTH		255	/* maximum length of file name */
#define PACKAGE_ENV_PREFIX	"GTMXC"	/* prefix for environemnt variable containing
					   external call table name */
#define CALLIN_ENV_NAME		"GTMCI"	/* call-in table environment variable */
#define MAX_ERRSTR_LEN		1024	/* maximum length of the error string returned
					   by dlerror(). Couldn't find any system
					   defined length, 1024 is just arbitrary */
#define MAX_TABLINE_LEN		1024	/* maximum length of a line estimated to be sufficient
					   to specify MAXIMUM_PARAMETERS parameters in the
					   callin/xcall table */

#define	UNKNOWN_SYSERR 		"unknown system error"
#define COPY_DLLERR_MSG(err_ptr, err_buf)					\
{										\
	int len = 0;								\
										\
	if ((err_ptr = dlerror()) != NULL)					\
	{									\
		len = real_len(SIZEOF(err_buf) - 1, (uchar_ptr_t)err_ptr);	\
		strncpy(err_buf, err_ptr, len);					\
		err_buf[len] = '\0';						\
	} else									\
	{	/* Ensure we will not overrun err_buf limits */			\
		assert(ARRAYSIZE(UNKNOWN_SYSERR) < ARRAYSIZE(err_buf));		\
		STRCPY(err_buf, UNKNOWN_SYSERR);				\
	}									\
}

typedef int4	(*fgnfnc)();
typedef void	(*clnupfptr)();

struct extcall_string
{
#ifdef __osf__
	int	len;
#else
	long	len;
#endif
	char	*addr;
};

/* A chain of packages, each package has a list of "entries", that is external routine entry points.  */

struct extcall_package_list
{
	struct extcall_package_list	*next_package;
	struct extcall_entry_list	*first_entry;
	void_ptr_t			package_handle;
	mstr				package_name;
	clnupfptr			package_clnup_rtn;
};

enum xc_types
{
	xc_notfound,
	xc_void,
	xc_status,
	xc_int,
	xc_uint,
	xc_long,
	xc_ulong,
	xc_float,
	xc_double,
	xc_int_star,
	xc_uint_star,
	xc_long_star,
	xc_ulong_star,
	xc_string_star,
	xc_float_star,
	xc_char_star,
	xc_char_starstar,
	xc_double_star,
	xc_pointertofunc,
	xc_pointertofunc_star
};

enum callintogtm_fncs {
	xc_hiber_start,
	xc_hiber_start_any,
	xc_start_timer,
	xc_cancel_timer,
	xc_gtm_malloc,
	xc_gtm_free,
	xc_unknown_function
};

/* There is one of these for each external routine.  Each is owned by a package.  */
struct extcall_entry_list
{
	struct extcall_entry_list	*next_entry;
	enum xc_types			return_type;	/* function return value */
	int				ret_pre_alloc_val; /* amount of space to be pre-allocated for the return type */
	uint4				input_mask;	/* is it an input parameter lsb = 1st parm */
	uint4				output_mask;	/* is it an output parameter lsb = 1st parm */
	int				parmblk_size;	/* size in bytes of parameter block to be allocated for call*/
	int				argcnt;		/* number of arguments */
	enum xc_types			*parms;		/* pointer to parameter array */
	int				*param_pre_alloc_size; /* amount of space to be pre-allocated for the parameters */
	fgnfnc				fcn;		/* address of runtime routine */
	mstr				entry_name;	/* name of M entryref */
	mstr				call_name;	/* corresponding name of C function */
};

/* A list of entries in the call-in table each indicating the signature of
   a call-in routine */
typedef struct callin_entry_list
{
	mstr			label_ref;	/* labelref name of M routine */
	mstr			call_name;	/* corresponding name of C function */
	uint4			input_mask;	/* input parameter? LSB = 1st parm */
	uint4			output_mask;	/* output parameter? LSB = 1st parm */
	unsigned short		argcnt;		/* number of arguments */
	enum xc_types		return_type;
	enum xc_types		*parms;		/* parameter types */
	struct callin_entry_list	*next_entry;
} callin_entry_list;

/* parameter block that ci_restart uses to pass arguments to M routine */
typedef struct parmblk_struct
{
	void	(*ci_rtn)(void);
	int4    argcnt;
	void    *rtnaddr, *labaddr, *retaddr;
	int4    mask;
	lv_val	*args[MAXIMUM_PARAMETERS];
} parmblk_struct;

#include "rtnhdr.h"

/* function prototypes */
void_ptr_t	fgn_getpak(char *pak_name, int msgtype);
fgnfnc 		fgn_getrtn(void_ptr_t pak_handle, mstr *sym_name, int msgtype);
void		fgn_closepak(void_ptr_t pak_handle, int msgtype);
int 		fgncal_getint(mstr *inp);
int 		fgncal_read_args(mstr *inp);
void 		fgncal_getstr(mstr *inp, mstr *str);
void 		fgncal_lkbind(mstr *inp);
void 		fgn_glopref(mval *v);
struct extcall_package_list 	*exttab_parse (mval *package);
callin_entry_list		*citab_parse (void);

#endif
