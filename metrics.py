#!/usr/bin/python3
import argparse

from datetime import datetime

def getTime(line):
    bracketOpen = line.find("[")
    bracketClose = line.find("]")
    timestampSub = line[bracketOpen+1:bracketClose]
    timestamp = datetime.strptime(timestampSub, '%Y-%m-%d %H:%M:%S.%f')
    return timestamp.timestamp()


def parseLogs(filename, interval, minTxnCount = 0):

    with open(filename) as f:

        totalTime = 0
        totalTxns = 0
        totalObjs = 0
        totalLoadTime = 0


        start = 0
        end = 0
        totalLedgers = 0

        intervalTime = 0
        intervalTxns = 0
        intervalObjs = 0
        intervalLoadTime = 0

        intervalStart = 0
        intervalEnd = 0
        intervalLedgers = 0

        for line in f:
            if "Load phase" in line:
                sequenceIdx = line.find("Sequence : ")
                hashIdx =  line.find(" Hash :")
                sequence = line[sequenceIdx + len("Sequence : "):hashIdx]
                txnCountSubstr = "txn count = "
                objCountSubstr = ". object count = "
                loadTimeSubstr = ". load time = "
                txnsSubstr = ". load txns per second = "
                objsSubstr = ". load objs per second = "
                txnCountIdx = line.find(txnCountSubstr)
                objCountIdx = line.find(objCountSubstr)
                loadTimeIdx = line.find(loadTimeSubstr)
                txnsIdx = line.find(txnsSubstr)
                objsIdx = line.find(objsSubstr)
                txnCount = line[txnCountIdx + len(txnCountSubstr):objCountIdx]
                objCount = line[objCountIdx + len(objCountSubstr):loadTimeIdx]
                loadTime = line[loadTimeIdx + len(loadTimeSubstr):txnsIdx]
                txnsPerSecond = line[txnsIdx + len(txnsSubstr):objsIdx]
                objsPerSecond = line[objsIdx + len(objsSubstr):-1]
                if int(txnCount) >= minTxnCount:
                    totalTime += float(loadTime);
                    totalTxns += float(txnCount)
                    totalObjs += float(objCount)
                    intervalTime += float(loadTime)
                    intervalTxns += float(txnCount)
                    intervalObjs += float(objCount)

                totalLoadTime += float(loadTime)
                intervalLoadTime += float(loadTime)
                

                if start == 0:
                    start = getTime(line)


                prevEnd = end
                end = getTime(line)
                if end - prevEnd > 3 and prevEnd != 0:
                    print("Caught up!")

                if intervalStart == 0:
                    intervalStart = getTime(line)

                intervalEnd = getTime(line)

                totalLedgers+=1
                intervalLedgers+=1
                ledgersPerSecond = 0
                if end != start:
                    ledgersPerSecond = float(totalLedgers) / float((end - start))
                intervalLedgersPerSecond = 0
                if intervalEnd != intervalStart:
                    intervalLedgersPerSecond = float(intervalLedgers) / float((intervalEnd - intervalStart))



                if int(sequence) % interval == 0:

                    print("Sequence = " + sequence + " : [time, txCount, objCount, txPerSec, objsPerSec]")
                    print(loadTime + " : " 
                        + txnCount + " : " 
                        + objCount + " : " 
                        + txnsPerSecond + " : " 
                        + objsPerSecond)
                    print("Interval Aggregate ( " + str(interval) + " ) [ledgers, txns, objects, elapsedTime, ledgersPerSec, avgLoadTime, txPerSec, objsPerSec]: ")
                    print(str(intervalLedgers) + " : " 
                            + str(intervalTxns) + " : "
                        + str(intervalObjs) + " : "
                        + str(intervalEnd - intervalStart) + " : " 
                        + str(intervalLedgersPerSecond) + " : " 
                        + str(intervalLoadTime/intervalLedgers) + " : " 
                        + str(intervalTxns/intervalTime) + " : " 
                        + str(intervalObjs/intervalTime))
                    print("Total Aggregate: [ledgers, txns, objects, elapsedTime, ledgersPerSec, avgLoadTime, txPerSec, objsPerSec]")
                    print(str(totalLedgers) + " : " 
                            str(totalTxns) + " : "
                            + str(totalObjs) + " : "
                        + str(end-start) + " : " 
                        + str(ledgersPerSecond) + " : " 
                        + str(totalLoadTime/totalLedgers) + " : " 
                        + str(totalTxns/totalTime) + " : " 
                        + str(totalObjs/totalTime))
                    if int(sequence) % interval == 0:
                        intervalTime = 0
                        intervalTxns = 0
                        intervalObjs = 0
                        intervalStart = 0
                        intervalEnd = 0
                        intervalLedgers = 0
                        intervalLoadTime = 0


    

parser = argparse.ArgumentParser(description='parses logs')
parser.add_argument("--filename")
parser.add_argument("--interval",default=100000)
parser.add_argument("--minTxnCount",default=0)

args = parser.parse_args()

def run(args):
    parseLogs(args.filename, int(args.interval), int(args.minTxnCount))

run(args)
