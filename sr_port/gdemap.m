;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
;	Copyright 2010, 2013 Fidelity Information Services, Inc	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
map:	;create maps for put and show, names for get and show
PUTMAKE
	k lexnams n t1
	d SHOWMAKE
	s s1=$ztr($zj("",SIZEOF("mident"))," ",$zch(255)),t1=$ztr($zj("",SIZEOF("mident"))," ",$zch(0))
	f  s s2=s1,s1=$o(map(s1),-1),map(s2_$ze(t1,$zl(s2)+1,SIZEOF("mident")))=map(s1) q:s1="$"  k map(s1)
	s map("#)"_$ze(t1,3,SIZEOF("mident")))=map("#)"),map("%"_$ze(t1,2,SIZEOF("mident")))=map("$")
	f s2="#","#)","$" k map(s2)
 	q
;----------------------------------------------------------------------------------------------------------------------------------
SHOWMAKE
	n lexnams s s=""
	f  s s=$o(nams(s)) q:'$zl(s)  d lexins(s)
	s map("$")=nams("*"),map("#")=nams("#")
	s i=1
	f  s i=$o(lexnams(i)) q:'$zl(i)  d showstar(i)
	s s=""
	f  s s=$o(lexnams(0,s),-1) q:'$zl(s)  d pointins(s,lexnams(0,s))
	s s1=$o(map(""),-1)
	f  s s2=s1,s1=$o(map(s1),-1) q:s2="$"  i map(s1)=map(s2) k map(s2)
	q
;----------------------------------------------------------------------------------------------------------------------------------
SHOWNAM
	n lexnams,t1,map
	d SHOWMAKE
	s s1=$ztr($zj("",SIZEOF("mident"))," ",$zch(255)),t1=$ztr($zj("",SIZEOF("mident"))," ",$zch(0))
	f  s s2=s1,s1=$o(map(s1),-1),map(s2)=map(s1) q:s1="$"  k map(s1)
	k map("#") i '$$MAP2NAM(.map) zm gdeerr("GDECHECK")\2*2
 	q
