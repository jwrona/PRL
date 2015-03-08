#!/usr/bin/env bash
#author: Jan Wrona
#email: xwrona00@stud.fit.vutbr.cz

USAGE="Usage: ${0} problem_size"
MPIPATH="/usr/local/share/OpenMPI/bin/"
NAME="pms"

if [[ ! $1 =~ ^[0-9]+(\^[0-9]+)?$ ]]
then 
    echo "${USAGE}"
    exit 1
fi

PS=`echo "${1}" | bc`
PS="${PS%.*}"
LOG_FLOAT=`echo "l(${PS})/l(2)" | bc -l`
LOG="${LOG_FLOAT%.*}"

if [ ${PS} -ne `echo "2^${LOG}" | bc` ]
then
    echo "Error: problem size has to be a power of two."
    exit 1
fi

CPUS=$((LOG+1))

#create random input file
dd if=/dev/urandom bs=1 count="${PS}" of=numbers 2> /dev/null

#hexdump numbers -ve '/1 "%d""\n"' > numbers.txt
#/usr/bin/time -f "%E" sort -n numbers.txt > sorted.txt

#compilation
"${MPIPATH}mpic++" -DDEBUG -o "${NAME}" "${NAME}.cpp"

#spusteni
time "${MPIPATH}mpirun" -np "${CPUS}" "${NAME}"

#uklid
rm -f "${NAME}" numbers
