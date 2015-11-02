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

#ifndef ZSHOW_H
#define ZSHOW_H

#define ZSHOW_DEVICE	 	1
#define ZSHOW_GLOBAL		2
#define ZSHOW_LOCAL		3
#define ZSHOW_BUFF_ONLY 	4
#define ZSHOW_NOPARM		-1
#define ZSHOW_ALL		"IVBDLGSC"

#define CLEANUP_ZSHOW_BUFF				\
{							\
	GBLREF char	*zwr_output_buff;		\
	if (NULL != zwr_output_buff)			\
	{						\
		free(zwr_output_buff);			\
		zwr_output_buff = NULL;			\
	}						\
}

#define	FIRST_LINE_OF_ZSHOW_OUTPUT(out)	(('G' != out->code) && ('g' != out->code)&& ('L' != out->code)&& ('l' != out->code) \
						? (1 != out->line_num) : (0 != out->line_num))

typedef struct
{
	struct lv_val_struct	*lvar;	/* local variable to output to			*/
	struct lv_val_struct	*child;	/* output variable with function subscript added */
} zs_lv_struct;

typedef struct
{
	int		end;		/* gv_currkey->end for global output variable	*/
	int		prev;		/* gv_currkey->prev 				*/
} zs_gv_struct;

typedef struct zshow_out_struct
{
	char		type;		/* device, local variable or global variable				*/
	char		code;		/* function = "BDSW"							*/
	char		curr_code;	/* code from previous write						*/
	int		size;		/* total size of the output buffer					*/
	char		*buff;		/* output buffer							*/
	char		*ptr;		/* end of current output line in output buffer				*/
	int		len;		/* UTF-8 character length in the current buffer(ZSHOW_DEVICE)
					   or maximum length of global output record (ZSHOW_GLOBAL) 		*/
	int		displen;	/* Display length of the current buffer(ZSHOW_DEVICE) unused otherwise	*/
	int		line_num;	/* index for output variable starts at one				*/
	boolean_t	flush;		/* flush the buffer							*/
	union
	{
		zs_lv_struct	lv;
		zs_gv_struct	gv;
	} out_var;
} zshow_out;

#include "mlkdef.h"

#define QUOTE			"\""
#define QUOTE_CONCAT		"\"_"
#define COMMA 			","
#define CLOSE_PAREN		")"

#define DOLLARCH 		"$C("
#define QUOTE_DCH 		"\"_$C("
#define CLOSE_PAREN_QUOTE 	")_\""
#define CLOSE_PAREN_DOLLARCH 	")_$C("

#define DOLLARZCH 		"$ZCH("
#define QUOTE_DZCH 		"\"_$ZCH("
#define CLOSE_PAREN_DOLLARZCH 	")_$ZCH("

void		zshow_stack(zshow_out *output);
void		zshow_devices(zshow_out *output);
void		zshow_format_lock(zshow_out *output, mlk_pvtblk *temp);
void		zshow_locks(zshow_out *output);
void		zshow_output(zshow_out *out, const mstr *str);
void		zshow_svn(zshow_out *output, int svn);
void		zshow_zbreaks(zshow_out *output);
void		zshow_zcalls(zshow_out *output);
void		zshow_gvstats(zshow_out *output);
void		zshow_zwrite(zshow_out *output);
boolean_t	zwr2format(mstr *src, mstr *des);
int		zwrkeyvallen(char* ptr, int len, char **val_off, int *val_len, int *val_off1, int *val_len1);
int		format2zwr(sm_uc_ptr_t src, int src_len, unsigned char *des, int *des_len);
void		mval_write(zshow_out *output, mval *v, boolean_t flush);
void		mval_nongraphic(zshow_out *output, char *cp, int len, int num);
void		gvzwr_fini(zshow_out *out, int pat);
void		lvzwr_fini(zshow_out *out, int t);

#endif
