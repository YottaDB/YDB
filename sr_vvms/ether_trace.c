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

#include "mdef.h"
#include "nmadef.h"
#include "ddphdr.h"
#include <descrip.h>
#include <iodef.h>
#include <efndef.h>
#include "util.h"
#include "ddp_trace_output.h"


unsigned char packet_header[20];
unsigned char local_buffer[1000];
char *buffer_top = local_buffer;

#define CHECK_STATUS if ((status & 1) == 0) lib$signal(status);
#define CHECK_IOSB if ((status & 1) != 0) status = ether_iosb.result ; CHECK_STATUS;
int ether_trace(void)
{
    int4 status;
    char ether_string[] = "ESA0";
    $DESCRIPTOR(ether_name, ether_string);
    $DESCRIPTOR(output_qual, "OUTPUT");
    int4 ether_channel = 0;
    int i;
    struct
    {
	unsigned short result;
	short length;
	int4 devspec;
    } ether_iosb;
    struct
    {
	short param_id;
	int4 param_value;
    } set_parm_array[] =
    {
	{
	    NMA$C_PCLI_PRM, NMA$C_STATE_ON
	}
    };
    struct dsc$descriptor_s set_parm;

    util_out_open(&output_qual);
    set_parm.dsc$w_length = SIZEOF(set_parm_array);
    set_parm.dsc$a_pointer = set_parm_array;
    set_parm.dsc$b_dtype = 0;
    set_parm.dsc$b_class = 0;
    status = sys$assign (
	&ether_name,
	&ether_channel,
	0,
	0);
    CHECK_STATUS;
    status = sys$qiow (
		 0,
		 ether_channel,
		 IO$_SETMODE | IO$M_CTRL | IO$M_STARTUP,
		 &ether_iosb,
		 0,
		 0,
		 0,
		 &set_parm,
		 0,
		 0,
		 0,
		 0);
    CHECK_IOSB;
    for (i = 0 ; i < 2000 ; i++)
    {
retry:
        status = sys$qiow(EFN$C_ENF,
    		 ether_channel,
    		 IO$_READPBLK,
    		 &ether_iosb,
    		 0,
    		 0,
    		 buffer_top, 1500, 0, 0, packet_header, 0);
        CHECK_IOSB;
	if (packet_header[0] != 0xAA && packet_header[6] != 0xAA)
		goto retry;
/*	util_out_write("Packet", SIZEOF("Packet"));
	ddp_trace_output(packet_header, SIZEOF(packet_header));
	ddp_trace_output(buffer_top, ether_iosb.length);
*/
	ddp_trace_output(buffer_top, 30, DDP_RECV);
    }
    util_out_close();
    return 1;
}