;----------------------------------------------------------------------------------------------------------------------------------
MAP2NAM(list)
	n maxMap,currMap,currMapLen,prevMap,prevMapLen,currReg,prevPrefix,currPrefix
	n namSpc,currNam,currNamLen,stopLoop,i,startMap,midentSize,prevPrevMap
	s currMap=$o(list("")) q:currMap'="#)" 0
	s currMap=$o(list(currMap))
	i currMap="%" s list("$")=list("%")	; if "$" is missing, assign it the same value as "%"
	e  q:currMap'="$" 0
	s maxMap=$ztr($zj("",SIZEOF("mident"))," ",$zch(255)),currMap=$o(list(""),-1) q:currMap'=maxMap 0
	k nams
	f  q:currMap="$"  s prevMap=$o(list(currMap),-1) d  s currMap=prevMap
	. s currReg=list(currMap)
	. ; Note that with the above mapping, all keys in the range [prevMap,currMap) are mapped to currReg.
	. ;	where [ denotes closed interval and ) denotes open interval i.e. prevMap <= key < currMap
	. ; Case (1a) : If currMap contains ")" (e.g. "abc)"), it most likely means prevMap would be "abc".
	. ;		In this case, just add "abc" as a namespace. No more processing needed for currMap.
	. ; Case (1b) : But it is also possible prevMap is not "abc".
	. ;		In this case, do the "abc" namespace addition (as if prevMap was "abc").
	. ;		But also proceed with the current iteration of the for loop as if "currMap" was "abc".
	. s currMapLen=$zl(currMap)
	. i $ze(currMap,currMapLen)=")" d  q:prevMap=namSpc
	. . s namSpc=$ze(currMap,1,currMapLen-1),nams(namSpc)=currReg
	. . i prevMap'=namSpc s currMap=namSpc,currMapLen=currMapLen-1
	. ; Case (3) : If prevMap contains ")" (e.g. "abc)"), then check to see if its previous entry (say prevPrevMap)
	. ; is the same region as currReg. If so, we can coalesce the entire range [prevPrevMap,currMap) into one with the
	. ; exception of "abc" for which add an explicit namespace. Do this for as many ")" prevMap entries that you can find
	. ; as long as its prevPrevMap entry is the same region as currReg. This could potentially coalesce a lot of intervals.
	. ; Note: Need to handle a situation like Case (1b) here too. We do this by keeping prevMap as it is but adjusting
	. ; just prevMapLen to be 1 byte less (to remove trailing ")").
	. s stopLoop=0
	. f  d  q:stopLoop
	. . s prevMapLen=$zl(prevMap)
	. . i $ze(prevMap,prevMapLen)'=")" s stopLoop=1 q
	. . s namSpc=$ze(prevMap,1,$i(prevMapLen,-1))
	. . s prevPrevMap=$o(list(prevMap),-1)
	. . i prevPrevMap'=namSpc  s stopLoop=1 q
	. . s nams(namSpc)=list(prevMap)
	. . i list(prevMap)'=currReg s stopLoop=1 q
	. . s prevMap=$o(list(prevMap),-1)
	. . i prevMap="$" s stoploop=1,prevMapLen=1 q
	. ; Note: At this point prevMap could contain a trailing ")" but in that case prevMapLen would have been adjusted to
	. ; not consider that last byte. As long as all following usages of prevMap are of the form $ze(prevMap,1,prevMapLen)
	. ; we will never see the ")" in prevMap.
	. ; Case (4) : The map entry "currMap" exists and "prevMap" is the previous map entry.
	. ; Determine the namespaces that potentially lie between the two map entries.
	. ; And add them to the "nams" array.
	. f i=1:1:currMapLen  i $ze(currMap,i)'=$ze(prevMap,i) q
	. s matchLen=i-1  ; the length of the maximal common prefix between prevMap and currMap
	. ; Subcase (4a) : matchLen == prevMapLen
	. ;	In this case we are guaranteed that prevMapLen < currMapLen, and we need to add only ONE namespace.
	. ;	Example prevMap="ag", currMap="agk". Here, matchLen=2, prevMapLen=2, currMapLen=3. Add only "ag*".
	. i (matchLen=prevMapLen)&(prevMapLen<currMapLen) s namSpc=$ze(prevMap,1,prevMapLen)_"*",nams(namSpc)=currReg q
	. ; Subcase (4b) : matchLen < prevMapLen
	. ;	Again we are guaranteed that matchLen < currMapLen.
	. ;	Example prevMap="agxy", currMap="ajk". Here, matchLen=1, prevMapLen=4, currMapLen=3
	. ;	In this case, we need to add namespace for "aj*", "ai*", "ah*" as a first step.
	. ;	Whether or not we can add "ag*" depends on an optimization check explained below.
	. ;		If "ag*" can be added, then we add it and are done.
	. ;		If not, then we need to add one or more smaller namespaces under "ag" explained below as the SECOND PART.
	. ;	To explain the optimization check, let us say the "list" variable contains the following.
	. ;		list("ag")="DEFAULT"
	. ;		list("ag)")="STAR2"
	. ;		list("agQ")="STAR1"
	. ;		.
	. ;		.
	. ;		list("agwx")="STAR1"
	. ;		list("agxy")="STAR2"
	. ;		list("ajk")="STAR1"	<-- currMap points here
	. ;	Let us say currMap = "ajk". So prevMap = "agxy". In this case, we would definitely add "aj*", "ai*" and "ah*".
	. ;	To determine whether we need to add "ag*", we take a look at where the start of "ag*" namespace maps to.
	. ;	And that would be the entry following list("ag") i.e. list("ag)"). This maps to STAR2 but since it is a point
	. ;	mapping (no * specification), we can skip that as an exception. The next entry list("agQ") maps to STAR1 region.
	. ;	Since this is the same region as the region of currMap, we can safely add just one namespace "ag*" to cover
	. ;	the entire range from list("ag)") to list("ajk") with entries in between covering sub-namespaces that are
	. ;	exceptions (i.e. those which dont map to STAR1). If list("agQ") maps to a region other than STAR1, we need to
	. ;	add more sub-namespaces under the "ag*" namespace which is explained in the SECOND PART below.
	. ;	The FIRST PART of adding "aj*", "ai*" and "ah*" is taken care of by the simple for loop below.
	. ;	There is one exception though.
	. ;		a) If prevMap="ER1" and currMap="ER2", matchLen=2, prevMapLen=3, currMapLen=3
	. ;		   But we should not add the "ER2*" namespace.
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
	. s startMap=$$findStartMap(currPrefix)
	. i list(startMap)=currReg  s i=prevMapLen  ; set i to force skip of SECOND PART of processing below.
	. ;	The SECOND PART does the additional "ag" specific processing. Since the stop point is "agxy", we need to add
	. ;	namespaces "agz*", "agy*" first. And then by the same reasoning (for not adding "ag*" in the FIRST PART),
	. ;	we cannot add "agx*" unless the optimization check says it is okay to do so. Assuming it says that is not okay,
	. ;	we go further down by adding "agxz*" and then "agxy*" and then stop.
	. f  s i=i+1  q:i>prevMapLen  d
	. . s currPrefix=prevPrefix_"z",prevPrefix=$ze(prevMap,1,i)
	. . f  q:currPrefix=prevPrefix  s namSpc=currPrefix_"*",nams(namSpc)=currReg,currPrefix=$$lexprev(currPrefix)
	. . ; Do optimization check at each sub-namespaces level. If it succeeds, stop processing any higher level sub-namespaces.
	. . s startMap=$$findStartMap(currPrefix)
	. . i list(startMap)=currReg  s i=prevMapLen  q  ; set i to force quit out of for loop
	. s namSpc=currPrefix_"*",nams(namSpc)=currReg
	; Update "nams" variable to contain # of elements in "nams" array
	; Take this opportunity to remove redundant namespaces.
	; Example : If "a*" and "ab*" both map to the same region, the namespace "ab*" can be safely removed.
	; But if "a*" maps to AREG, "aa*" maps to BREG, and "aaa*" maps to AREG, we cannot remove "aaa*" because
	; "a*" and "aaa*" maps to the same reg. This is because there is a more restrictive mapping "aa*" which
	; maps to a different region than "aaa*". That should prevail.
	; Similarly if "ab*" and "abc" both map to the same region, the namespace "abc" can be safely removed.
	; But if "abc*" also is mapped and to a different region than "abc", then "abc" cannot be removed.
	s currNam="",nams=0
	; With SIZEOF("mident")=32, we allow a max of 31-byte global name specifications.
	; But with SIZEOF("mident")=8 (for older versions with no longnames support, we allow a max of 8-byte global names.
	; Handle this 1-byte discrepancy for the 8-byte case by setting the variable midentSize accordingly.
	s midentSize=$s(SIZEOF("mident")=8:9,1:SIZEOF("mident"))
	s nams("*")=list("$")
	f  s currNam=$o(nams(currNam)) q:currNam=""  s nams=nams+1  d
	. s currNamLen=$zl(currNam)
	. s currReg=nams(currNam)
	. s killed=0,quitLoop=0
	. f i=$s($ze(currNam,currNamLen)="*":(currNamLen-2),1:currNamLen):-1:0  d  q:quitLoop
	. . s currPrefix=$ze(currNam,1,i)_"*"
	. . s prevReg=$g(nams(currPrefix))
	. . i (""'=prevReg) d
	. . . s quitLoop=1
	. . . i currReg=prevReg k nams(currNam) s nams=nams-1,killed=1
	. i 'killed,currNamLen'<midentSize k nams(currNam)  s nams($ze(currNam,1,midentSize-1))=currReg
	; Do some final cleanup
	s nams("#")=list("#)"),nams=nams+1
	q 1
;----------------------------------------------------------------------------------------------------------------------------------
lexins:(s)
	n x,l
	i s["*" s l=$zl(s)
	e       s l=0
	s lexnams(l,s)=nams(s)
	q
showstar:(i)
	s j=""
	f  s j=$o(lexnams(i,j),-1) q:'$zl(j)  d starins($ze(j,1,$zl(j)-1),lexnams(i,j))
	q
starins:(s,reg)
	n next
	s next=$$lexnext(s)
	i $zl(next),'$d(map(next)) s map(next)=map($o(map(next),-1))
	s map(s)=reg
	q
pointins:(s,reg)
	n next
	s next=$$alphnext(s)
	i $zl(next),'$d(map(next)) s map(next)=map($o(map(next),-1))
	s map(s)=reg
	q
alphnext:(s)
	i $zl(s)=MAXNAMLN q $$lexnext(s)
	e  q s_")"
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
	n len,last,prio
	s len=$zl(s),last=$ze(s,len)
	s s=$ze(s,1,len-1),prio=$zch($za(last)-1)
	i prio?1AN q s_prio
	i prio]"Z" q s_"Z"
	i prio]"9" q:len=1 "%" q s_"9"
	q ""
findStartMap:(key)
	n startMap
	s startMap=$o(list(key))
	s startMapLen=$zl(startMap)
	i ($ze(startMap,startMapLen)=")")&($ze(startMap,1,startMapLen-1)=key) s startMap=$o(list(startMap))
	q startMap
