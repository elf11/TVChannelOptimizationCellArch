for i in $(seq 25 25 100)
do
	./run.sh $i $1 out$1$i.txt
done
