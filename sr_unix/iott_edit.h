/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IOTT_EDIT_H
#define IOTT_EDIT_H

#ifdef KEEP_zOS_EBCDIC
LITREF	unsigned char	ebcdic_lower_to_upper_table[];
LITREF	unsigned char	e2a[];
#	define	INPUT_CHAR	asc_inchar
#	define  GETASCII(OUTPARM, INPARM)	{OUTPARM = e2a[INPARM];}
#	define	NATIVE_CVT2UPPER(OUTPARM, INPARM)	{OUTPARM = ebcdic_lower_to_upper_table[INPARM];}
#else
#	define	INPUT_CHAR	inchar
#	define	GETASCII(OUTPARM, INPARM)
#	define	NATIVE_CVT2UPPER(OUTPARM, INPARM)	{OUTPARM = lower_to_upper_table[INPARM];}
#endif

#define GET_OFF(OFFSET)	(utf8_active ? buffer_32_start[OFFSET] : buffer_start[OFFSET])
#define STORE_OFF(CHAR,OFFSET)	{if (utf8_active)							\
    					buffer_32_start[OFFSET] = CHAR;					\
				 else									\
					buffer_start[OFFSET] = (unsigned char)CHAR;}
#define BUFF_ADDR(OFFSET)	(utf8_active ? (char *)&buffer_32_start[OFFSET] : (char *)&buffer_start[OFFSET])
#define BUFF_CHAR_SIZE		(utf8_active ? SIZEOF(wint_t) : SIZEOF(unsigned char))
#define SET_BUFF(OFFSET,CHAR,LENGTH)	{if (utf8_active)						\
					 {	int off = OFFSET;					\
						 for ( ; off < (OFFSET + (LENGTH)); off++)		\
						 	buffer_32_start[off] = CHAR;			\
					 }								\
					 else								\
						memset(&buffer_start[OFFSET], CHAR, LENGTH);}
#define MOVE_BUFF(OFFSET,SOURCE,LENGTH)	{if (utf8_active)						\
					   memmove(&buffer_32_start[OFFSET], SOURCE, (LENGTH) * SIZEOF(wint_t));	\
					 else								\
					   memmove(&buffer_start[OFFSET], SOURCE, LENGTH);}

#define	IOTT_COMBINED_CHAR_CHECK								\
{												\
	int	tmp_instr;									\
												\
	/* It is possible that a combining character is the character under the cursor in which	\
	 * case we need to erase it by going back "instr" as much as needed until we find	\
	 * a non-zero width representation and from that point on repaint the display.		\
	 */											\
	dx_next = compute_dx(BUFF_ADDR(0), instr + 1, ioptr_width, dx_start);			\
	if (dx_next == dx_instr)								\
	{	/* 0-width character (i.e. combining character). handle specially */		\
		for (dx_prev = dx_instr, tmp_instr = instr; (tmp_instr != 0); )			\
		{										\
			tmp_instr--;								\
			dx_prev = compute_dx(BUFF_ADDR(0), tmp_instr, ioptr_width, dx_start);	\
			if (dx_prev != dx_next)							\
				break;								\
		}										\
		delchar_width = dx_instr - dx_prev;						\
		if (delchar_width)								\
		{										\
			move_cursor_left(dx, delchar_width);					\
			dx = (dx - delchar_width + ioptr_width) % ioptr_width;			\
			write_str(BUFF_ADDR(tmp_instr), instr - tmp_instr, dx, FALSE, FALSE);	\
			move_cursor_right(dx, delchar_width);					\
			dx = (dx + delchar_width + ioptr_width) % ioptr_width;			\
		}										\
	}											\
}

int	iott_write_raw(int fildes, void *str832, unsigned int len);
int	write_str(void *str832, unsigned int len, unsigned int start_x, boolean_t move, boolean_t multibyte);
int	write_str_spaces(unsigned int len, unsigned int start_x, boolean_t move);
int	move_cursor_left (int col, int num_cols);
int	move_cursor_right (int col, int num_cols);
int	write_loop(int fildes, unsigned char *str, int num_times);
int	move_cursor(int fildes, int num_up, int num_left);
int 	compute_dx(void *str832, unsigned int index, unsigned int width, unsigned int dx_start);

#endif
