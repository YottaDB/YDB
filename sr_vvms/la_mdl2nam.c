/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* la_mdl2nam.c : hardware model converted to hardware name
   used in      : la_edit.c, lm_edit.c, la_listpak.c, lm_listpak.c
*/
#include "mdef.h"
#include "ladef.h"

#define HWSIZ 185
typedef struct
{
	char	nam[HWLEN];
} hw_model;

readonly hw_model hw_name[HWSIZ] =
	{ "\0", "V780", "V782", "V750", "V730", "V785", "VUV1", "VWS1", "VUV2", "VWS2", "VWSD", "V8600", "V8650", "V8200", "V8300",
	"V8530", "V8550", "V8700", "V8800", "VWS2000", "VUV2000", "VWSD2000", "V009", "V8250", "V8350", "V3600", "V3600W",
	"V3600D", "V6210", "V3820", "V3520L", "V8840", "V9RR", "VUV2_S", "VUV2_J", "VWS2_T", "VWS2_J", "VWSD_T", "VWSD_J",
	"VUV2000_S", "VUV2000_J", "VWS2000_T", "VWS2000_J", "VWSD2000_T", "VWSD2000_J", "V3600_S", "V3600_J", "V3600W_T",
	"V3600W_J", "V3600D_T", "V3600D_J", "V3820_S", "V3820_J", "V3820L_T", "V3520L_J", "V8250L", "V8250L_J", "VCV", "VCVWS",
	"VCVWSD", "VCV_S", "VCV_J", "VCVWS_T", "VCVWS_J", "VCVWSD_T", "VCVWSD_J", "V8500", "V8370", "V8650P", "V6220", "V6230",
	"V6240", "V6250", "V6260", "V6270", "V6280", "V6215", "V6225", "V6235", "V6245", "V6255", "V6265", "V6275", "V6285",
	"V8810", "V8820", "V8830", "V3400", "V3400W", "V3400D", "V3400_S", "V3400_J", "V3400W_T", "V3400W_J", "V3400D_T",
	"V3400D_J", "VUV2000_O", "VWS2000_O", "VWSD2000_O", "VWSK2000", "V6210_S", "V6220_S", "V6230_S", "V6240_S", "V6250_S",
	"V6260_S", "V6270_S", "V6280_S", "V6310_S", "V6320_S", "V6330_S", "V6340_S", "V6350_S", "V6360_S", "V6370_S", "V6380_S",
	"V6200_J", "V6300_J", "V3900", "V3900_S", "V3900D", "V3900D_T", "V3900_J", "V3900D_J", "V2000A", "V2000A_S", "V2000AW",
	"V2000AD", "V2000AW_T", "V2000AD_T", "V2000A_J", "V2000AW_J", "V2000AD_J", "V3840", "V3840_S", "V3540L", "V3840L_T",
	"V3860", "V3860_S", "V3560L", "V3860L_T", "V3880", "V3880_S", "V3580L", "V3880L_T", "V38A0", "V38A0_S", "VPV", "VPVWS",
	"VPVWSD", "VPV_S", "VPV_J", "VPVWS_T", "VPVWS_J", "VPVWSD_T", "VPVWSD_J", "VTM", "VTM_S", "VTM_J", "V9RR10_T", "V9RR20_T",
	"V9RR30_T", "V9RR40_T", "V9RR50_T", "V9RR60_T", "V9RR70_T", "V9RR80_T", "V9RR10_S", "V9RR20_S", "V9RR30_S", "V9RR40_S",
	"V9RR50_S", "V9RR60_S", "V9RR70_S", "V9RR80_S", "V9RR10_J", "Vxxx10", "Vxxx20", "Vyyy10", "Vyyy20", "Vyyy30", "Vyyy40",
	"V6305E_T", "V6305E_S", "V6305E_J" };

short la_mdl2nam (nam,mdl)
char	nam[] ;					/* returns - hw. name	*/
int4	mdl ;					/* hw. model		*/
{
	int 	k ;
	short 	w ;
	static readonly char x[17] = "0123456789ABCDEF" ;

	if (mdl>=HWSIZ)
	{
		nam[0] = '0' ; nam[1] = 'x' ;
		k = 8 ;
		while (k!=0)
		{
			k-- ;
			nam[k+2] = x[mdl%16] ;
			mdl = mdl>>4 ;
		}
		w = 10 ;
	}
	else
	{
		k = mdl;
		w = 0 ;
		while (hw_name[k].nam[w]!=0)
		{
			nam[w] = hw_name[k].nam[w] ;
			w++ ;
		}
	}
	nam[w] = 0 ;
	return w ;
}
