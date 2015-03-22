#!/usr/bin/env bash
#author: Jan Wrona
#email: xwrona00@stud.fit.vutbr.cz

OUTFILE=out.txt
RUNS=10

echo "probmel_size walltime cputime" > "${OUTFILE}"
for EXP in {2..31}
do
    printf "2^${EXP}: "
    WALLTIME=0.0
    CPUTIME=0.0

    for RUN in `seq 1 ${RUNS}`
    do
	printf "${RUN}, "
	./test.sh 2^${EXP} | cut "-d " -f 2 > out
	WALLTIME="`head -1 out`+`echo ${WALLTIME}`"
	CPUTIME="`tail -1 out`+`echo ${CPUTIME}`"

	WALLTIME=`echo "${WALLTIME}" | bc -l`
	CPUTIME=`echo "${CPUTIME}" | bc -l`
    done
    printf "\n"
    rm -f out

    AVG_WALLTIME=`echo "${WALLTIME} / ${RUNS}" | bc -l`
    AVG_CPUTIME=`echo "${CPUTIME} / ${RUNS}" | bc -l`

    echo "2^${EXP} ${AVG_WALLTIME} ${AVG_CPUTIME}" >> "${OUTFILE}"
    echo "wall: total = ${WALLTIME}, avg = ${AVG_WALLTIME}"
    echo "cpu:  total = ${CPUTIME}, avg = ${AVG_CPUTIME}"
done
