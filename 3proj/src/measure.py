#!/usr/bin/env python2
#author: Jan Wrona
#email: <xwrona00@stud.fit.vutbr.cz>

import numpy as np
import subprocess, math, sys

int_range = 2**31
n = p = int(math.sqrt(float(sys.argv[1])))
reps = 3

#print header
print '#n =', n, '\tp =', p, '\treps =', reps
print 'elements', 'time'

for m in range(1, 100001, 1000):
    durations = []
    for i in range(reps):
        #generate input
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
        try:
            durations.append(float(stdout))
        except Exception as e:
            print e
            exit(1)

    print n * m + p * m, np.average(durations)
