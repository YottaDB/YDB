BEGIN	{
		dontprint = 0; insrclist = 1; cursrcline = 1; listline = 0;
	}
$1 == ""						{ dontprint = 1; }
$1 == ""						{ dontprint = 1; }
$1 == "Routine" && $2 == "Size:"			{ dontprint = 1; }
$1 == "Source" && $2 == "Listing"			{ dontprint = 3; }
$1 == "Machine" && $2 == "Code" && $3 == "Listing"	{ dontprint = 3; }
$1 == ".PSECT" && $2 == "$LINK$,"			{ exit; }
$1 == ".PSECT"  					{ insrclist = 0; next; }
dontprint						{ dontprint--; next; }
insrclist	{
			sub("^[ 0-9][ 0-9][ 0-9][ 0-9][ 0-9][ 0-9][ 0-9][X\t]","",$0);
			if (srcline[$1] == "")
				srcline[$1] = cursrcline++;
			next;
		}
!insrclist	{
			sub("^\t","        ",$0);	# replace tabs with spaces at the beginning
			offset = substr($0, 10, 8);
			gsub(" ", "0", offset);
			lastbutone = NF - 1;
			# do not consider usages like "; R28" as a listing line number
			if (($lastbutone == ";") && (substr($NF, 1, 1) != "R"))
			{
				listline = +$NF;	# the "+" is to typecast $NF into a number (instead of a string)
				printf "\tset ^offset(\"%s\",\"%s\",0)=%d\n", module, offset, srcline[listline];
				printf "\tset ^offset(\"%s\",\"%s\",1)=%d\n", module, offset, listline;
			}
		}
END	{
	}
