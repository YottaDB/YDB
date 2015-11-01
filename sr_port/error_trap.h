/****************************************************************
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ERROR_TRAP_H
#define ERROR_TRAP_H

#define IS_ETRAP	(err_act == &dollar_etrap.str)

void		ecode_init(void);
void		ecode_get(int level, mval *result);	/* return $ECODE (if "level" < 0) or $STACK(level,"ECODE") in "result" */
void		ecode_set(int errnum);			/* convert "errnum" to error-string and call ecode_add() */
boolean_t	ecode_add(mstr *str);			/* add "str" to $ECODE */
void		error_return(void);
#ifdef VMS
void		error_return_vms(void);
#endif

typedef	void	(*error_ret_fnptr)(void);

void	get_dollar_stack_info(int level, int mode,  mval *result);
void	get_frame_creation_info(int level, int cur_zlevel, mval *result);
void	get_frame_place_mcode(int level, int mode, int cur_zlevel, mval *result);

#define	DOLLAR_ECODE_MAXINDEX	32	/* maximum of 32 ecodes in $ECODE */
#define	DOLLAR_STACK_MAXINDEX	256	/* maximum of 256 levels will be stored for $STACK(level) */

#define	DOLLAR_ECODE_ALLOC	(1 << 15)	/* 32K chunk memory malloced for $ECODE */
#define	DOLLAR_STACK_ALLOC	(1 << 15)	/* 32K chunk memory malloced for $STACK(level) */

#define	STACK_ZTRAP_EXPLICIT_NULL	-1	/* used to indicate an explicit SET $ZTRAP = "" */

typedef struct dollar_ecode
{
	mstr		ecode_str;
} dollar_ecode_struct;

typedef struct dollar_stack	/* contents of a single $STACK(level) entry */
{
	mstr			mode_str;	/* $STACK(level) */
	dollar_ecode_struct	*ecode_ptr;	/* $STACK(level,"ECODE") */
	mstr			mcode_str;	/* $STACK(level,"MCODE") */
	mstr			place_str;	/* $STACK(level,"PLACE") */
} dollar_stack_struct;

enum stack_mode {
	DOLLAR_STACK_INVALID,
	DOLLAR_STACK_ECODE,	/* $STACK(level,"ECODE") */
	DOLLAR_STACK_PLACE,	/* $STACK(level,"PLACE") */
	DOLLAR_STACK_MCODE,	/* $STACK(level,"MCODE") */
	DOLLAR_STACK_MODE	/* $STACK(level) i.e. how this frame got created */
};

typedef	struct {
	char			*begin;	/* beginning of malloced memory holding the complete $ECODE */
	char			*end;	/* pointer to where next $ECODE can be added */
	char			*top;	/* allocated end of malloced memory holding the complete $ECODE */
	dollar_ecode_struct	*array;	/* array of DOLLAR_ECODE_MAXINDEX dollar_ecode_struct structures */
	uint4			index;	/* current count of number of filled structures in array */
	int4			error_last_ecode; /* last error code number */
	unsigned char		*error_frame_mpc; /* ptr to original mpc in case error_frame's mpc was overwritten to error_ret() */
	unsigned char		*error_frame_ctxt;/* ptr to original ctxt in case error_frame's ctxt was overwritten */
		/* note that error_frame_mpc and error_frame_ctxt are used only if error_frame->mpc != CODE_ADDRESS(ERROR_RTN) */
	unsigned char		*error_last_b_line;	/* ptr to beginning of line where error occurred */
	struct stack_frame_struct *first_ecode_error_frame;	/* "frame_pointer" at the time of adding the first ECODE */
	unsigned char		*error_rtn_addr;	/* CODE_ADDRESS(ERROR_RTN) */
	unsigned char		*error_rtn_ctxt;	/* CONTEXT(ERROR_RTN) */
	error_ret_fnptr		error_return_addr;	/* CODE_ADDRESS(ERROR_RETURN) */
} dollar_ecode_type;

