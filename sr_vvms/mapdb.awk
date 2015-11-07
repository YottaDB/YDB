BEGIN	{
		dontprint = 0;
		start = 0;
		module = ""
		endoffset = "";
	}
$4 == "Synopsis"					{ synopsis_section = $2; dontprint += 3; }
$1 == ""						{ dontprint = 6; }
$1 == "$CODE$"						{ dontprint = 1; }
dontprint						{ dontprint--; next; }
synopsis_section == "Object"	{
					if ($2 == "")
					{
						module = $1;
						next;
					} else if (module == "")
					{
						module = $1;
						precreator = $6;
						creator = $7;
					} else
					{
						precreator = $5;
						creator = $6;
					}
					if (creator == "Message")
						filext[module] = ".MSG";
					else if (precreator == "MACRO-64")
						filext[module] = ".M64";
					else if (creator == "AMAC")
						filext[module] = ".MAR";
					else
						filext[module] = ".C";
					module = "";
					next;
				}
$1 == "$CODE"			{ start = 1; }
$1 == "$BSS$"			{ start = 0; }
$1 == "$DATA$"			{ start = 0; }
$1 == "$LITERAL$"		{ start = 0; }
$1 == "$READONLY$"		{ start = 0; }
$1 == "$READONLY_ADDR$"		{ start = 0; }
$NF == "MOD"			{ next; }		# e.g. lines starting with $LINKAGE, $SYMVECT, _AMAC$LINKAGE, etc.
start		{
			if ($1 != "" && $2 == "")
			{
				module = $1;
			} else
			{
				if (module == "")
				{
					module = $1;
					begin = $2;
					end = $3;
					octa_field = $7;
				} else
				{
					begin = $1;
					end = $2;
					octa_field = $6;
				}
				if (octa_field == "OCTA")
				{
					gsub("_", "", image);
					printf "\tset ^%s(\"%s\")=\"%s%s\"\n", image, begin, module, filext[module];
					endoffset = end;
				}
				module = "";
			}
		}
END	{
		if (endoffset != "")
			printf "\tset ^%s(\"%s\")=\"%s%s\"\n", image, endoffset, "IMAGE", ".END";
	}
