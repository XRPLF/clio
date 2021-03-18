#!/usr/bin/python3
import argparse

def parseLogs(filename):

    with open(filename) as f:

        totalTime = 0
        totalTxns = 0
        totalObjs = 0
        
        milTime = 0
        milTxns = 0
        milObjs = 0

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
                totalTime += float(loadTime);
                totalTxns += float(txnCount)
                totalObjs += float(objCount)
                milTime += float(loadTime)
                milTxns += float(txnCount)
                milObjs += float(objCount)
                if int(sequence) % 1000000 == 0:
                    print("This million: ")
                    print(str(milTxns/milTime) + " : " + str(milObjs/milTime))
                    milTime = 0
                    milTxns = 0
                    milObjs - 0

                print("Sequence = " + sequence + " : [time, txCount, objCount, txPerSec, objsPerSec]")
                print(loadTime + " : " + txnCount + " : " + objCount + " : " + txnsPerSecond + " : " + objsPerSecond)
                print("Aggregate: [txPerSec, objsPerSec]")
                print(str(totalTxns/totalTime) + " : " + str(totalObjs/totalTime))


        print("Last million: [txPerSec, objPerSec]")
        print(str(milTxns/milTime) + " : " + str(milObjs/milTime))
        print("Totals : [txnPerSec, objPerSec]")
        print(str(totalTxns/totalTime) + " : " + str(totalObjs/totalTime))
    

parser = argparse.ArgumentParser(description='parses logs')
parser.add_argument("--filename")

args = parser.parse_args()

def run(args):
    parseLogs(args.filename)

run(args)
