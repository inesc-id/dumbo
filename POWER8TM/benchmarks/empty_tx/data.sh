

workspace=$1
if [ $4 == "no_random" ];then
resultsdir=$2
runsdir=$resultsdir/runs
else
DATE=$(date +%b%d-%H%M)
resultsdir=$2/$DATE
mkdir -p $resultsdir/runs
runsdir=$resultsdir/runs
mkdir $resultsdir/plots
mkdir $resultsdir/summary
echo $3 >> $resultsdir/desc.txt
fi




#name of the backend folder
backends[1]="htm-sgl"
backends[2]="p8tm-si"
backends[3]="p8tm"
backends[4]="tinystm"




#type of benchmark
benchmarks[1]="hashmap-lowcap-hicon-u10"
benchmarks[2]="hashmap-lowcap-hicon-u50"
benchmarks[3]="hashmap-lowcap-hicon-u90"
benchmarks[4]="hashmap-lowcap-lowcon-u10"
benchmarks[5]="hashmap-lowcap-lowcon-u50"
benchmarks[6]="hashmap-lowcap-lowcon-u90"
benchmarks[7]="hashmap-hirocap-hicon-u10"
benchmarks[8]="hashmap-hirocap-hicon-u50"
benchmarks[9]="hashmap-hirocap-hicon-u90"
benchmarks[10]="hashmap-hirocap-lowcon-u10"
benchmarks[11]="hashmap-hirocap-lowcon-u50"
benchmarks[12]="hashmap-hirocap-lowcon-u90"
benchmarks[13]="hashmap-hirwcap-hicon-u10"
benchmarks[14]="hashmap-hirwcap-hicon-u50"
benchmarks[15]="hashmap-hirwcap-hicon-u90"
benchmarks[16]="hashmap-hirwcap-lowcon-u10"
benchmarks[17]="hashmap-hirwcap-lowcon-u50"
benchmarks[18]="hashmap-hirwcap-lowcon-u90"



#app for benchmark
bStr[1]="hashmap"


#parameters to pass for the respective benchmark configuration
params[1]="-u10 -i50 -b1 -r50 -d5000000 -q 1 -y 1"
params[2]="-u50 -i50 -b1 -r50 -d5000000 -q 1 -y 1"
params[3]="-u90 -i50 -b1 -r50 -d5000000 -q 1 -y 1"
params[4]="-u10 -i50000 -b1000 -r50000 -d5000000 -q 1 -y 1"
params[5]="-u50 -i50000 -b1000 -r50000 -d5000000 -q 1 -y 1"
params[6]="-u90 -i50000 -b1000 -r50000 -d5000000 -q 1 -y 1"
params[7]="-u10 -i2000 -b10 -r2000 -d5000000 -q 1 -y 1"
params[8]="-u50 -i2000 -b10 -r2000 -d5000000 -q 1 -y 1"
params[9]="-u90 -i2000 -b10 -r2000 -d5000000 -q 1 -y 1"
params[10]="-u10 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"
params[11]="-u50 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"
params[12]="-u90 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"
params[13]="-u10 -i2000 -b1 -r2000 -d5000000 -q 1 -y 1"
params[14]="-u50 -i2000 -b1 -r2000 -d5000000 -q 1 -y 1"
params[15]="-u90 -i2000 -b1 -r2000 -d5000000 -q 1 -y 1"
params[16]="-u10 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"
params[17]="-u50 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"
params[18]="-u90 -i200000 -b1000 -r200000 -d5000000 -q 1 -y 1"

#values to use for retries
cms[1]="0 2 1"
cms[2]="10 5 1"


wait_until_finish() {
    pid3=$1
    echo "process is $pid3"
    LIMIT=90
    for ((j = 0; j < $LIMIT; ++j)); do
        kill -s 0 $pid3
        rc=$u
        if [[ $rc != 0 ]] ; then
            echo "returning"
            return;
        fi
        sleep 1s
    done
    kill -9 $pid3
}



for c in 1 2
do
	for b in 1 4 7 8 10 11
	do
		for cm in 1
		do
			cd $workspace;
			cd benchmarks/datastructures/
			bash build-datastructures.sh ${backends[$c]} ${cms[$cm]}
			cd ${bStr[$b]};
			for t in 1 2 4 8 16 32 40 80 #number of threads
			do
        	    		for a in 1 2 3 4 5 #number of attempts for each run
            			do
                			echo "${benchmarks[$b]} | ${backends[$c]}-${cms[$cm]} | threads $t | attempt $a"
					echo "${benchmarks[$b]} | ${backends[$c]}-${cms[$cm]} | threads $t | attempt $a" >> $resultsdir/desc.txt
                			./${bStr[$b]} ${params[$b]} -n $t > $runsdir/${benchmarks[$b]}-${backends[$c]}-$cm-$t-$a.data 2> $runsdir/${benchmarks[$b]}-${backends[$c]}-$cm-$t-$a.err &
					pid3=$!
					wait_until_finish $pid3
					wait $pid3
        	        		rc=$?
                			if [[ $rc != 0 ]] ; then
                    				echo "Error within: | ${benchmarks[$b]} | ${backends[$c]}-${cms[$cm]} | threads $t | attempt $a" >> $runsdir/error.out
               				fi
				done
			done
                        #cd ..
		done
	done
done


exit 0