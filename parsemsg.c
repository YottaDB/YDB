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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int read_and_stack(FILE *fp);
int print_stack();
int print_cfile();
char *basename(char *filename, char *suffix); 
int format_line(char *buf, FILE *fp);

#define RECORD_SIZE 1024

#define NUM_SEV_TYPES 6
#define NO_SEV_DEFINED 9999

char *sevtypes[] = {"warning", "success", "error", "info", "fatal", "severe"};
int  sevnums[] =  {0, 1, 2, 3, 4, 4};


typedef struct Lines
{
	char buff[RECORD_SIZE];
	char sev[16];
	char code[256];
	int  sev_num;
	int  fao;
	struct Lines *next;
}NewLine;

NewLine *newline;
NewLine *first;

int main(int argc, char **argv)
{


	FILE *fp;
	FILE *fpout;
	int  len;
	char *basenme;
	char *p;
	char infilename[256];

	newline = NULL;
	if (!argv[1])
	{
		printf("Must specify input file name\n");
		return 1;
	}

	fp = fopen(argv[1],"r");
	if (!fp)
	{
		printf("Error on open\n");
		return 1;
	}

	
	if (!read_and_stack(fp))
	{
		printf("Error no records in file %s\n",argv[1]);
		return 1;
		
	}
/*	print_stack(); */
	strcpy(infilename,argv[1]);
	basenme = basename(infilename,".msg");

	fpout = stdout;
	print_cfile(fpout,basenme);
	fflush(stdout);

	fclose(fp);
	return 0;
}

int read_and_stack(FILE *fp)
{
	int count;
	char buff[RECORD_SIZE];
	char *fdata;
	

	first = NULL;
	for (;;)
	{
		fdata = fgets(buff,sizeof(buff),fp);
		if ( (!fdata) || (buff[0] == 0x0))
			break;
		if (feof(fp))
			break;	
		/* ignore comment lines, empty lines */
		if (buff[0] == '\n' || buff[0] == '!')
			continue;
		if (!first)
		{
			first = (NewLine*)malloc(sizeof(NewLine));
			if (!first)
			{
				printf("Malloc failed for newline\n");
				return 0; 
			}
			newline = first;
			newline->next = NULL;


		}
		else
		{
			newline->next = (NewLine*)malloc(sizeof(NewLine));
			if (!newline->next)
			{
				newline->next = NULL;
				printf("Malloc failed for next newline\n");
	
			}
			newline = newline->next;
			


		}
		strcpy(newline->buff,buff);
		count = 0;
/*
		while(newline->buff[count] != 0x0)
		{	
			if (newline->buff[count] == '\t')
				newline->buff[count] = ' '; 
			count++;

		}
*/
/*
		newline->next->buff[0] = 0x0;
		newline = newline->next;
*/
	}
	if (!first)
	{
		printf("No input records!!\n");
		return 0;
	}
	return 1;   /* return true */


}

int print_stack()
{
	newline = first;

	while(newline->buff[0] != 0x0)
	{

		printf("%s",newline->buff);
		newline = newline->next;
	}

	return 0;
}

