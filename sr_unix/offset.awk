#################################################################
#								#
#	Copyright 2002, 2004 Sanchez Computer Associates, Inc.	#
#								#
# Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
BEGIN	{
		curlyparenlevel = 0;
		squareparenlevel = 0;
		prevfield = "";
		fieldcount = 0;
	}
	{
		input = $0;
		gsub("\\[.*\\]", " ");
		gsub("\".*\"", " ");
		gsub("'.*'", " ");
		gsub("\\[", " & ");	# telecaster's awk doesn't allow for putting [ within a [] block hence this is separate from the following line
		gsub("\\]", " & ");
		gsub("[#;,{}=*]", " & ");
		if ($1 != "")
		{
			for (i = 1; i <= NF; i++)
			{
				if ($i == "{")
				{
					if (curlyparenlevel == 0 && squareparenlevel == 0 && prevfield == c_struct)
					{
						found_struct=1;
						gsub(c_struct,c_struct"_type",input);
					}
					curlyparenlevel++;
					if (curlyparenlevel == 1 && squareparenlevel == 0)
						fieldcount = 0;
				} else if ($i == "}")
					curlyparenlevel--;
				else if ($i == "[")
					squareparenlevel++;
				else if ($i == "]")
					squareparenlevel--;
				else if ($i == ";" || $i == ",")
				{
					if (squareparenlevel == 0 && curlyparenlevel == 1 && prevfield !~ /^[0-9][0-9]*$/)
						structarray[fieldcount++] = prevfield;
					else if ((curlyparenlevel == 0 && squareparenlevel == 0) && (prevfield == c_struct || found_struct))
					{
						for (j = 1; j <= i; j++)
							printf "%s ", $j;
						if (found_struct) printf "\ntypedef struct %s_type %s;\n",c_struct,c_struct;
						printf "\n#undef offsetof\n#define	offsetof(TYPE, MEMBER) ((int)(intptr_t)&((TYPE *)0)->MEMBER)";
						printf "\n#define	PRINT_OFFSET(TYPE, MEMBER)	printf(\"\\toffset = %%.4d [0x%%.4x]      size = %%.4ld [0x%%.4lx]    ----> \"#MEMBER\" \\n\", offsetof(TYPE,MEMBER), offsetof(TYPE,MEMBER), sizeof(temp_##TYPE.MEMBER), sizeof(temp_##TYPE.MEMBER))\n";
						printf "\nvoid main()\n{\n\t%s\ttemp_%s;\n\n", c_struct, c_struct;
						printf "\tprintf (\"\\nStructure ----> %s <---- \tsize = %%ld [0x%%lx] \\n\\n\", sizeof(temp_%s), sizeof(temp_%s));\n", c_struct, c_struct, c_struct;
						for (i = 0; i < fieldcount; i++)
						{
							gsub(/[)(]*/,"",structarray[i]);
							printf "\n\tPRINT_OFFSET(%s,%s);", c_struct, structarray[i];
						}
						printf "\n\tprintf(\"\\n\");\n}\n";
						exit
					}
				}
				if (squareparenlevel == 0 && $i != "]")
					prevfield = $i;
			}
			print input;
		}
	}
