#!/usr/bin/python

import sys

filename = sys.argv[1]

f = open(filename, "r")
start = -1
for l in f.readlines() :
    ws = l.split()
    if start == -1:
        start = float(ws[0])
    ws[0] = str((float(ws[0]) - start))
    print ' '.join(ws)



