#!/bin/bash

NR_SPU=$2
NR_FRAMES=$1
FILE=$3

#echo "nrspu " $NR_SPU
#echo "nrframes " $NR_FRAMES
#echo "method " $METHOD
#echo "file " $FILE

echo "rulare normala" >> $FILE
./program /tmp/images result/ $NR_FRAMES $NR_SPU 0 >> $FILE
echo "rulare cu 2 linii" >> $FILE
./program /tmp/images result1/ $NR_FRAMES $NR_SPU 1 >> $FILE
echo "rulare double buffering" >> $FILE
./program /tmp/images result2/ $NR_FRAMES $NR_SPU 2 >> $FILE
echo "rulare liste dma" >> $FILE
./program /tmp/images result3/ $NR_FRAMES $NR_SPU 3 >> $FILE
