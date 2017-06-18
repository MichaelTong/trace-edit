#!/usr/bin/env python
#title           :characteristic.py
#description     :Get the characteristic of a trace
#author          :Cheng Wu, Michael(Hao) Tong
#date            :20150519, 20170124
#version         :0.1, 0.2.1
#usage           :
#notes           :
#python_version  :2.7.5+
#precondition    :ordered
#changelog.0.2.1 :Change output to latex code, an organized table
#==============================================================================

import math,numpy
from collections import defaultdict

def convertToByteStr(size_in_byte):
    size_byte = int(float(size_in_byte))
    ret_str = ""
    if size_byte > (1024 ** 3):
      ret_str += "%.2f" % (float(size_byte) / (1024 ** 3)) + " GB"
    elif size_byte > (1024 ** 2):
      ret_str += "%.2f" % (float(size_byte) / (1024 ** 2)) + " MB"
    elif size_byte > 1024:
      ret_str += "%.2f" % (float(size_byte) / 1024) + " KB"
    else:
      ret_str += str(size_byte) + "B"
    return ret_str

def getTraceInfo(tracefile):
  out = open("out/" + tracefile + "-char.tex", 'w')
  traceIn = open("in/" + tracefile)

  ioCount = 0
  writeCount = 0
  randomWriteCount = 0
  readCount = 0

  writeSize = []
  readSize = []

  timeInterval = []
  #using -1 distinguish first time and other
  firstTime = -1
  lastTime = -1
  currentTime = 0

  #using -1 distinguish first time and other
  lastBlockNo = -1
  lastBlockCount = 0
  #-------------
  #total size
  total_write_size = 0
  total_read_size = 0
  
  #size bucket
  write_countbucket = [0] * 7 # 32, 64, 128, 256, 512, 1024, 1024+
  read_countbucket = [0] * 7
  
  write_sizebucket = [0] * 7
  read_sizebucket = [0] * 7
  
  write_max_size = -1
  write_min_size = -1
  read_max_size = -1
  read_min_size = -1

  write_max_offset = -1
  write_min_offset = -1

  read_max_offset = -1
  read_min_offset = -1

  max_offset = -1
  min_offset = -1

  #start and end time
  starttrace_time = None 
  endtrace_time = None

  #small random writes
  small_random_writes = 0
  #-------------
  
  #written block number
  written_block = defaultdict(int)
 

  for line in traceIn:
    ioCount += 1
    words = line.split(" ")
    ioType = int(words[4])

    if firstTime == -1:
        firstTime = float(words[0])

    if ioType == 0:
      total_write_size += (float(words[3]) * 0.5)
      writeCount+=1
      writeSize.append(float(words[3])*512)
      if write_max_size < 0 or write_max_size < float(words[3]):
        write_max_size = float(words[3])
      if write_min_size < 0 or write_min_size > float(words[3]):
        write_min_size = float(words[3])
      if write_max_offset < 0 or write_max_offset < float(words[2]):
        write_max_offset = int(words[2])
      if write_min_offset < 0 or write_min_offset > float(words[2]):
        write_min_offset = int(words[2])
      if lastBlockNo != -1:
        if (lastBlockNo + lastBlockCount) != int(words[2]):
          randomWriteCount += 1
          if lastBlockCount * 0.5 <= 32: #KB
            small_random_writes += 1
      lastBlockNo = int(words[2])
      lastBlockCount = float(words[3])
    elif ioType == 1:
      total_read_size += (float(words[3]) * 0.5)
      readCount+=1
      readSize.append(float(words[3])*512)

      if read_max_size < 0 or read_max_size < int(words[3]):
        read_max_size = float(words[3])
      if read_min_size < 0 or read_min_size > int(words[3]):
        read_min_size = float(words[3])
      if read_max_offset < 0 or read_max_offset < int(words[2]):
        read_max_offset = int(words[2])
      if read_min_offset < 0 or read_min_offset > int(words[2]):
        read_min_offset = int(words[2])



    if max_offset < 0 or max_offset < int(words[2]):
      max_offset = int(words[2])
    if min_offset < 0 or min_offset > int(words[2]):
      min_offset = int(words[2])
    if float(words[0]) != -1:
      currentTime = float(words[0])
      timeInterval.append(currentTime - lastTime)
      lastTime = currentTime

    #---start of countbucket part---
    #sizes in countbucket are in KB
    bucket_slot = int(math.ceil(math.log(float(words[3]) * 0.5, 2))) - 5
    bucket_slot = max(0,bucket_slot) and min(6,bucket_slot)

    if ioType == 0:
      write_countbucket[bucket_slot] += 1
      write_sizebucket[bucket_slot] += (float(words[3]) * 512)
    else: #ioType == 1
      read_countbucket[bucket_slot] += 1
      read_sizebucket[bucket_slot] += (float(words[3]) * 512)
    #---end of countbucket part---

    #---start of written block part---
    if ioType == 0: #if write
      for writtenblk in range(int(words[2]), int(int(words[2]) + float(words[3]))):
        written_block[writtenblk] += 1
    #---end of written block part---
    #Note start and end trace time
    if starttrace_time == None:
      starttrace_time = float(words[0])
    endtrace_time = float(words[0])
    #-----------------------------
  totalsec = float(endtrace_time-starttrace_time) / 1000

  outstr = ""
  basic_size_offset = """
% Please add the following required packages to your document preamble:
% \usepackage{{graphicx}}
\\begin{{table}}[]
\centering
\caption{{Characteristic of {}}}
\label{{char-{}}}
\\resizebox{{\\textwidth}}{{!}}{{%
\\begin{{tabular}}{{|l|l|l|l|}}
\\hline
\multicolumn{{4}}{{|c|}}{{\\textbf{{Characteristic: {}}}}}                                            \\\\ \\hline
\multicolumn{{2}}{{|c|}}{{\\textbf{{BASICS}}}}           & \multicolumn{{2}}{{c|}}{{\\textbf{{SIZE}}}}             \\\\ \\hline
total time                  & {:.2f}             & max write                     & {}           \\\\ \\hline
IO count                    & {}            	 & min write                     & {}           \\\\ \\hline
IO/s                        & {:.2f}             & max read                      & {}           \\\\ \\hline
\%write                     & {:.2f}             & min read                      & {}           \\\\ \\hline
write(KB)/s                 & {:.2f}             & \multicolumn{{2}}{{c|}}{{\\textbf{{OFFSET}}}}           \\\\ \\hline
write/s                     & {:.2f}             & max write                     & {}           \\\\ \\hline
avg write(KB)               & {:.2f}             & min write                     & {}           \\\\ \\hline
\%read                      & {:.2f}             & span write                    & {}           \\\\ \\hline             
read(KB)/s                  & {:.2f}             & max read                      & {}           \\\\ \\hline
read/s                      & {:.2f}             & min read                      & {}           \\\\ \\hline
avg read(KB)                & {:.2f}     	 & span read                     & {}           \\\\ \\hline
"""

  outstr += basic_size_offset.format(tracefile, tracefile, tracefile,
		                     (lastTime - firstTime)/1000, convertToByteStr(write_max_size * 512),
		                     ioCount, convertToByteStr(write_min_size * 512),
		                     float(ioCount) / totalsec, convertToByteStr(read_max_size * 512),
		                     float(writeCount) / float(ioCount) * 100, convertToByteStr(read_min_size * 512),
		                     float(total_write_size) / totalsec,
		                     float(writeCount) / totalsec, convertToByteStr(write_max_offset * 512),
                                     float(total_write_size) / writeCount, convertToByteStr(write_min_offset*512),
		                     float(readCount) / float(ioCount) * 100, convertToByteStr((write_max_offset - write_min_offset)*512),
		                     float(total_read_size) / totalsec, convertToByteStr(read_max_offset * 512),
		                     float(readCount) / totalsec, convertToByteStr(read_min_offset*512),
		                     float(total_read_size) / readCount, convertToByteStr((read_max_offset - read_min_offset)*512))

  sizebucket = """
\multicolumn{{4}}{{|c|}}{{\\textbf{{SIZE BUCKET(KB) {{[}}0-32,32-64,64-128,128-256,256-512,512-1024{{]}}}}}} \\\\ \\hline
write count                  & \multicolumn{{2}}{{l|}}{{{}}}                      & {}                   \\\\ \\hline
read count                 & \multicolumn{{2}}{{l|}}{{{}}}                      & {}                   \\\\ \\hline
write size                   & \multicolumn{{2}}{{l|}}{{{}}}                      & {}                   \\\\ \\hline
read size                  & \multicolumn{{2}}{{l|}}{{{}}}                      & {}                   \\\\ \\hline
"""
  str_read_sizebucket = "["
  for size in read_sizebucket:
    str_read_sizebucket += convertToByteStr(size) + ", "
  str_read_sizebucket = str_read_sizebucket[:-2] + "]"

  str_write_sizebucket = "["
  for size in write_sizebucket:
    str_write_sizebucket += convertToByteStr(size) + ", "
  str_write_sizebucket = str_write_sizebucket[:-2] + "]"


  outstr += sizebucket.format(str(write_countbucket), sum(write_countbucket),
			      str(read_countbucket), sum(read_countbucket),
			      str_write_sizebucket, convertToByteStr(str(sum(write_sizebucket))),
			      str_read_sizebucket,convertToByteStr(str(sum(read_sizebucket))))

  small_big = """
\multicolumn{{4}}{{|c|}}{{\\textbf{{Small IO (\\textless= 32KB) v.s. Big IO(\\textgreater32KB)}}}}         \\\\ \\hline
rand writes                 & {}             & small rand writes/s           & {:.2f}           \\\\ \\hline
big writes                  & {}             & big writes/s                  & {:.2f}           \\\\ \\hline
\multicolumn{{2}}{{|l|}}{{score(\#big/\#small)}}      & \multicolumn{{2}}{{l|}}{{{:.2f}}}                       \\\\ \\hline
"""

  outstr += small_big.format(small_random_writes, float(small_random_writes) / totalsec,
		             sum(write_countbucket[1:len(write_countbucket)]),
		             float(sum(write_countbucket[1:len(write_countbucket)])) / totalsec,
		             float(sum(write_countbucket[1:len(write_countbucket)])) / write_countbucket[0] if write_countbucket[0] > 0 else 9999)

  plot = """
\multicolumn{{4}}{{|c|}}{{\\textbf{{Whisker plot: min, 25\%, med, 75\%, max}}}}                          \\\\ \\hline
write(B)                     & \multicolumn{{3}}{{l|}}{{{}}}                                         \\\\ \\hline
read(B)                    & \multicolumn{{3}}{{l|}}{{{}}}                                         \\\\ \\hline
interval(ms)                & \multicolumn{{3}}{{l|}}{{{}}}                                         \\\\ \\hline
same write                  & \multicolumn{{3}}{{l|}}{{{}}}                                         \\\\  \\hline
\end{{tabular}}%
}}
\end{{table}}
"""

  writeSize.sort()
  readSize.sort()
  timeInterval.sort()
  overwritten_blk_count = numpy.array(written_block.values())
  
  outstr += plot.format(str(writeSize[0]) + ", "+ str(writeSize[(writeCount-1)/4]) + ", " +str(writeSize[(writeCount-1)/2]) + ", "+str(writeSize[3*(writeCount-1)/4])+", "+str(writeSize[writeCount-1]),
			str(readSize[0]) + ", "+ str(readSize[(readCount-1)/4]) + ", " +str(readSize[(readCount-1)/2]) + ", "+str(readSize[3*(readCount-1)/4])+", "+str(readSize[readCount-1]),		        
                        "{:.02f}".format(timeInterval[0]) + ", "+ "{0:.2f}".format(timeInterval[(len(timeInterval)-1)/4]) + ", " +"{0:.2f}".format(timeInterval[(len(timeInterval)-1)/2]) + ", "+"{0:.2f}".format(timeInterval[3*(len(timeInterval)-1)/4])+", "+"{0:.2f}".format(timeInterval[len(timeInterval)-1]),
		        str(numpy.percentile(overwritten_blk_count,0)) + ", "
		        + str(numpy.percentile(overwritten_blk_count,25)) + ", "
		        + str(numpy.percentile(overwritten_blk_count,50)) + ", "
		        + str(numpy.percentile(overwritten_blk_count,75)) + ", "
		        + str(numpy.percentile(overwritten_blk_count,99)))
  out.write(outstr)
