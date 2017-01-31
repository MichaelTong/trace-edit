#!/usr/bin/env python
#title           :trace-editor.py
#description     :process a trace disk
#author          :Vincentius Martin, Michael(Hao) Tong
#date            :-, 20170124
#version         :0.1, 0.2.1
#usage           :python trace-editor.py
#notes           :
#python_version  :2.7.5+
#changelog.0.2.1 :Change functions to rerate/resize writes/reads
#==============================================================================

from random import randint

# input: request list (list), modify the size x times (float)
def rresize(reqlist, times):
  for request in reqlist:
    if int(request[4]) == 1:
      request[3] = ('%f' % (times * float(request[3]))).rstrip('0').rstrip('.')
  return reqlist

# input: request list (list), modify the size x times (float)
def wresize(reqlist, times):
  for request in reqlist:
    if int(request[4]) == 0:
      request[3] = ('%f' % (times * float(request[3]))).rstrip('0').rstrip('.')
  return reqlist

# input: request list (list), modify the size x rate times (float)
def modifyrRate(reqlist, rate):
  i = 0
  while i < len(reqlist):
    #if float(reqlist[i][0]) * rate > 300000:
    #  del reqlist[i:len(reqlist)]
    #  break
    if int(reqlist[i][4]) == 1:
      reqlist[i][0] = '%.3f' % (1.0 / rate * float(reqlist[i][0]))
    i += 1
  return reqlist

# input: request list (list), modify the size x rate times (float)
def modifywRate(reqlist, rate):
  i = 0
  while i < len(reqlist):
    #if float(reqlist[i][0]) * rate > 300000:
    #  del reqlist[i:len(reqlist)]
    #  break
    if int(reqlist[i][4]) == 0:
      reqlist[i][0] = '%.3f' % (1.0 / rate * float(reqlist[i][0]))
    i += 1
  return reqlist

#interval: in ms; size in KB
def insertIO(reqlist,size,interval,iotype):
    insert_time = interval
    maxoffset = int(max(reqlist, key=lambda x: int(x[2]))[2])
    i = 0
    while i < len(reqlist):
        if float(reqlist[i][0]) > insert_time: #7190528,7370752
            reqlist.insert(i,['%.3f' % insert_time,str(0),str(randint(0,maxoffset)),str(size * 2),str(iotype)])
            insert_time += interval
            i += 1
        i += 1
    return reqlist

def printRequestList(requestlist, filename):
  #chagnelog.0.2.1: since requests are rerated for reads and writes independently, it is possible that
  #timestamp are in wrong order
  out = open("out/" + filename + "-modified.trace" , 'w')
  requestlist.sort(key=lambda val:float(val[0]))#sort by timestamp
  
  for elm in requestlist:
    out.write(str(elm[0]) + " " + str(elm[1]) + " " + str(elm[2]) + " " + str(elm[3]) + " " + str(elm[4])+"\n")
  out.close()