typedef struct {
	char			*begin;		/* beginning of malloced memory holding all $STACK(level) detail */
	char			*end;		/* pointer to where next $STACK(level) detail can be added */
	char			*top;		/* allocated end of malloced memory holding all $STACK(level) detail */
	dollar_stack_struct	*array;		/* array of DOLLAR_STACK_MAXINDEX dollar_stack_struct structures */
	uint4			index;		/* current count of number of filled structures in array */
	boolean_t		incomplete;	/* TRUE if we were not able to fit in all $STACK info */
} dollar_stack_type;

/* reset all $ECODE related variables to correspond to $ECODE = NULL state */
#define	NULLIFY_DOLLAR_ECODE						\
{									\
	GBLREF	dollar_ecode_type	dollar_ecode;			\
	GBLREF	dollar_stack_type	dollar_stack;			\
									\
	dollar_ecode.end = dollar_ecode.begin;				\
	dollar_ecode.index = 0;						\
	dollar_stack.end = dollar_stack.begin;				\
	dollar_stack.index = 0;						\
	dollar_stack.incomplete = FALSE;				\
	dollar_ecode.first_ecode_error_frame = NULL;			\
}

/* nullify "error_frame" and its associated pointer variables */
#define	NULLIFY_ERROR_FRAME				\
{							\
	GBLREF	stack_frame		*error_frame;	\
	GBLREF	dollar_ecode_type	dollar_ecode;	\
							\
	error_frame = NULL;				\
	dollar_ecode.error_frame_mpc = NULL;		\
	dollar_ecode.error_frame_ctxt = NULL;		\
}

/* set "error_frame" and its associated pointer variables to point to "frame_pointer". also reset mpc/ctxt to error_rtn_addr/ctxt */
#define SET_ERROR_FRAME(fp)								\
{											\
	GBLREF	stack_frame		*error_frame;					\
	GBLREF	dollar_ecode_type	dollar_ecode;					\
	GBLREF	unsigned char		*error_frame_save_mpc[DOLLAR_STACK_MAXINDEX];	\
	int				level;						\
											\
	error_frame = fp;								\
	dollar_ecode.error_frame_mpc = fp->mpc;						\
	level = dollar_zlevel() - 1;							\
	if (level < DOLLAR_STACK_MAXINDEX)						\
	{										\
		error_frame_save_mpc[level] = error_frame->mpc;				\
		/* an error at a level can clear higher level saved values */		\
		for (level = level + 1; level < DOLLAR_STACK_MAXINDEX; level++)		\
			error_frame_save_mpc[level] = NULL;				\
	}										\
	dollar_ecode.error_frame_ctxt = fp->ctxt;					\
	error_frame->mpc = dollar_ecode.error_rtn_addr;					\
	error_frame->ctxt = dollar_ecode.error_rtn_ctxt;				\
}

/* invoke the function error_return() if the necessity of error-rethrow is detected */
#define	INVOKE_ERROR_RET_IF_NEEDED										\
{														\
	GBLREF	dollar_ecode_type	dollar_ecode;								\
	GBLREF	stack_frame		*error_frame;								\
														\
	if (NULL != error_frame)										\
	{													\
		if (error_frame == frame_pointer)								\
		{												\
			if (dollar_ecode.index)		/* non-zero implies non-NULL $ECODE */			\
			{	/* this is an error frame and $ECODE is non-NULL during QUIT out of this frame. \
				 * rethrow the error at lower level */						\
				(*dollar_ecode.error_return_addr)();						\
				assert(FALSE);	/* this should not return */					\
			} else											\
			{											\
				assert(FALSE);									\
				NULLIFY_ERROR_FRAME;	/* don't know how we reached here. reset it in PRO */	\
			}											\
		} else if (error_frame < frame_pointer)								\
		{												\
			assert(FALSE);										\
			NULLIFY_ERROR_FRAME;	/* don't know how we reached here. reset it in PRO */		\
		}												\
	}													\
}

#endif /* ERROR_TRAP_H */