int print_cfile(FILE *fpout,char *base)
{       
	int  len;
	int  hit;
	int  hit2;
	int  textend;
	char *ptextend;
	int  facnum;
	char error_code[256];
	char error_text[RECORD_SIZE];
	char error_text_escape[RECORD_SIZE];
	char error_facility[256];
	char error_facnum[256];
	char error_prefix[256];
	char error_title[256];
	char error_fao[256];
	NewLine *start;         /* start of lines to be printed */
	char *phit, *phit2;

	char quote[2] = {'"',0x0};

	error_code[0] = 0x0;
	error_text[0] = 0x0;
	error_facility[0] = 0x0;
	error_facnum[0] = 0x0;
	error_prefix[0] = 0x0;
	error_title[0] = 0x0;
	len = 0;
	hit = 0;

        newline = first;
	/* parse facility */
	phit = strchr(newline->buff,'.');

	if (!phit)
	{
		printf("No facility record found!!\n");
		return 0;  /* return false */
	}

	hit = phit - newline->buff;

	if (memcmp(&newline->buff[hit],".FACILITY",strlen(".FACILITY")) != 0)
	{
		printf("No facility record found!!\n");
		return 0;  /* return false */
	}
	
	hit = hit + strlen(".FACILITY");
	hit2 = 0;

	for(;newline->buff[hit] != 0x0;hit++)
	{
		if (newline->buff[hit] == ',')
		{
			break;
		}
		if (!isalpha(newline->buff[hit]) )
			continue;
		error_facility[hit2] = newline->buff[hit];
		hit2++;
		
	}

	error_facility[hit2] = 0x0;
	if (newline->buff[hit] != ',')
	{
		printf("Invalid facility record found!!\n");
		return 0;  /* return false */
	}
	hit2 = 0;
	hit++;	
	for (;newline->buff[hit] != 0x0;hit++)
	{
		if (newline->buff[hit] == '/')
			break;
		error_facnum[hit2] = newline->buff[hit];
		hit2++;
	
	}			
	error_facnum[hit2] = 0x0;	
	if (newline->buff[hit] != '/')
	{
		printf("Invalid facility record found!!\n");
		return 0;  /* return false */
	}
	
	hit++;
	if (memcmp(&newline->buff[hit],"PREFIX=",strlen("PREFIX=") != 0 ))
	{
		printf("Invalid facility record found!!\n");
		return 0;  /* return false */
	}
	hit = hit + strlen("PREFIX=");

	hit2 = 0;
	for (;newline->buff[hit] != 0x0;hit++)
	{
		error_prefix[hit2] = newline->buff[hit];
		if (!isalnum(newline->buff[hit]))
		{
			if (newline->buff[hit] != '_')
			{
				error_prefix[hit2] = 0x0;	
				continue;
			}
		}
		hit2++;
	}
	error_prefix[hit2] = 0x0;	

		
	/* parse title */
	newline = newline->next;
	if (!newline)
	{
		printf("No TITLE record found\n");
		return 0;
	}
	/* don't really need the title record since the files basename is used as the title */
	newline = newline->next;
	if (!newline)
	{
		printf("no records found after title \n");
		return 0;
	}


	fprintf(fpout,"#include %smdef.h%s\n",quote,quote); 
	fprintf(fpout,"#include %smerror.h%s\n",quote,quote); 
	fprintf(fpout,"\n");
	fprintf(fpout,"LITDEF\terr_msg %s[] = {\n",base);

	start = newline;	/* save the starting point of print lines */
        for(;newline;newline = newline->next)
        {       
		if (strcmp(newline->buff,".end") == 0)
			break; 
#ifdef MSG_DEBUG
                printf("%s",newline->buff);
#endif
		if (newline->buff[0] == '\n')
			continue;
		phit = strchr(newline->buff,'<');
		phit2 = strchr(newline->buff,'\t');
		if (!phit)
		{
			phit = strchr(newline->buff,'.');
			if (phit)
			{	
				hit = phit - newline->buff;
				if (strncmp(&newline->buff[hit],".end",3) == 0)
					break;  /* done */

			}
			fprintf(fpout,"Error in line format missing <\n");
			continue;
		}
		if ( (phit2) && (phit2 < phit))  /* found tab before text */
			phit = phit2;

		hit = phit - newline->buff;

		strncpy(error_code,newline->buff,hit);
		error_code[hit] = 0x0;

		hit = 0;
		for (;error_code[hit] != 0x0;hit++)
		{
			if (isalnum(error_code[hit]))
				continue;  
			if (error_code[hit] == '_')
				continue;
			error_code[hit] = 0x0;
		}
		/* save it for later to print the defines */
		strcpy(newline->code,error_code);

	
		phit = strchr(newline->buff,'<');
		if (!phit)
			continue;
		phit2 = strchr(newline->buff,'>');
		if (!phit2)
		{
			fprintf(fpout,"Error in line format missing >\n");
			continue;
		}
		ptextend = phit2;  /* sev codes should start here */
		hit = (phit - newline->buff) + 1;
		hit2 = (phit2 - newline->buff) - 1;
		len = hit2 - hit;	
		if (len <= 1)
		{
			error_text[0] = 0x0;
		}
		else
		{
			len++;
			strncpy(error_text,&newline->buff[hit],len);
			error_text[len] = 0x0;
		}
		
		/* parse out the severity type and fao number */

		textend = ptextend - newline->buff;
		textend++;   /* should be a / or it's an error */
		for(;newline->buff[textend] != 0x0;textend++)
		{
			if (newline->buff[textend] == '/')
				break;
		}
		if (newline->buff[textend] == 0x0)
		{
			fprintf(fpout,"Error in line format missing sev value\n");
			continue;

		}
		textend++;  /* bump past the / so we can find the next one */
		phit = strchr(&newline->buff[textend],'/');
		if (!phit)
		{
			fprintf(fpout,"Error in line format missing sev value ending\n");
			continue;
		}
		len = phit - &newline->buff[textend];
		strncpy(newline->sev,&newline->buff[textend],len);

		/* lower case the sev code */
		for (hit=0;newline->sev[hit] != 0x0;hit++)
		{
			newline->sev[hit] = tolower(newline->sev[hit]);
		}
		/* convert sev text to sev num */
		newline->sev_num = NO_SEV_DEFINED;
		for (hit=0;hit < NUM_SEV_TYPES;hit++)
		{
			if (strncmp(newline->sev,sevtypes[hit],strlen(sevtypes[hit])) == 0)
			{
				newline->sev_num = sevnums[hit];
				break;
			}

		}

		if (newline->sev_num == NO_SEV_DEFINED)
		{
			fprintf(fpout,"Error in line format invalid sev value\n");
			continue;
		}

		hit2 = textend + len; /* now find fao from here */

		phit = strchr(&newline->buff[hit2],'=');
		if (!phit)
		{
			fprintf(fpout,"Error in line format missing fao value\n");
			continue;
		}
		hit = phit - &newline->buff[0];
	
		if (strncmp(&newline->buff[hit-3],"fao",3) != 0)
		{
			fprintf(fpout,"Error in line format missing fao value\n");
			continue;
		}
		hit++;
		hit2 = 0;

		for(;newline->buff[hit] != 0;hit++,hit2++)
		{
			error_fao[hit2] = newline->buff[hit];
		}

		/* add escape char for quoted strings */

		hit2 = 0;	
		error_text_escape[0] = 0x0;
		for (hit=0;error_text[hit] != 0x0;hit++,hit2++)
		{
			if (error_text[hit] == '"')
			{
				error_text_escape[hit2] = '\\';
				hit2++;
			}
			error_text_escape[hit2] = error_text[hit];

		}
		error_text_escape[hit2] = 0x0;

		newline->fao = atoi(error_fao);
		fprintf(fpout,"\t%s%s%s, %s%s%s, %d,\n",quote,error_code,quote,quote,error_text_escape,quote,newline->fao);
                
        }
	fprintf(fpout,"};\n");
	fprintf(fpout,"\n");
	
	facnum = atoi(error_facnum);
	newline = start;  /* restart loop at first print line */
	hit2 = 1;
        for(;newline;newline = newline->next)
	{
		if (newline->code[0] == 0x0)
			continue;
		hit = (facnum+2048)*65536+((hit2+4096)*8)+newline->sev_num;
		fprintf(fpout,"LITDEF\tint %s%s = %d;\n",error_prefix,newline->code,hit);
		hit2++;

	}

	fprintf(fpout,"\n");
	fprintf(fpout,"LITDEF\terr_ctl %s_ctl = {\n",base);
	fprintf(fpout,"\t%d,\n",facnum);
	fprintf(fpout,"\t%s%s%s,\n",quote,error_facility,quote);
	fprintf(fpout,"\t&%s[0],\n",base);
	hit2--;
	fprintf(fpout,"\t%d};\n",hit2);  /* record count releative to 0 */
	
        return 1;  /* return TRUE */
}               


int format_line(char *buf, FILE *fp)
{
	int  len;
	char *phit;
	char *phit2;
	char error_code[256];
	char error_text[256];
	char quote[2] = {'"',0x0};

	error_code[0] = 0x0;
	error_text[0] = 0x0;
	len = 0;

	phit = strchr(buf,' ');
	if (!phit)
		return 0;

	strncpy(error_code,buf,(phit-buf));
	
	phit = strchr(buf,'<');
	if (!phit)
		return 0;
	phit2 = strchr(buf,'>');
	if (!phit2)
		return 0;
	strncpy(error_text,phit,phit2-phit);
	fprintf(fp,"%s%s%s, %s%s%s\n",quote,error_code,quote,quote,error_text,quote);
}



char *basename(char *filename, char *suffix) 
{
  char *p, *base;
  p = strrchr(filename, '/');
  base = p ? (p+1) : filename;
  if (suffix && strlen(base) >= strlen(suffix)) 
  {
    p = &base[strlen(base)-strlen(suffix)];
    if (!strcmp(p, suffix)) *p = '\0';
  }
  return base;
}


