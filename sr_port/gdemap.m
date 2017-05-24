;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2010-2017 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
map:	;create maps for put and show, names for get and show
CREATEGLDMAP ; create map for GDEPUT to write to .gld file
	k lexnams n suffix,s1,s2
	d NAM2MAP
	s s1="",$zpi(s1,$zch(255),SIZEOF("mident"))=""
	f  s s2=s1,s1=$o(map(s1),-1),map(s2)=map(s1) q:s1="$"  k map(s1)
	s map("%")=map("$")
	f s2="#","$" k map(s2)
 	q
;----------------------------------------------------------------------------------------------------------------------------------
NAM2MAP ;
	; transform nams() array to map() array
	;
	n lexnams s s=""
	s map("$")=nams("*"),map("#")=nams("#")   ; initialize map() array for "*" name and local-locks ("#" name).
	f  s s=$o(nams(s)) q:'$zl(s)  d lexins(s) ; initializes lexnams() array
	; convert names into maps. more general names coming ahead of more specific names
	; i.e. if names are of the form, ABCD, ABCD(1:10), ABCD(3), ABCD(3,7), ABCD(3,5:10), ABC*, AB* AND A*
	; process them in the order A*, AB*, ABC*, ABCD, ABCD(1:10), ABCD(3), ABCD(3,5:10), ABCD(3,7)
	s i=1 f  s i=$o(lexnams(i)) q:'$zl(i)  d starins(i)
	; Insert rest of the names (no * in them). Do unsubscripted names first and then the subscripted names.
	; This order is important as otherwise the mappings would end up in the wrong region.
	s s="" f  s s=$o(lexnams(0,s),-1) q:'$zl(s)  d pointins(s,lexnams(0,s))	; insert unsubscripted names
	s i=0 f  s i=$o(lexnams(i),-1) q:'$zl(i)  d
	. s s="" f  s s=$o(lexnams(i,s),-1) q:'$zl(s)  d pointins(s,lexnams(i,s))	; insert subscripted names
	; remove map entries that are contiguous and are mapped to same region. keeps map() array size minimal
	s s1=$o(map(""),-1)
	f  s s2=s1,s1=$o(map(s1),-1) q:s2="$"  i map(s1)=map(s2) k map(s2)
	q
;----------------------------------------------------------------------------------------------------------------------------------
SHOWNAM
	; to show names, we transform the NAMES to MAP and then reverse transform the MAP back to NAMES.
	; this ensure we optimize the names (remove redundant name specifications) before showing it.
	n lexnams,map
	d NAM2MAP
	s s1="",$zpi(s1,$zch(255),SIZEOF("mident"))=""
	f  s s2=s1,s1=$o(map(s1),-1),map(s2)=map(s1) q:s1="$"  k map(s1)
	k map("#") i '$$MAP2NAM(.map) zm gdeerr("GDECHECK")\2*2
 	q
