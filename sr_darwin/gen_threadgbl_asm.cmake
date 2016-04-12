
file(STRINGS ${gtmthreadgblasmaccess} asmaccesstypes REGEX "^[A-Za-z_]+")
foreach(asmaccess ${asmaccesstypes})
  string(REGEX REPLACE "^([A-Za-z_]+)[^A-Za-z_].*$" "ggo_\\1" asm "${asmaccess}")
  file(STRINGS ${gtmthreadgblasmhdr} asmdef REGEX ${asm})
  file(WRITE ${gtmthreadgblasmfile} "${asmdef}\n")
endforeach()
