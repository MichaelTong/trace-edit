#!/bin/bash
# Author: Huaicheng <huaicheng@cs.uchicago.edu>

MYPATH=/home/huaicheng/bin
#trace=traces/5min/MSNFS-10-15
trace=traces/5min/TPCC-5min-modified.trace
#trace=traces/5min/DTRS-5min-0.5t.trace
#trace=traces/5min/DTRS-5min-2x.trace

echo "===>Enter Replay Mode: (0: default, 5: gct, 4: ebusy)"
read mode

echo "======Start: $(date)====="
$MYPATH/resetcnt
$MYPATH/getcnt
$MYPATH/changeReadPolicy $mode
echo ""

echo ""
printf "===>You are in Mode: "
dmesg | tail -n 1
echo ""
sleep 2

echo "===>Checking Raid Status.."
cat /proc/mdstat
echo ""
echo ""
sleep 2

echo "===>Making sure you're running $trace"
read
sudo ./replayer /dev/md0 $trace

echo ""
$MYPATH/getcnt | tee /tmp/kern.cnt.log
echo ""

echo "======End: $(date)====="
echo "Please enter Trace Type: (e.g. tpcc)"
read ttype
echo "Please enter Running Mode: (e.g. gct, def)"
read tmode
echo "Please enter Seq NO. of this run:"
read tnum
echo "Please enter comment of this run(warmup, cpu, etc.):"
read comment

echo $comment >> /tmp/kern.cnt.log
mkdir -p ~/share/ttrais/kernellogs/$ttype-$tmode-$tnum/
cp /tmp/kern.cnt.log ~/share/ttrais/kernellogs/$ttype-$tmode-$tnum/

filename=$ttype-$tmode-$tnum

cp replay_metrics.txt rst-mike/$filename.log
./filter_rd_log.sh rst-mike/$filename.log

echo "rst-mike/$filename-rd.log"

echo "rst-mike/$filename-rd.log" > /tmp/runname.tmp
