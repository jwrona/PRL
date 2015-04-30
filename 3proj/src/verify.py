#!/usr/bin/env python2
#author: Jan Wrona
#email: <xwrona00@stud.fit.vutbr.cz>

import numpy as np
import subprocess, math, sys

int_range = 2**31

cores = int(sys.argv[1])
sq_cores = int(math.sqrt(float(sys.argv[1])))

modes = [
    {'max_n': 1, 'max_m': 1, 'max_p': 1, 'runs': 10}, #1x1 * 1x1
    {'max_n': cores, 'max_m': 1, 'max_p': 1, 'runs': 10}, #row vector * 1x1
    {'max_n': 1, 'max_m': 1, 'max_p': cores, 'runs': 10}, #1x1 * column vector
    {'max_n': sq_cores, 'max_m': 1, 'max_p': sq_cores, 'runs': 10}, #row vector * column vector
    {'max_n': sq_cores, 'max_m': 100000, 'max_p': sq_cores, 'runs': 10} #NxM * MxP
]

for mode in modes:
    print mode
    for i in range(mode['runs']):
        #generate input
        n = np.random.randint(1, mode['max_n'] + 1)
        m = np.random.randint(1, mode['max_m'] + 1)
        p = np.random.randint(1, mode['max_p'] + 1)
        mat1 = np.random.randint(-int_range, int_range, (n, m))
        mat2 = np.random.randint(-int_range, int_range, (m, p))

        #matrix multiplication by numpy
        numpy_res = np.dot(mat1, mat2)

        #store input to mat{1,2} files
        with open('mat1', 'w') as f:
            f.write(str(n) + '\n')
            np.savetxt(f, mat1, fmt='%i')
        with open('mat2', 'w') as f:
            f.write(str(p) + '\n')
            np.savetxt(f, mat2, fmt='%i')

        #call test.sh
        proc = subprocess.Popen(['./test.sh'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        if proc.returncode or stderr:
            print stderr
            exit(proc.returncode)

        #parse test.sh output
        header, matrix = stdout.split('\n', 1)
        test_n, test_p = header.split(':', 1)
        test_n, test_p = int(test_n), int(test_p)
        test_res = np.fromstring(matrix,  dtype=int, sep=' ')
        test_res = test_res.reshape(test_n, test_p)

        #check for same shape and elements values
        if not np.array_equal(numpy_res, test_res):
            print 'Result mismatch:'
            print numpy_res
            print
            print test_res
            exit(1)
