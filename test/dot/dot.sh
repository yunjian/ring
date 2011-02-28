#!/usr/bin/sh

gss="gs -sDEVICE=pswrite -sOutputFile=output.ps -dNOPAUSE -dBATCH "
test="/cygdrive/c/Research/Code/ring-dev/test/dot"
for i in `ls $test/*.dot | cut -d '.' -f 1`; {
    echo ":: $i"
    gss=${gss}" "${i}".ps"
    /home/wjiang/graphviz-2.2/bin/dot -Tps -o${i}.ps ${i}.dot
}
exec ${gss}