;----------------------------------------------------------------------------------------------------------------------------------
MAP2NAM(list1)
	n maxMap,currMap,currMapLen,prevMap,prevMapLen,currReg,prevReg,nextReg,prevPrefix,currPrefix,gblname,coll,key,key2
	n namSpc,namSpc2,currNam,currNamLen,i,startMap,midentSize,prevPrevMap,list,mapsublvl,mapisplusplus,nextMap
	m list=list1
	s currMap=$o(list("")) q:currMap'="#)" 0
	s currMap=$o(list(currMap))
	i currMap="%" s list("$")=list("%")	; if "$" is missing, assign it the same value as "%"
	e  q:currMap'="$" 0
	s midentSize=$s(SIZEOF("mident")=8:8,1:SIZEOF("mident")-1)
	s $zpi(maxMap,$zch(255),midentSize+1)="",currMap=$o(list(""),-1)
	; In pre-V61 maps, we would have a 32-byte $zch(255) as the final map. But V61 onwards, we have only 31-bytes of $zch(255)
	; If ever we read a pre-V61 gld file, treat the 32-byte as if it was a 31-byte $zch(255)
	i currMap'=maxMap i maxMap_$zch(255)=currMap  s list(maxMap)=list(currMap) k list(currMap) s currMap=maxMap
	i currMap'=maxMap q 0
	k nams
	; ----------------------------------------------------------------------------------------------------
	; Stage 1 : Replace all subscripted global entries in the map with unsubscripted global name entries.
	; This simplifies processing in later steps. This is in turn divided into sub-stages.
	;     ------------------------------------------------------------------------------------------------
	;     Substage (a) : Do some preparatory processing for subscripted map entries.
	;			1) Calculate the level (# of subscripts) of each map entry.
	;			2) Note down the map entry based on its level in the "mapsublvl" array.
	;			3) Note down the ending offset of the map minus the last subscript.
	;			4) Note down if the map entry is of the ++ form (in the "mapisplusplus" array).
	;			5) Add a map entry corresponding to the unsubscripted global name if not already present.
	;		All of these are used in later substages.
	n mapsublvl
	s mapsublvl(0)=""	; to be used as a loop terminator later
	s currMap="" f  s currMap=$o(list(currMap)) q:currMap=""  d
	. s mapsublvl=$zl(currMap,ZERO)-1
	. i mapsublvl=0 q
	. n offset,startoff
	. s offset=0  f i=1:1:mapsublvl s offset=$zfind(currMap,ZERO,offset) i i=1 s startoff=offset
	. s mapsublvl(mapsublvl,currMap)=""		; Note down map entries based on their subscript level
	. s list(currMap,"SUBLEVEL")=mapsublvl		; subscript level of this map entry
	. s list(currMap,"PRFXOFF")=offset-1		; offset of the prefix which is the map minus the last subscript
	. s list(currMap,"GBLNAMEOFF")=startoff-2	; ending offset of the unsubscripted global name
	. s currMapLen=$zl(currMap)
	. s list(currMap,"MAPLEN")=currMapLen	; length of the full map entry
	. i $$isplusplus(currMap,currMapLen) s mapisplusplus(mapsublvl,currMap)=""
	. s gblname=$ze(currMap,1,startoff-2)
	. i '$d(list(gblname)) s list(gblname)=list($o(list(gblname)))
	;     ------------------------------------------------------------------------------------------------
	;     Substage (b) : Process all map entries at the same mapsublvl starting from the highest level.
	;			Add name entries corresponding to map entries of one level.
	;			If necessary add map entries of a parent (lower) level as part of removing map entries at one level.
	;			Also remove any intervals that map to same region and become contiguous due to the above removal.
	;			Process map entries of the ++ form first. This is easy and once this is done, all we are left
	;			with are non-++ map entries at a given level and this is much easier to handle.
	n lvl
	s lvl="" f  s lvl=$o(mapsublvl(lvl),-1) q:lvl=0  d
	. ; Case (i) : process ++ entries (if any) at this level first
	. n map,mapLen
	. s map="" f  s map=$o(mapisplusplus(lvl,map),-1) q:map=""  d
	. . s mapLen=list(map,"MAPLEN"),currReg=list(map)
	. . s gblname=$ze(map,1,list(map,"GBLNAMEOFF")),coll=+$g(gnams(gblname,"COLLATION"))
	. . s key=$ze(map,1,mapLen-1),namSpc=$zcollate(key_ZERO_ZERO,coll,1) d add2nams(namSpc,currReg,"POINT")
	. . i '$d(list(key)) m list(key)=list(map)  s mapsublvl(lvl,key)="",list(key,"MAPLEN")=list(key,"MAPLEN")-1
	. . e  d
	. . . ; if we reach here, this means we are going to remove a ++ map entry and did not add anything instead in "list"
	. . . ; in this case check if the map entries surrounding the ++ entry map to the same region. if so remove the first one
	. . . d killPrevMapIfPossible(.list,map)
	. . d killmap(.list,map) ; remove the ++ map entry and associated structures
	. ; Case (ii) : process non-++ entries (if any) at this level next
	. n prfxoff1,prfxoff2,parentMap,parentMapLen,offset
	. n trailingrange	; this indicates whether we need to add a trailing range as part of removing a map entry
	. s trailingrange=1,map="" f  s map=$o(mapsublvl(lvl,map),-1) q:map=""  d
	. . s currReg=list(map)
	. . s gblname=$ze(map,1,list(map,"GBLNAMEOFF")),coll=+$g(gnams(gblname,"COLLATION"))
	. . i trailingrange d
	. . . ; Case (iii) : Check if a namespace for the trailing range needs to be added
	. . . ; e.g. If x(1,2) is the current map entry, check the parent level map entry (i.e. x(1)) to see the region it falls in.
	. . . ; If the name (not map) x(1) maps to the exact same region as the region following the map entry x(1,2), then there is
	. . . ; no need to add the trailing namespace since a bigger namespace would get added as part of processing the lower-level
	. . . ; subscripted map entry x(1). If x(1) maps to a different region, then we need to add the range x(1,2:) so it
	. . . ; overrides the x(1) namespace that will get added while processing the lower-level subscripted map entry x(1).
	. . . s nextReg=list($o(list(map))),parentMap=$ze(map,1,list(map,"PRFXOFF")-1),prevReg=list($o(list(parentMap)))
	. . . i nextReg=prevReg q  ; x(1) and x(1,2:) map to same region
	. . . ; add range x(1,2:)
	. . . s mapLen=list(map,"MAPLEN")
	. . . s key=$ze(map,1,mapLen),namSpc=$zcollate(key_ZERO_ZERO,coll,1)
	. . . s nullsub="""""",namSpc=$ze(namSpc,1,$zl(namSpc)-1)_":"_nullsub_")"
	. . . d add2nams(namSpc,nextReg,"RANGE")
	. . s prevMap=$o(list(map),-1)
	. . s prfxoff1=+$g(list(prevMap,"PRFXOFF")),prfxoff2=list(map,"PRFXOFF"),parentMap=$ze(map,1,prfxoff2-1)
	. . i (prfxoff1=prfxoff2)&($ze(prevMap,1,prfxoff1)=$ze(map,1,prfxoff2)) d  q
	. . . ; Case (iv) : These are TWO adjacent map entries with different last subscripts (every other subscript identical).
	. . . ; Can be easily replaced by a RANGE type namespace.
	. . . ; For example x(1,2) is the current map entry and x(1,1) is the previous map entry.
	. . . ; Then we can add the name space x(1,1:2) and remove the x(1,2) map entry.
	. . . ; Similar to the "trailingrange" case, we can check if the parent level map entry maps to same region as the current
	. . . ; range and if so skip this range altogether.
	. . . i list($o(list(parentMap)))'=currReg d
	. . . . s prevMapLen=list(prevMap,"MAPLEN"),namSpc=$zcollate(prevMap_ZERO_ZERO,coll,1)
	. . . . n lastSubs
	. . . . s mapLen=list(map,"MAPLEN"),lastSubs=$ze(map,prfxoff2,mapLen)
	. . . . s key=gblname_lastSubs,namSpc2=$zcollate(key_ZERO_ZERO,coll,1)
	. . . . s namSpc2=$ze(namSpc2,$zl(gblname)+3,$zl(namSpc2)-1) ; extract out the last/ONLY subscript
	. . . . s namSpc=$ze(namSpc,1,$zl(namSpc)-1)_":"_namSpc2_")"
	. . . . d add2nams(namSpc,currReg,"RANGE")
	. . . s trailingrange=0
	. . . d killmap(.list,map)
	. . s nextMap=$o(list(map))
	. . i 'trailingrange  d  i '$d(list(map)) q
	. . . ; Case (v) : Check if "map" maps to same region as the next map (possible because we skipped the
	. . . ;            "do killPrevMapIfPossible" call in Case (iv)). If so remove "map".
	. . . i currReg=list(nextMap) d killmap(.list,map)
	. . . s trailingrange=1 ; now that we know adjacent map entries cannot be replaced by a RANGE type, set this first
	. . ; A few cases are possible here.
	. . ; Case (vi) : The current map is say x(1,2) and the NEXT map is x(1)++. In this case, the map entry x(1)++
	. . ;             can be removed as it is equivalent to x(1,<max>) (where <max> is the maximum subscript possible)
	. . ;             and the range x(1,2:) has already been added. In addition IF map entry x(1,2) maps to the same
	. . ;             region as the map entry AFTER x(1)++, then the map entry x(1,2) can also be removed without adding
	. . ;             a x(1) namespace. And we can move on to the next iteration. IF not, follow through to Case (vii).
	. . i $d(mapisplusplus(lvl-1,nextMap))&((parentMap_ONE)=nextMap) d  i '$d(list(map)) q
	. . . d killPrevMapIfPossible(.list,nextMap) ; this COULD remove list(map)
	. . . d killmap(.list,nextMap)
	. . ; Case (vii) : The current map is say x(1,2). The previous map could be x(1) OR could be x(k) where k < 1 or just x
	. . ;              If previous map is x(1), then we only need to add the NAMESPACE x(1) and delete the MAP x(1,2)
	. . ;              If previous map is x(k) where k<1 or just x, then add MAP x(1), add NAMESPACE x(1) and delete MAP x(1,2)
	. . ; As to adding NAMESPACE x(1), similar to the "trailingrange" case, one might be tempted to check if parent level map
	. . ; entry maps to the same region as x(1) and if so skip this NAMESPACE altogether. But the x(1) name addition is
	. . ; actually an override to a potential range (e.g. x(0:2) name) at the same level. Therefore dont optimize here.
	. . i '$d(list(parentMap)) d
	. . . s list(parentMap)=currReg          ; add MAP x(1)
	. . . i lvl=1 q
	. . . s list(parentMap,"SUBLEVEL")=lvl-1,list(parentMap,"MAPLEN")=$zl(parentMap)
	. . . s list(parentMap,"GBLNAMEOFF")=list(map,"GBLNAMEOFF")
	. . . s offset=0  f i=1:1:lvl-1 s offset=$zfind(parentMap,ZERO,offset)
	. . . s list(parentMap,"PRFXOFF")=offset-1
	. . . s mapsublvl(lvl-1,parentMap)=""    ; complete addition of MAP x(1)
	. . e  d
	. . . ; the MAP x(1) already existed and we are about to delete the MAP x(1,2).
	. . . ; check if the MAP after x(1,2) maps to same region as MAP x(1). If so, MAP x(1) can be removed
	. . . d killPrevMapIfPossible(.list,map)
	. . s namSpc=$zcollate(parentMap_ZERO_ZERO,coll,1)
	. . d add2nams(namSpc,currReg,"POINT")	; add NAMESPACE x(1)
	. . d killmap(.list,map)		; delete MAP x(1,2)
	; ------------------------------------------------------------------------------
	; Stage 2 : Replace all unsubscripted map entries having ")" at the end (e.g. "abc)") with entries not having the ")".
	;		Also remove any intervals that map to same region and become contiguous due to the above removal.
	; This simplifies processing in later steps.
	s currMap=maxMap,nextReg=""
	f  q:currMap="$"  s prevMap=$o(list(currMap),-1) d  s currMap=prevMap,nextReg=currReg
	. s currReg=list(currMap)
	. i currReg=nextReg k list(currMap)
	. ; If currMap contains ")" (e.g. "abc)"), it means one of two possibilities.
	. ;	a) prevMap is "abc". In this case, just add name "abc" (assuming it was not already added in previous stages)
	. ;		And delete the "abc)" map entry from later processing.
	. ; 	b) prevMap is not "abc". In this case, add name "abc" (assuming it was not already added in previous stages)
	. ;		In addition, replace the "abc)" map entry with "abc" for later processing.
	. s currMapLen=$zl(currMap)
	. i $ze(currMap,currMapLen)'=")" q
	. s namSpc=$ze(currMap,1,currMapLen-1)
	. i '$d(nams(namSpc)) s nams(namSpc)=currReg
	. k list(currMap)
	. i prevMap'=namSpc s list(namSpc)=currReg q
	. ; if we reach here, this means we removed a ")" map entry and did not add anything instead in "list"
	. ; so update "currReg" to correspond to next map (the map entry we processed in the previous for loop iteration)
	. s currReg=nextReg
	; ------------------------------------------------------------------------------
	; Stage 3 : Now that unsubscripted namespaces are out of the way, add * namespaces as applicable
	s currMap=maxMap
	f  q:currMap="$"  s prevMap=$o(list(currMap),-1) d  s currMap=prevMap
	. s currReg=list(currMap)
	. s currMapLen=$zl(currMap)
	. s prevMapLen=$zl(prevMap)
	. i (currMap=maxMap)&(prevMap="$") q  ; "$" and maxMap are mapped to same region. Skip processing
	. ; The map entry "currMap" exists and "prevMap" is the previous map entry.
	. ; Determine the namespaces that potentially lie between the two map entries.
	. ; And add them to the "nams" array.
	. f i=1:1:currMapLen  i $ze(currMap,i)'=$ze(prevMap,i) q
	. s matchLen=i-1  ; the length of the maximal common prefix between prevMap and currMap
	. ; Case (3a) : matchLen == prevMapLen
	. ;	In this case we are guaranteed that prevMapLen < currMapLen, and we need to add only ONE namespace.
	. ;	Example prevMap="ag", currMap="agk". Here, matchLen=2, prevMapLen=2, currMapLen=3. Add only "ag*".
	. i (matchLen=prevMapLen)&(prevMapLen<currMapLen) s namSpc=$ze(prevMap,1,prevMapLen)_"*",nams(namSpc)=currReg q
	. ; Case (3b) : matchLen < prevMapLen
	. ;	Example prevMap="agxy", currMap="ajk". Here, matchLen=1, prevMapLen=4, currMapLen=3
	. ;	In this case, we need to add namespace for "aj*", "ai*", "ah*" as a first step.
	. ;	Whether or not we can add "ag*" depends on an optimization check explained below.
	. ;		If "ag*" can be added, then we add it and are done.
	. ;		If not, then we need to add one or more smaller namespaces under "ag" explained below as the SECOND PART.
	. ;	To explain the optimization check, let us say the "list" variable contains the following.
	. ;		list("ag")="DEFAULT"
	. ;		list("agQ")="STAR1"
	. ;		.
	. ;		.
	. ;		list("agwx")="STAR1"
	. ;		list("agxy")="STAR2"
	. ;		list("ajk")="STAR1"	<-- currMap points here
	. ;	Let us say currMap = "ajk". So prevMap = "agxy". In this case, we would definitely add "aj*", "ai*" and "ah*".
	. ;	To determine whether we need to add "ag*", we take a look at where the start of "ag*" namespace maps to.
	. ;	And that would be the entry following list("ag") i.e. list("agQ"). This maps to the STAR1 region.
	. ;	Since this is the same region as the region of currMap, we can safely add just one namespace "ag*" to cover
	. ;	the entire range from list("ag)") to list("ajk") with entries in between covering sub-namespaces that are
	. ;	exceptions (i.e. those which dont map to STAR1). If list("agQ") maps to a region other than STAR1, we need to
	. ;	add more sub-namespaces under the "ag*" namespace which is explained in the SECOND PART below.
	. ;	The FIRST PART of adding "aj*", "ai*" and "ah*" is taken care of by the simple for loop below.
	. ;	There is one exception though.
	. ;		a) If prevMap="XY1" and currMap="XY2", matchLen=2, prevMapLen=3, currMapLen=3
	. ;		   But we should not add the "XY2*" namespace.
	. ;		   This is achieved by the "i<currMapLen" check before the for loop.
	. i currMap=maxMap s currMapLen=1,currMap=$zch(1+$za("z")) ; needed to help lexPrev call return "z"
	. s prevPrefix=$ze(prevMap,1,i),currPrefix=$ze(currMap,1,i)
	. i (i<currMapLen) s namSpc=currPrefix_"*",nams(namSpc)=currReg
	. ; Note: If i=1, it is possible that prevMap="$" in which case, $$lexprev would return "%" and then "" (but not "$").
	. ; So currPrefix=prevPrefix will not work in that case. Hence the additional check for "" return below.
	. f  s currPrefix=$$lexprev(currPrefix) q:(currPrefix=prevPrefix)!('$zl(currPrefix))  d
	. . s namSpc=currPrefix_"*",nams(namSpc)=currReg
	. ;	Do the optimization check.
	. s currPrefix=prevPrefix  ; reset currPrefix to be equal to prevPrefix just in case it is ""
	. s startMap=$o(list(currPrefix))
	. i list(startMap)=currReg  s i=prevMapLen  ; set i to force skip of SECOND PART of processing below.
	. ;	The SECOND PART does the additional "ag" specific processing. Since the stop point is "agxy", we need to add
	. ;	namespaces "agz*", "agy*" first. And then by the same reasoning (for not adding "ag*" in the FIRST PART),
	. ;	we cannot add "agx*" unless the optimization check says it is okay to do so. Assuming it says that is not okay,
	. ;	we go further down by adding "agxz*" and then "agxy*" and then stop.
	. f  q:$i(i)>prevMapLen  d
	. . s currPrefix=prevPrefix_"z",prevPrefix=$ze(prevMap,1,i)
	. . f  q:currPrefix=prevPrefix  s namSpc=currPrefix_"*",nams(namSpc)=currReg,currPrefix=$$lexprev(currPrefix)
	. . ; Do optimization check at each sub-namespaces level. If it succeeds, stop processing any higher level sub-namespaces.
	. . s startMap=$o(list(currPrefix))
	. . i list(startMap)=currReg  s i=prevMapLen  q  ; set i to force quit out of for loop
	. s namSpc=currPrefix_"*",nams(namSpc)=currReg
	; ------------------------------------------------------------------------------
	; Stage 4 : Remove redundant unsubscripted namespaces. As for subscripted namespaces, we are guaranteed they are not
	; 	redundant because of the way Stage 1 processed them.
	; Example : If "a*" and "ab*" both map to the same region, the namespace "ab*" can be safely removed.
	; But if "a*" maps to AREG, "aa*" maps to BREG, and "aaa*" maps to AREG, we cannot remove "aaa*" because
	; "a*" and "aaa*" maps to the same reg. This is because there is a more restrictive mapping "aa*" which
	; maps to a different region than "aaa*". That should prevail.
	; Similarly if "ab*" and "abc" both map to the same region, the namespace "abc" can be safely removed.
	; But if "abc*" also is mapped and to a different region than "abc", then "abc" cannot be removed.
	; With SIZEOF("mident")=32, we allow a max of 31-byte global name specifications.
	; But with SIZEOF("mident")=8 (for older versions with no longnames support, we allow a max of 8-byte global names.
	; Handle this 1-byte discrepancy for the 8-byte case by setting the variable midentSize accordingly.
	s midentSize=$s(SIZEOF("mident")=8:9,1:SIZEOF("mident"))
	n starName
	s nams("*")=list("$")
	; Also take this opportunity to update "nams" variable to contain # of elements in "nams" array
	s currNam="",nams=0
	f  s currNam=$o(nams(currNam)) q:currNam=""  s nams=nams+1  d
	. i (+$g(nams(currNam,"NSUBS"))) q  ; subscripted namespace; dont try to optimize
	. s currNamLen=$zl(currNam)
	. s currReg=nams(currNam)
	. s killed=0,quitLoop=0,starName=($ze(currNam,currNamLen)="*")
	. ; If processing a non-"*" name that is already at the max gvname length, remove corresponding "*" name if any exists
	. ; with the same long name. This is because the non-"*" name overrides the "*" name at the max length (no
	. ; other names are possible other than the max length name).
	. i ('starName&(currNamLen=(midentSize-1))&($d(nams(currNam_"*")))) k nams(currNam_"*")
	. f i=$s(starName:(currNamLen-2),1:currNamLen):-1:0  d  i (""'=prevReg) s:currReg=prevReg nams=nams-1,killed=1 q
	. . s currPrefix=$ze(currNam,1,i)_"*"
	. . s prevReg=$g(nams(currPrefix))
	. k:killed nams(currNam)
	. ; Replace namespaces of the form "...*" where ... is 31 characters long (max-mident) with the "*" removed.
	. i 'killed,currNamLen'<midentSize k nams(currNam) s nams($ze(currNam,1,midentSize-1))=currReg
	; Do some final cleanup
	s nams("#")=list("#)"),nams=nams+1
	q 1
;----------------------------------------------------------------------------------------------------------------------------------
add2nams(nam,reg,type)
	; add a subscripted namespace to "nams" array
	n nsubs,len,c,quotestate,start,i,isrange,gvnprefixoff
	s nsubs=0,len=$zl(nam),start=1,quotestate=0,nam=$ze(nam,2,len)	; remove "^" from beginning of "nam"
	i $d(nams(nam)) q  ; some lower level processing has already creating this name with a region mapping; dont override it
	s nams(nam)=reg
	s nams(nam,"NAME")=nam
	s nams(nam,"TYPE")=type
	s isrange=(type="RANGE")
	f i=1:1:len  d
	. s c=$ze(nam,i)
	. i c="""" s quotestate=$s(quotestate=1:2,1:1)
	. e        s quotestate=$s(quotestate=2:0,1:quotestate) i 'quotestate d
	. . i ((c="(")!(c=",")!(c=")")!(isrange&(c=":"))) d
	. . . s nams(nam,"SUBS",nsubs)=$ze(nam,start,i-1),nsubs=nsubs+1,start=i+1
	. . . i (isrange&((c=",")!(c="("))) s gvnprefixoff=i
	i isrange s nams(nam,"GVNPREFIX")=$ze(nam,1,gvnprefixoff)
	s nams(nam,"NSUBS")=$s(nsubs=0:0,1:nsubs-1)
	q
killPrevMapIfPossible(list,map)
	; checks previous and next map entry of input "map". if they map to same region then removes "previous" map entry.
	; this assumes the input "map" entry is going to be removed after this call.
	n nextReg,prevReg,prevMap
	s prevMap=$o(list(map),-1)
	s nextReg=list($o(list(map))),prevReg=list(prevMap)
	i nextReg'=prevReg q
	d killmap(.list,prevMap)
	q
killmap(list,map)
	n lvl
	s lvl=+$g(list(map,"SUBLEVEL"))
	k list(map),mapsublvl(lvl,map)
	k mapisplusplus(lvl,map) ; this node may not exist but kill of non-existent data works fine so do it always.
	q
lexins:(s)
	; Insert names into "lexnams" array.
	; * names go into lexnams(N,...) where N > 0 and N is the byte length of the name including the *
	; Amongst the non-* names, unsubscripted names go into lexnams(N,...) where N = 0
	; Amongst the non-* names, subscripted names go into lexnams(N,...) where N < 0 and N is the # of subscripts
	;	Within subscripted names with the same # of subscripts, we want to process ranges ahead of points
	;	Hence the isrange-(subslvl*2) calculation below.
	; This lets us process the names in that order (N>0 first, N=0 next, N=-1,-2,... last) later in the caller function.
	; That order is important to ensure correct mappings of names.
	n l,isrange,subslvl
	; if range subscript, "NSUBS" node is actually 1 more than the subscript level (# of commas).
	s isrange=($g(nams(s,"TYPE"))="RANGE"),subslvl=$g(nams(s,"NSUBS"))-isrange
	; check for subscripted name first; check for * in name afterwards; doing it the other way could give false results
	; since it is possible for * to be inside a string subscript
	s l=$s(s["(":isrange-(subslvl*2),s["*":$zl(s),1:0)
	s lexnams(l,s)=nams(s)
	q
starins:(i)
	n j,s,reg,next
	s j=""
	f  s j=$o(lexnams(i,j),-1) q:'$zl(j)  d
	. s s=$ze(j,1,$zl(j)-1)
	. s reg=lexnams(i,j)
	. s next=$$lexnext(s)
	. i $zl(next),'$d(map(next)) s map(next)=map($o(map(next),-1))
	. s map(s)=reg
	q
pointins:(s,reg)
	n keylo,keyhi,nsubs,hasrange,gblname,coll
	s hasrange=($g(nams(s,"TYPE"))="RANGE")
	s nsubs=+$g(nams(s,"NSUBS"))
	i 'hasrange d
	. ; is a point specification
	. ;	e.g. X    -> In this case X and X) are the bounding points
	. ;	e.g. X(1) -> In this case X(1) and X(1)++ are the bounding points
	. ;		where ++ denotes the immediately "next" valid key (byte sequence)
	. i 'nsubs s keylo=s,keyhi=$$alphnext(s)
	. e  d
	. . ; For a subscripted gvn, the next key is obtained by appending a 0x01 byte at the very end (before the two null bytes)
	. . ; Such a key is referred to be in the ++ form.
	. . ; For a numeric subscript, we are guaranteed, 0x01 is never present (as it means the mantissa was 00 which would
	. . ; 	actually have been ignored during subscript representation).
	. . ; For a string subscript, it is possible to have 0x01 in the middle of the subscript representation. But in that
	. . ;	case we expect the 0x01 to be followed by a 0x01 or 0x02 (indicating 0x00 or 0x01 bytes in the original string
	. . ;	subscript). So if we see a 0x01 at the end of the subscript we need to scan back until we dont see a 0x01 and
	. . ;	determine if the 0x01 run length is odd or even and accordingly decide if the last 0x01 is a lone byte or not.
	. . ;	If it is a lone byte, it means this was added as part of the "next" key determination in pointins.
	. . ;	There are two exceptions to this and that is
	. . ;		a) if 0x01 run length is 1 and the immediately preceding byte is 0x00.
	. . ;			In this case, this corresponds to the null ("") string subscript.
	. . ;		b) if 0x01 run length is 2 and the immediately preceding byte is 0x00.
	. . ;			In this case, this is a ++ form key (++ of the "" null subscript)
	. . ; This logic will be used in MAP2NAM.
	. . s gblname=nams(s,"SUBS",0),coll=+$g(gnams(gblname,"COLLATION"))
	. . s keylo=$$gvn2gdsnotrailingnulls("^"_s,coll),keyhi=keylo_$zch(1)
	. i $zl(keyhi),'$d(map(keyhi)) s map(keyhi)=map($o(map(keyhi),-1))
	e  d
	. ; is a range specification
	. ;	e.g. X(1:"abc")      -> In this case X(1) and X("abc") are the bounding points
	. ;	e.g. X("":"")        -> In this case X("") and X) are the bounding points
	. ;	e.g. X(2,"":"")      -> In this case X(2,"") and X(2)++ are the bounding points
	. ;	e.g. X("abc","z":"") -> In this case X("abc","z") and X("abc")++ are the bounding points
	. ;		where ++ denotes the immediately "next" valid key (byte sequence)
	. n range,rangelo,rangehi,rlo,rhi,nullsub
	. s rlo=nams(s,"SUBS",nsubs-1),rhi=nams(s,"SUBS",nsubs)
	. s nsubs=nams(s,"NSUBS"),range=nams(s,"GVNPREFIX")
	. s gblname=nams(s,"SUBS",0),coll=+$g(gnams(gblname,"COLLATION"))
	. s rangelo="^"_range_rlo_")",keylo=$$gvn2gdsnotrailingnulls(rangelo,coll)
	. s nullsub=""""""
	. i (rhi'=nullsub) d
	. . s rangehi="^"_range_rhi_")",keyhi=$$gvn2gdsnotrailingnulls(rangehi,coll)
	. e  d
	. . ; if "" is right side of range, this is equivalent to the "next" parent level subscript.
	. . ; use that to construct the subscript level representation as that is easier than trying
	. . ; to represent the max-possible-key at this level (using a sequence of 1019 0xFFs or so)
	. . n gvn
	. . i nsubs=2 s keyhi=$$alphnext(gblname) ; special case in case range is at FIRST subscript level
	. . e  s gvn="^"_$ze(range,1,$zl(range)-1)_")",keyhi=$$gvn2gdsnotrailingnulls(gvn,coll),keyhi=keyhi_$zch(1)
	. i $zl(keyhi),'$d(map(keyhi)) d
	. . n prevMap
	. . s prevMap=$o(map(keyhi),-1)
	. . s map(keyhi)=map(prevMap)
	. . ; check for sub-range and if so adjust mapping region of previous map entry accordingly
	. . i (prevMap]keylo) s map(prevMap)=reg
	s map(keylo)=reg
	q
gvn2gds(gvn,coll)
	; return subscript (gds) representation for input "gvn".
	; checks for GVSUBOFLOW error and if so issues GDE-specific GVSUBOFLOW error
	n key
	n savetrap
	s savetrap=$etrap
	n $etrap
	s $etrap="goto gvsuboflowerr"
	s key=$zcollate(gvn,coll)
	q key
gvsuboflowerr
	n len
	i $zstatus'["GVSUBOFLOW" q  ; dont know how a non-GVSUBOFLOW error occured. let parent frame handle it like any other error
	s $ecode="",len=$zl(gvn)
	s $etrap=savetrap
	; Do not attempt to print the full "gvn" value as it might exceed the buffer allocated by zmessage.
	; So print first 100 bytes and last 100 bytes with a "..." in between
	zm gdeerr("NAMGVSUBOFLOW"):$ze(gvn,2,100):$ze(gvn,len-100,len):coll	; 2 (instead of 1) to skip ^ at start of gvn
	q
gvn2gdsnotrailingnulls:(gvn,coll)
	; return subscript (gds) representation for input "gvn". Removes trailing double null-byte
	n key
	s key=$$gvn2gds(gvn,coll)
	q $ze(key,1,$zl(key)-2)
alphnext:(s)
	q $s($zl(s)=MAXNAMLN:$$lexnext(s),1:s_")")
	;
lexnext:(s)
	n len,last,succ
	s len=$zl(s),last=$ze(s,len)
	i last="z" f  s len=len-1,last=$ze(s,len) q:last'="z"!'len
	i 'len q ""
	s s=$ze(s,1,len-1),succ=$zch($za(last)+1)
	i succ?1AN q s_succ
	i "A"]succ q s_"A"
	i "a"]succ q s_"a"
	q ""
lexprev:(s)
	n len,last,prior
	s len=$zl(s),last=$ze(s,len)
	s s=$ze(s,1,len-1),prior=$zch($za(last)-1)
	i prior?1AN q s_prior
	i prior]"Z" q s_"Z"
	i prior]"9" q:len=1 "%" q s_"9"
	q ""
setinbetween(keylo1,keylo2,keyhi1,keyhi2,keylo1inbetween,keyhi1inbetween)
	; Allow sub-ranges (i.e. a smaller range completely inside a bigger range) but not allow overlapping ranges.
	; An easy check for this is IF
	;	(a) keylo1 and keyhi1 are both in between keylo2 and keyhi2 OR
	;	(b) keylo1 and keyhi1 are both NOT in between keylo2 and keyhi2
	; In this case we allow. If not, we dont allow this range.
	s keylo1inbetween=((keylo1=keylo2)!(keylo1]keylo2))&(keyhi2]keylo1)
	s keyhi1inbetween=(keyhi1]keylo2)&((keyhi1=keyhi2)!(keyhi2]keyhi1))
	; adjust keylo1inbetween and keyhi1inbetween to take into account a few edge cases
	i (keylo1=keylo2) s keylo1inbetween=$s((keyhi1]keyhi2):0,1:1),keyhi1inbetween=keylo1inbetween
	i (keyhi1=keyhi2) s keylo1inbetween=$s((keylo1]keylo2):1,1:0),keyhi1inbetween=keylo1inbetween
	q
namcoalesce
	n quitLoop,coll,nam1,nam2,namnew,keylo1,keylo2,keyhi1,keyhi2,keylo1inbetween,keyhi1inbetween,prefix,nsubs2,nsubs1
	n tmpnam1,tmpnam2,range
	; ASSERT : i '$d(namrangeoverlap)  zsh "*"  h
	s nam1=""
	f  s nam1=$o(namrangeoverlap(nam1))  q:nam1=""  d  i quitLoop s nam1=""
	. s nam2=nam1
	. s quitLoop=0
	. s coll=+$g(gnams(nams(nam1,"SUBS",0),"COLLATION"))
	. f  s nam2=$o(namrangeoverlap(nam2))  q:nam2=""  d  q:quitLoop
	. . ; if subscripts dont match before the range, there is no chance of a range overlap issue
	. . s range=nams(nam1,"GVNPREFIX") i range'=nams(nam2,"GVNPREFIX") q
	. . ; Below code is similar to "namerangeoverlapcheck2^GDEPARSE". See there for comments
	. . m tmpnam1=nams(nam1) s tmpnam1=nams(nam1)
	. . m tmpnam2=nams(nam2) s tmpnam2=nams(nam2)
	. . d getrangelohikey^GDEPARSE(.tmpnam1,.keylo1,.keyhi1,coll,range)
	. . d getrangelohikey^GDEPARSE(.tmpnam2,.keylo2,.keyhi2,coll,range)
	. . d setinbetween(keylo1,keylo2,keyhi1,keyhi2,.keylo1inbetween,.keyhi1inbetween)
	. . ; the above sets keylo1inbetween and keyhi1inbetween
	. . ; first check overlap case
	. . s prefix=nams(nam1,"GVNPREFIX"),nsubs1=nams(nam1,"NSUBS"),nsubs2=nams(nam2,"NSUBS")
	. . i (keylo1inbetween'=keyhi1inbetween) d
	. . . ; we are guaranteed both nam1 and nam2 map to same region or else NAMRANGEOVERLAP error would have been issued before
	. . . ; ASSERT : i nams(nam1)'=nams(nam2)  zsh "*"  h
	. . . s quitLoop=1
	. . . ; this is a case of overlapping ranges where both ranges map to same region.
	. . . ; create a new super-range that encompasses both overlapping ranges
	. . . i keylo1inbetween d
	. . . . ; merge the ranges [keylo1,keyhi1] and [keylo2,keyhi2] into one super-range [keylo2,keyhi1]
	. . . . s namnew=prefix_nams(nam2,"SUBS",nsubs2-1)_":"_nams(nam1,"SUBS",nsubs1)_")"
	. . . . d add2nams("^"_namnew,nams(nam1),"RANGE")
	. . . . s namrangeoverlap(namnew)=""
	. . . e  d
	. . . . ; merge the ranges [keylo1,keyhi1] and [keylo2,keyhi2] into one super-range [keylo1,keyhi2]
	. . . . s namnew=prefix_nams(nam1,"SUBS",nsubs1-1)_":"_nams(nam2,"SUBS",nsubs2)_")"
	. . . . d add2nams("^"_namnew,nams(nam1),"RANGE")
	. . . . s namrangeoverlap(namnew)=""
	. . . k nams(nam1),namrangeoverlap(nam1)
	. . . k nams(nam2),namrangeoverlap(nam2)
	. . ; next check if [keylo1,keyhi1] is completely inside [keylo2,keyhi2]
	. . e  i (keylo1inbetween) d
	. . . s quitLoop=1
	. . . ; keylo1 and keyhi1 are both in between keylo2 and keyhi2
	. . . ; i.e. [keylo1,keyhi1] lies completely inside [keylo2,keyhi2]
	. . . i nams(nam1)=nams(nam2) d
	. . . . ; a sub-range lies completely inside a super-range and both map to same region
	. . . . ; safely delete the sub-range
	. . . . k nams(nam1),namrangeoverlap(nam1)
	. . . e  d
	. . . . ; a sub-range lies completely inside a super-range and both map to different regions
	. . . . ; split range [keylo2,keyhi2] into two sub-ranges [keylo2,keylo1] and [keyhi1,keyhi2]
	. . . . ; create range [keylo2,keylo1]
	. . . . i keylo2'=keylo1 d
	. . . . . s namnew=prefix_nams(nam2,"SUBS",nsubs2-1)_":"_nams(nam1,"SUBS",nsubs1-1)_")"
	. . . . . d add2nams("^"_namnew,nams(nam2),"RANGE")
	. . . . . s namrangeoverlap(namnew)=""
	. . . . ; create range [keyhi1,keyhi2]
	. . . . i keyhi1'=keyhi2 d
	. . . . . s namnew=prefix_nams(nam1,"SUBS",nsubs1)_":"_nams(nam2,"SUBS",nsubs2)_")"
	. . . . . d add2nams("^"_namnew,nams(nam2),"RANGE")
	. . . . . s namrangeoverlap(namnew)=""
	. . . . ; kill   range [keylo2,keyhi2]
	. . . . k nams(nam2),namrangeoverlap(nam2)
	. . ; next check if [keylo2,keyhi2] is completely inside [keylo1,keyhi1]
	. . e  i ((keylo2=keylo1)!(keylo2]keylo1))&(keyhi1]keylo2) d
	. . . i nams(nam1)=nams(nam2) d
	. . . . ; a sub-range lies completely inside a super-range and both map to same region
	. . . . ; safely delete the sub-range
	. . . . k nams(nam2),namrangeoverlap(nam2)
	. . . . ; since only nam2 is killed, and nam1 is untouched, continue in the nam2 loop (i.e. dont set quitLoop to 1)
	. . . e  d
	. . . . ; a sub-range lies completely inside a super-range and both map to different regions
	. . . . ; split range [keylo1,keyhi1] into two sub-ranges [keylo1,keylo2] and [keyhi2,keyhi1]
	. . . . ; create range [keylo1,keylo2]
	. . . . i keylo1'=keylo2 d
	. . . . . s namnew=prefix_nams(nam1,"SUBS",nsubs1-1)_":"_nams(nam2,"SUBS",nsubs2-1)_")"
	. . . . . d add2nams("^"_namnew,nams(nam1),"RANGE")
	. . . . . s namrangeoverlap(namnew)=""
	. . . . ; create range [keyhi2,keyhi1]
	. . . . i keyhi2'=keyhi1 d
	. . . . . s namnew=prefix_nams(nam2,"SUBS",nsubs2)_":"_nams(nam1,"SUBS",nsubs1)_")"
	. . . . . d add2nams("^"_namnew,nams(nam1),"RANGE")
	. . . . . s namrangeoverlap(namnew)=""
	. . . . ; kill   range [keylo1,keyhi1]
	. . . . k nams(nam1),namrangeoverlap(nam1)
	. . . . s quitLoop=1
	. . ; next check if [keylo1,keyhi1] is immediately followed by [keylo2,keyhi2] and map to same region
	. . ; in this case merge the two into one super-range [keylo1,keyhi2]
	. . e  i (keyhi1=keylo2)&(nams(nam1)=nams(nam2)) d
	. . . s quitLoop=1
	. . . ; create range [keylo1,keyhi2]
	. . . s namnew=prefix_nams(nam1,"SUBS",nsubs1-1)_":"_nams(nam2,"SUBS",nsubs2)_")"
	. . . d add2nams("^"_namnew,nams(nam1),"RANGE")
	. . . s namrangeoverlap(namnew)=""
	. . . ; kill range   [keylo1,keyhi1]
	. . . k nams(nam1),namrangeoverlap(nam1)
	. . . ; kill range   [keylo2,keyhi2]
	. . . k nams(nam2),namrangeoverlap(nam2)
	. . ; next check if [keylo2,keyhi2] is immediately followed by [keylo1,keyhi1] and map to same region
	. . ; in this case merge the two into one super-range [keylo2,keyhi1]
	. . e  i (keyhi2=keylo1)&(nams(nam1)=nams(nam2)) d
	. . . s quitLoop=1
	. . . ; create range [keylo2,keyhi1]
	. . . s namnew=prefix_nams(nam2,"SUBS",nsubs2-1)_":"_nams(nam1,"SUBS",nsubs1)_")"
	. . . d add2nams("^"_namnew,nams(nam2),"RANGE")
	. . . s namrangeoverlap(namnew)=""
	. . . ; kill range   [keylo2,keyhi2]
	. . . k nams(nam2),namrangeoverlap(nam2)
	. . . ; kill range   [keylo1,keyhi1]
	. . . k nams(nam1),namrangeoverlap(nam1)
	k namrangeoverlap  ; now that all sub-ranges and/or overlapping range coalesces have been automatically taken care of
	q
isplusplus(currMap,currMapLen)
	n c,i
	; currMap should be a map entry corresponding to a subscripted name (i.e. contain ZERO in it)
	; Now that we know this is a subscripted gvn, check if the last subscript is of the ++ form.
	; At a high level, this is the case if the last byte is 0x01. But there is more to it which can be
	; found in a comment below (search for MAP2NAM in a comment above that also mentions the ++ form).
	f i=currMapLen:-1:1 s c=$ze(currMap,i) q:c'=ONE
	s i=(currMapLen-i)
	; cases for the $s below are "", ""++, 0x01 0x01 byte sequence in a string subscript, unpaired 0x01 at end of subscript
	; if c=ZERO, then i is guaranteed to be either 1 or 2, nothing more. (1 implies "", 2 implies ""++)
	q $s((c=ZERO):$s((1=i):0,1:1),(0=(i#2)):0,1:1)
	q
