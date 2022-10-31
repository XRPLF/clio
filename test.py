#!/usr/bin/python3

from ast import parse
import websockets
import asyncio
import json
import io
import os
import subprocess
import argparse
import time
import threading

def checkAccountInfo(aldous, p2p):
    return isSubset(aldous["object"], p2p["result"]["account_data"])

def isSubset(sub, sup):
    for x in sub:
        if x == "deserialization_time_microsecond":
            continue
        if not x in sup:
            return False
        elif not sub[x] == sup[x]:
            return False
    return True


async def call(ip,port,msg):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
                await ws.send(msg)
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)


def compareAccountInfo(aldous, p2p):
    p2p = p2p["result"]["account_data"]
    aldous = aldous["object"]
    if isSubset(p2p,aldous):
        print("Responses match!!")
    else:
        print("Response mismatch")
        print(aldous)
        print(p2p)
def compareTx(aldous, p2p):
    p2p = p2p["result"]
    if aldous["transaction"] != p2p["tx"]:
        print("transaction mismatch")
        print(aldous["transaction"])
        print(p2p["tx"])
        return False
    if aldous["metadata"] != p2p["meta"] and not isinstance(p2p["meta"],dict):
        print("metadata mismatch")
        print("aldous : " + aldous["metadata"])
        print("p2p : " + str(p2p["meta"]))
        return False
    if aldous["ledger_sequence"] != p2p["ledger_index"]:
        print("ledger sequence mismatch")
        print(aldous["ledger_sequence"])
        print(p2p["ledger_index"])
    print("responses match!!")
    return True
    
def compareAccountTx(aldous, p2p):
    print(p2p)
    if "result" in p2p:
        p2p = p2p["result"]
    maxLedger = getMinAndMax(aldous)[1]
    minLedger = getMinAndMax(p2p)[0]
    p2pTxns = []
    p2pMetas = []
    p2pLedgerSequences = []
    for x in p2p["transactions"]:
        p2pTxns.append(x["tx_blob"])
        p2pMetas.append(x["meta"])
        p2pLedgerSequences.append(x["ledger_index"])
    aldousTxns = []
    aldousMetas = []
    aldousLedgerSequences = []
    for x in aldous["transactions"]:
        aldousTxns.append(x["transaction"])
        aldousMetas.append(x["metadata"])
        aldousLedgerSequences.append(x["ledger_sequence"])

    p2pTxns.sort()
    p2pMetas.sort()
    p2pLedgerSequences.sort()
    aldousTxns.sort()
    aldousMetas.sort()
    aldousLedgerSequences.sort()
    if p2pTxns == aldousTxns and p2pMetas == aldousMetas and p2pLedgerSequences == aldousLedgerSequences:
        print("Responses match!!!")
        print(len(aldousTxns))
        print(len(p2pTxns))
    else:
        print("Mismatch responses")
        print(len(aldousTxns))
        print(len(aldous["transactions"]))
        print(len(p2pTxns))
        print(len(p2p["transactions"]))
        print(maxLedger)

def getAccounts(filename):
    accounts = []
    with open(filename) as f:
        for line in f:
            if line[0] == "{":
                jv = json.loads(line)
                accounts.append(jv["Account"])
            if len(line) == 35:
                accounts.append(line[0:34])
            if len(line) == 44:
                accounts.append(line[3:43])
            if len(line) == 65:
                accounts.append(line[0:64])
            if len(line) == 41 or len(line) == 40:
                accounts.append(line[0:40])
            elif len(line) == 43:
                accounts.append(line[2:42])
    return accounts
def getAccountsAndCursors(filename):
    accounts = []
    cursors = []
    with open(filename) as f:
        for line in f:
            if len(line) == 0:
                continue
            space = line.find(" ")
            cursor = line[space+1:len(line)-1]
            if cursor == "None":
                cursors.append(None)
            else:
                cursors.append(json.loads(cursor))
            accounts.append(line[0:space])

    return (accounts,cursors)
def getBooks(filename):
    books = []
    with open(filename) as f:
        for line in f:
            if len(line) == 68:
                books.append(line[3:67])
    return books
def compareLedgerData(aldous, p2p):
    aldous[0].sort()
    aldous[1].sort()
    p2p[0].sort()
    p2p[1].sort()
    if aldous[0] != p2p[0]:
        print("Keys mismatch :(")
        print(len(aldous[0]))
        print(len(p2p[0]))
        return False
    if aldous[1] != p2p[1]:
        print("Objects mismatch :(")
        print(len(aldous[1]))
        print(len(p2p[1]))
        return False
    print("Responses match!!!!")



    



async def account_infos(ip, port, accounts, numCalls):

    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            print(len(accounts))
            for x in range(0,numCalls):
                account = accounts[random.randrange(0,len(accounts))]
                start = datetime.datetime.now().timestamp()
                await ws.send(json.dumps({"command":"account_info","account":account,"binary":True}))
                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                if (end - start) > 0.1:
                    print("request took more than 100ms")

    except websockets.exceptions.connectionclosederror as e:
        print(e)


async def account_info(ip, port, account, ledger, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    print(binary)
    try:
        async with websockets.connect(address) as ws:
            if ledger is None:
                await ws.send(json.dumps({"command":"account_info","account":account, "binary":bool(binary)}))
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
            else:
                await ws.send(json.dumps({"command":"account_info","account":account, "ledger_index":int(ledger), "binary":bool(binary)}))
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
                return res
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

def getMinAndMax(res):
    minSeq = None
    maxSeq = None
    for x in res["transactions"]:
        seq = None
        if "ledger_sequence" in x:
            seq = int(x["ledger_sequence"])
        else:
            seq = int(x["ledger_index"])
        if minSeq is None or seq < minSeq:
            minSeq = seq
        if maxSeq is None or seq > maxSeq:
            maxSeq = seq
    return (minSeq,maxSeq)
    

async def account_tx(ip, port, account, binary, forward=False, minLedger=None, maxLedger=None):

    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            if minLedger is None or maxLedger is None:
                await ws.send(json.dumps({"command":"account_tx","account":account, "binary":bool(binary),"forward":bool(forward),"limit":200}))
            else:
                await ws.send(json.dumps({"command":"account_tx","account":account, "binary":bool(binary),"forward":bool(forward),"limit":200,"ledger_index_min":minLedger, "ledger_index_max":maxLedger}))

            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
            return res
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def account_txs_full(ip, port, accounts, cursors, numCalls, limit):

    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            print(len(accounts))
            cursor = None
            account = None
            time = 0.0
            for x in range(0,numCalls):

                idx = random.randrange(0,len(accounts))
                account = accounts[idx]
                cursor = cursors[idx]
                start = datetime.datetime.now().timestamp()
                if cursor is None:
                    await ws.send(json.dumps({"command":"account_tx","account":account,"binary":True,"limit":limit}))
                else:
                    marker = {}
                    marker["ledger"] = cursor["ledger_sequence"]
                    marker["seq"] = cursor["transaction_index"]
                    await ws.send(json.dumps({"command":"account_tx","account":account,"cursor":cursor,"marker":marker,"binary":True,"limit":limit,"forward":False}))

                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                print(end-start)
                time += (end - start)
                txns = []
                if "result" in res:
                    txns = res["result"]["transactions"]
                else:
                    txns = res["transactions"]
                print(len(txns))
                print(account + " " + json.dumps(cursor))
                if (end - start) > 0.1:
                    print("request took more than 100ms")
            print("Latency = " + str(time / numCalls))

    except websockets.exceptions.connectionclosederror as e:
        print(e)
async def account_txs(ip, port, accounts, numCalls):

    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            print(len(accounts))
            cursor = None
            account = None
            for x in range(0,numCalls):

                if cursor is None:
                    account = accounts[random.randrange(0,len(accounts))]
                    start = datetime.datetime.now().timestamp()
                    await ws.send(json.dumps({"command":"account_tx","account":account,"binary":True,"limit":200}))
                else:
                    await ws.send(json.dumps({"command":"account_tx","account":account,"cursor":cursor,"binary":True,"limit":200}))


                res = json.loads(await ws.recv())
                if "cursor" in res:
                    if cursor:
                        print(account + " " + json.dumps(cursor))
                    else:
                        print(account + " " + "None")
                    #cursor = res["cursor"]
                elif cursor:
                    print(account + " " + json.dumps(cursor))
                    cursor = None

                    
                end = datetime.datetime.now().timestamp()
                if (end - start) > 0.1:
                    print("request took more than 100ms")

    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def account_tx_full(ip, port, account, binary,forward=False,minLedger=None, maxLedger=None):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        cursor = None
        marker = None
        req = {"command":"account_tx","account":account, "binary":bool(binary),"forward":bool(forward),"limit":200}
        results = {"transactions":[]}
        numCalls = 0
        async with websockets.connect(address) as ws:
            while True:
                numCalls = numCalls+1
                if not cursor is None:
                    req["cursor"] = cursor
                if not marker is None:
                    req["marker"] = marker
                if minLedger is not None and maxLedger is not None:
                    req["ledger_index_min"] = minLedger
                    req["ledger_index_max"] = maxLedger
                start = datetime.datetime.now().timestamp()
                print("sending")
                print(req)
                await ws.send(json.dumps(req))
                res = await ws.recv()
                
                end = datetime.datetime.now().timestamp()
                
                print(end - start)
                res = json.loads(res)
                #print(res)
                print(json.dumps(res,indent=4,sort_keys=True))
                if "result" in res:
                    print(len(res["result"]["transactions"]))
                else:
                    print(len(res["transactions"]))
                if "result" in res:
                    results["transactions"].extend(res["result"]["transactions"])
                else:
                    results["transactions"].extend(res["transactions"])
                if "cursor" in res:
                    cursor = {"ledger_sequence":res["cursor"]["ledger_sequence"],"transaction_index":res["cursor"]["transaction_index"]}
                    print(cursor)
                if "marker" in res:
                    marker = {"ledger":res["marker"]["ledger"],"seq":res["marker"]["seq"]}
                    print(marker)
                elif "result" in res and "marker" in res["result"]:
                    marker={"ledger":res["result"]["marker"]["ledger"],"seq":res["result"]["marker"]["seq"]}
                    print(marker)
                else:
                    print("no cursor or marker")
                    break    
            return results
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def tx(ip, port, tx_hash, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"tx","transaction":tx_hash,"binary":bool(binary)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
            return res
    except websockets.exceptions.connectionclosederror as e:
        print(e)
async def txs(ip, port, hashes, numCalls):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            for x in range(0,numCalls):
                h = hashes[random.randrange(0,len(hashes))]
                start = datetime.datetime.now().timestamp()
                await ws.send(json.dumps({"command":"tx","transaction":h,"binary":True}))
                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                if (end - start) > 0.1:
                    print("request took more than 100ms")
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_entry(ip, port, index, ledger, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger_entry","index":index,"binary":bool(binary),"ledger_index":int(ledger)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
            if "result" in res:
                res = res["result"]
            if "object" in res:
                return (index,res["object"])
            else:
                return (index,res["node_binary"])
    except websockets.exceptions.connectionclosederror as e:
        print(e)
async def ledger_entries(ip, port, ledger, keys, numCalls):
    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    try:
        async with websockets.connect(address) as ws:
            print(len(keys))
            for x in range(0,numCalls):
                index = keys[random.randrange(0,len(keys))]
                start = datetime.datetime.now().timestamp()
                await ws.send(json.dumps({"command":"ledger_entry","index":index,"binary":True,"ledger_index":int(ledger)}))
                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                if (end - start) > 0.1:
                    print("request took more than 100ms")

    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_entries(ip, port,ledger):
    address = 'ws://' + str(ip) + ':' + str(port)
    entries = await ledger_data(ip, port, ledger, 200, True)

    try:
        async with websockets.connect(address) as ws:
            objects = []
            for x,y in zip(entries[0],entries[1]):
                await ws.send(json.dumps({"command":"ledger_entry","index":x,"binary":True,"ledger_index":int(ledger)}))
                res = json.loads(await ws.recv())
                objects.append((x,res["object"]))
                if res["object"] != y:
                    print("data mismatch")
                    return None
            print("Data matches!")
            return objects

    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_data(ip, port, ledger, limit, binary, cursor):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            if limit is not None:
                await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":bool(binary),"limit":int(limit),"cursor":cursor,"marker":cursor}))
            else:
                await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":bool(binary),"cursor":cursor,"marker":cursor}))
            res = json.loads(await ws.recv())
            print(res)
            objects = []
            blobs = []
            keys = []
            if "result" in res:
                objects = res["result"]["state"]
            else:
                objects = res["objects"]
            if binary:
                for x in objects:
                    blobs.append(x["data"])
                    keys.append(x["index"])
                    if len(x["index"]) != 64:
                        print("bad key")
                return (keys,blobs)

    except websockets.exceptions.connectionclosederror as e:
        print(e)

def writeLedgerData(state,filename):
    print(len(state))

    with open(filename,'w') as f:
        for k,v in state.items():
            f.write(k)
            f.write(':')
            f.write(v)
            f.write('\n')


async def ledger_data_full(ip, port, ledger, binary, limit, typ=None, count=-1, marker = None):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        state = {}
        async with websockets.connect(address,max_size=1000000000) as ws:
            if int(limit) < 2048:
                limit = 2048
            while True:
                res = {}
                if marker is None:
                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":binary, "limit":int(limit),"out_of_order":True}))
                    res = json.loads(await ws.recv())
                    
                else:

                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"cursor":marker, "marker":marker,"binary":bool(binary), "limit":int(limit),"out_of_order":True}))
                    res = json.loads(await ws.recv())
                
                    
                if "error" in res:
                    print(res["error"])
                    continue

                objects = []
                if "result" in res:
                    objects = res["result"]["state"]
                else:
                    objects = res["objects"]
                for x in objects:
                    if binary:
                        if typ is None or x["data"][2:6] == typ:
                            #print(json.dumps(x))
                            state[x["index"]] = x["data"]
                    else:
                        if typ is None or x["LedgerEntryType"] == typ:
                            state[x["index"]] = x
                if count != -1 and len(state) > count:
                    print("stopping early")
                    print(len(state))
                    print("done")
                    return state
                if "cursor" in res:
                    marker = res["cursor"]
                    print(marker)
                elif "result" in res and "marker" in res["result"]:
                    marker = res["result"]["marker"]
                    print(marker)
                else:
                    print("done")
                    return state


    except websockets.exceptions.connectionclosederror as e:
        print(e)

def compare_offer(aldous, p2p):
    for k,v in aldous.items():
        if k == "deserialization_time_microseconds":
            continue
        if p2p[k] != v:
            print("mismatch at field")
            print(k)
            return False
    return True

def compare_book_offers(aldous, p2p):
    p2pOffers = {}
    for x in p2p:
        matched = False
        for y in aldous:
            if y["index"] == x["index"]:
                if not compare_offer(y,x):
                    print("Mismatched offer")
                    print(y)
                    print(x)
                    return False
                else:
                    matched = True
        if not matched:
            print("offer not found")
            print(x)
            return False
    print("offers match!")
    return True
                    
async def book_changes(ip, port, ledger):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({
                "command" : "book_changes",
                "ledger_index" : ledger
            }))
            res = json.loads(await ws.recv())
            print(json.dumps(res, indent=4, sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def book_offerses(ip, port, ledger, books, numCalls):
    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            print(len(books))
            for x in range(0,numCalls):
                book = books[random.randrange(0,len(books))]
                start = datetime.datetime.now().timestamp()
                await ws.send(json.dumps({"command":"book_offers","book":book,"binary":True}))
                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                print(book)
                print(len(res["offers"]))
                if (end - start) > 0.1:
                    print("request took more than 100ms")

    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def book_offers(ip, port, ledger, pay_currency, pay_issuer, get_currency, get_issuer, binary, limit):

    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        offers = []
        cursor = None
        async with websockets.connect(address) as ws:
            while True:
                taker_gets = json.loads("{\"currency\":\"" + get_currency+"\"}")
                if get_issuer is not None:
                    taker_gets["issuer"] = get_issuer
                taker_pays = json.loads("{\"currency\":\"" + pay_currency + "\"}")
                if pay_issuer is not None:
                    taker_pays["issuer"] = pay_issuer 
                req = {"command":"book_offers","ledger_index":int(ledger), "taker_pays":taker_pays, "taker_gets":taker_gets, "binary":bool(binary), "limit":int(limit)}
                if cursor is not None:
                    req["cursor"] = cursor
                await ws.send(json.dumps(req))
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
                if "result" in res:
                    res = res["result"]
                for x in res["offers"]:
                    offers.append(x)
                if "cursor" in res:
                    cursor = res["cursor"]
                    print(cursor)
                else:
                    print(len(offers))
                    return offers
                    
    except websockets.exceptions.connectionclosederror as e:
        print(e)

def compareLedger(aldous, p2p):
    p2p = p2p["result"]["ledger"]
    p2pHeader = p2p["ledger_data"]
    aldousHeader = aldous["header"]["blob"]
    if p2pHeader == aldousHeader:
        print("Headers match!!!")
    else:
        print("Header mismatch")
        print(aldousHeader)
        print(p2pHeader)
        return

    p2pTxns = []
    p2pMetas = []
    for x in p2p["transactions"]:
        p2pTxns.append(x["tx_blob"])
        p2pMetas.append(x["meta"])
    aldousTxns = []
    aldousMetas = []
    for x in aldous["transactions"]:
        aldousTxns.append(x["transaction"])
        aldousMetas.append(x["metadata"])



    p2pTxns.sort()
    p2pMetas.sort()
    aldousTxns.sort()
    aldousMetas.sort()
    if p2pTxns == aldousTxns and p2pMetas == aldousMetas:
        print("Responses match!!!")
    else:
        print("Mismatch responses")
        print(aldous)
        print(p2p)

def getHashesFromFile(filename):
    hashes = []
    with open(filename) as f:
        for line in f:
            if len(line) == 65:
                hashes.append(line[0:64])
    return hashes


def getHashes(res):
    if "result" in res:
        res = res["result"]["ledger"]

    hashes = []
    for x in res["ledger"]["transactions"]:
        print(x)
        if "hash" in x:
            hashes.append(x["hash"])
        elif "transaction" in x and "hash" in x["transaction"]:
            hashes.append(x["transaction"]["hash"])
        else:
            hashes.append(x)
    return hashes

import random
import datetime
import ssl
import pathlib
numCalls = 0
async def ledgers(ip, port, minLedger, maxLedger, transactions, expand, maxCalls):
    global numCalls
    address = 'ws://' + str(ip) + ':' + str(port)
    random.seed()
    ledger = 0
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    localhost_pem = pathlib.Path(__file__).with_name("cert.pem")
    ssl_context.load_verify_locations(localhost_pem)
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            global numCalls
            for i in range(0, maxCalls):
                

                ledger = random.randrange(minLedger,maxLedger)
                start = datetime.datetime.now().timestamp()
                await ws.send(json.dumps({"command":"ledger","ledger_index":int(ledger),"binary":True, "transactions":bool(transactions),"expand":bool(expand)}))
                res = json.loads(await ws.recv())
                end = datetime.datetime.now().timestamp()
                if (end - start) > 0.1:
                    print("request took more than 100ms : " + str(end - start))
                numCalls = numCalls + 1
                if "error" in res:
                    print(res["error"])
                else:
                    print(res["header"]["blob"])

    except websockets.exceptions.ConnectionClosedError as e:
        print(e)
        print(ledger)

async def getManyHashes(ip, port, minLedger,maxLedger):

    hashes = []
    for x in range(minLedger,maxLedger):
        res = await ledger(ip, port, x,True, True, False)
        hashes.extend(getHashes(res))
    print(len(hashes))
    return hashes
async def getManyHashes(ip, port, minLedger,maxLedger, numHashes):

    random.seed()
    hashes = []
    while len(hashes) < numHashes:

        lgr = random.randrange(minLedger,maxLedger)
        res = await ledger(ip, port, lgr,True, True, False)
        hashes.extend(getHashes(res))
    print(len(hashes))
    return hashes



async def ledger(ip, port, ledger, binary, transactions, expand):

    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address,max_size=1000000000) as ws:
            await ws.send(json.dumps({"command":"ledger","ledger_index":int(ledger),"binary":bool(binary), "transactions":bool(transactions),"expand":bool(expand)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
            return res

    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_range(ip, port):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger_range"}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
            if "error" in res:
                await ws.send(json.dumps({"command":"server_info"}))
                res = json.loads(await ws.recv())
                print(res)
                rng = res["result"]["info"]["complete_ledgers"]
                if rng == "empty":
                    return (0,0)
                idx = rng.find("-")
                return (int(rng[0:idx]),int(rng[idx+1:]))
                
            res = res["result"]
            return (res["ledger_index_min"],res["ledger_index_max"])
    except websockets.exceptions.connectionclosederror as e:
        print(e)
async def fee(ip, port):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"fee"}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def server_info(ip, port):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"server_info"}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_diff(ip, port, base, desired, includeBlobs):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger_diff","base_ledger":int(base),"desired_ledger":int(desired),"include_blobs":bool(includeBlobs)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)
    

async def perf(ip, port):
    res = await ledger_range(ip,port)
    time.sleep(10)
    res2 = await ledger_range(ip,port)
    lps = ((int(res2[1]) - int(res[1])) / 10.0)
    print(lps)


async def subscribe(ip, port):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"subscribe","streams":["ledger"]}))
            #await ws.send(json.dumps({"command":"subscribe","streams":["book_changes"]}))
            #await ws.send(json.dumps({"command":"subscribe","streams":["manifests"]}))
            while True:
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def verifySubscribe(ip,clioPort,ripdPort):
    clioAddress = 'ws://' + str(ip) + ':' + str(clioPort)
    ripdAddress = 'ws://' + str(ip) + ':' + str(ripdPort)
    ripdTxns = {}
    clioTxns = {}
    ripdBooks = {}
    clioBooks = {}
    try:
        async with websockets.connect(clioAddress) as ws1:
            async with websockets.connect(ripdAddress) as ws2:
                await ws1.send(json.dumps({"command":"server_info"}))
                res = json.loads(await ws1.recv())
                print(res)
                start = int(res["result"]["info"]["complete_ledgers"].split("-")[1])
                end = start + 2

                streams = ["ledger","transactions"]
                books = [{"both":True,"taker_pays":{"currency":"XRP"},"taker_gets":{"currency":"USD","issuer":"rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq"}},
                        {"taker": "r9cZA1mLK5R5Am25ArfXFmqgNwjZgnfk59", "taker_gets": {"currency": "XRP"},"taker_pays": {"currency": "USD","issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"}}]
                accounts = ["rDzTZxa7NwD9vmNf5dvTbW4FQDNSRsfPv6","rrpNnNLKrartuEqfJGpqyDwPj1AFPg9vn1"]
                await ws1.send(json.dumps({"command":"subscribe","streams":streams,"books":books,"accounts":accounts})),
                await ws2.send(json.dumps({"command":"subscribe","streams":streams,"books":books,"accounts":accounts}))

                res1 = json.loads(await ws1.recv())
                res2 = json.loads(await ws2.recv())
                print(json.dumps(res1,indent=4,sort_keys=True))
                print(json.dumps(res2,indent=4,sort_keys=True))
                res1 = res1["result"]
                res2 = res2["result"]
                assert("validated_ledgers" in res1 and "validated_ledgers" in res2)
                res1["validated_ledgers"] = ""
                res2["validated_ledgers"] = ""
                assert(res1 == res2)
                idx = 0
                def compareObjects(clio,ripd):
                    print("sorting")
                    clio.sort(key = lambda x : x["transaction"]["hash"])
                    ripd.sort(key = lambda x : x["transaction"]["hash"])
                    clioFiltered = []
                    ripdFiltered = []
                    for x in clio:
                        if x not in clioFiltered:
                            clioFiltered.append(x)
                    clio = clioFiltered
                    for x in ripd:
                        if x not in ripdFiltered:
                            ripdFiltered.append(x)
                    ripd = ripdFiltered

                    print("comparing")
                    if clio == ripd:
                        return True
                    else:
                        print("mismatch")
                        if len(clio) != len(ripd):
                            print("length mismatch!")
                            print(len(ripd))
                            print(len(clio))
                            for x in clio:
                                print(x["transaction"]["hash"])
                            for x in ripd:
                                print(x["transaction"]["hash"])
                            return False
                        for ripdElt,clioElt in zip(ripd,clio):
                            if clioElt != ripdElt:
                                print("mismatch at " + str(z))
                                if type(clioElt) is dict:
                                    for t in ripdElt:
                                        if t not in clioElt:
                                            print("missing from clio " + str(t))
                                            return False
                                        elif clioElt[t] != ripdElt[t]:
                                            print("mismatch at " + str(t))
                                            compareObjects(clioElt[t],ripdElt[t])
                                            return False
                                    for t in clioElt:
                                        if t not in clioElt:
                                            print("extra in clio : " + str(t))
                                elif type(clioElt) is list:
                                    if len(clioElt) != len(ripdElt):
                                        print("Mismatched list size")
                                        return False
                                    for x,y in zip(ripdElt,clioElt):
                                        if x != y:
                                            print("Mismatch in list")
                                            print(x)
                                            print(y)
                                            return False
                            return False

                while True:
                    res1 = json.loads(await ws1.recv())
                    if res1["type"] != "ledgerClosed":
                        continue
                    else:
                        break
                while True:
                    res2 = json.loads(await ws2.recv())
                    if res2["type"] != "ledgerClosed":
                        continue
                    else:
                        break

                async def getAllTxns(ws):
                    txns = []
                    while True:
                        res = json.loads(await ws.recv())
                        if res["type"] == "transaction":
                            txns.append(res)
                        elif res["type"] == "ledgerClosed":
                            print(json.dumps(res,indent=4,sort_keys=True))
                            return txns
                    return txns




                while True:
                    print("getting clio")
                    clioTxns = await getAllTxns(ws1)
                    print("getting ripd")
                    ripdTxns = await getAllTxns(ws2)
                    if not compareObjects(clioTxns,ripdTxns):
                        print("failed")
                        assert(False)
                    print("matched full ledger")
                    print("txn count = " + str(len(clioTxns)))
                

    except websockets.exceptions.connectionclosederror as e:
        print(e)




    

parser = argparse.ArgumentParser(description='test script for xrpl-reporting')
parser.add_argument('action', choices=["account_info", "tx", "txs","account_tx", "account_tx_full","ledger_data", "ledger_data_full", "book_offers","ledger","ledger_range","ledger_entry", "ledgers", "ledger_entries","account_txs","account_infos","account_txs_full","book_changes","book_offerses","ledger_diff","perf","fee","server_info", "gaps","subscribe","verify_subscribe","call"])

parser.add_argument('--ip', default='127.0.0.1')
parser.add_argument('--port', default='8080')
parser.add_argument('--marker')
parser.add_argument('--hash')
parser.add_argument('--account')
parser.add_argument('--ledger')
parser.add_argument('--limit', default='200')
parser.add_argument('--taker_pays_issuer',default='rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B')
parser.add_argument('--taker_pays_currency',default='USD')
parser.add_argument('--taker_gets_issuer')
parser.add_argument('--taker_gets_currency',default='XRP')
parser.add_argument('--p2pIp', default='127.0.0.1')
parser.add_argument('--p2pPort', default='6006')
parser.add_argument('--verify',default=False)
parser.add_argument('--binary',default=True)
parser.add_argument('--forward',default=False)
parser.add_argument('--expand',default=False)
parser.add_argument('--transactions',default=False)
parser.add_argument('--minLedger',default=-1)
parser.add_argument('--maxLedger',default=-1)
parser.add_argument('--filename',default=None)
parser.add_argument('--ledgerIndex', default=-1)
parser.add_argument('--index')
parser.add_argument('--numPages',default=3)
parser.add_argument('--base')
parser.add_argument('--desired')
parser.add_argument('--includeBlobs',default=False)
parser.add_argument('--type',default=None)
parser.add_argument('--cursor',default='0000000000000000000000000000000000000000000000000000000000000000')
parser.add_argument('--numCalls',default=10000)
parser.add_argument('--numRunners',default=1)
parser.add_argument('--count',default=-1)
parser.add_argument('--streams',default=None)
parser.add_argument('--accounts',default=None)
parser.add_argument('--request',default=None)




args = parser.parse_args()

def run(args):
    asyncio.set_event_loop(asyncio.new_event_loop())
    if args.action == "call":
        asyncio.get_event_loop().run_until_complete(
                call(args.ip,args.port,args.request))
        return
    elif args.action == "server_info":
        asyncio.get_event_loop().run_until_complete(server_info(args.ip, args.port))
        return

    rng =asyncio.get_event_loop().run_until_complete(ledger_range(args.ip, args.port))
    if args.ledger is None:
        args.ledger = rng[1]
    if args.maxLedger == -1:
        args.maxLedger = rng[1]
    if args.minLedger == -1:
        args.minLedger = rng[0]
    if args.action == "fee":
        asyncio.get_event_loop().run_until_complete(fee(args.ip, args.port))
    elif args.action == "perf":
        asyncio.get_event_loop().run_until_complete(
                perf(args.ip,args.port))
    elif args.action == "gaps":
        missing = []
        for x in range(rng[0],rng[1]):
            res = asyncio.get_event_loop().run_until_complete(
                    ledger(args.ip, args.port, x, True, False, False))
            if "error" in res:
                print("missing " + str(x))
                missing.append(x)
        print(missing)
    elif args.action == "subscribe":
        asyncio.get_event_loop().run_until_complete(subscribe(args.ip,args.port))
    elif args.action == "verify_subscribe":
        asyncio.get_event_loop().run_until_complete(verifySubscribe(args.ip,args.port,args.p2pPort))
    elif args.action == "account_info":
        res1 = asyncio.get_event_loop().run_until_complete(
                account_info(args.ip, args.port, args.account, args.ledger, args.binary))
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    account_info(args.p2pIp, args.p2pPort, args.account, args.ledger, args.binary))
            print(compareAccountInfo(res1,res2))
    elif args.action == "txs":
        #hashes = asyncio.get_event_loop().run_until_complete(getManyHashes(args.ip,args.port, int(args.minLedger),int(args.maxLedger), int(args.numCalls)))
        #for x in hashes:
        #    print(x)
        #return
        hashes = getHashesFromFile(args.filename)
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(txs(args.ip, args.port, hashes,int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
    elif args.action == "ledgers":
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(ledgers(args.ip, args.port, int(args.minLedger), int(args.maxLedger), args.transactions, args.expand, int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
        print("Latency = " + str((end - start) / int(args.numCalls)) + " seconds")
    elif args.action == "ledger_entries":
        keys = []
        ledger_index = 0
        with open(args.filename) as f:
            i = 0
            for line in f:
                if ledger_index == 0:
                    ledger_index = int(line)
                elif len(line) == 65:
                    keys.append(line[0:64])
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(ledger_entries(args.ip, args.port, ledger_index,keys, int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
    elif args.action == "account_txs":
        accounts = getAccounts(args.filename)
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(account_txs(args.ip, args.port,accounts, int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
    elif args.action == "account_txs_full":
        accounts,cursors = getAccountsAndCursors(args.filename)
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(account_txs_full(args.ip, args.port,accounts,cursors,int(args.numCalls), int(args.limit))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
        print("Latency = " + str((end - start) / int(args.numCalls)) + " seconds")
    elif args.action == "account_infos":
        accounts = getAccounts(args.filename)
        async def runner():

            tasks = []
            for x in range(0,int(args.numRunners)):
                tasks.append(asyncio.create_task(account_infos(args.ip, args.port,accounts, int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
   
    elif args.action == "book_changes":
        asyncio.get_event_loop().run_until_complete(book_changes(args.ip, args.port, int(args.ledger)))
            
    elif args.action == "book_offerses":
        books = getBooks(args.filename)
        async def runner():

            tasks = []
            for x in range(0, int(args.numRunners)):
                tasks.append(asyncio.create_task(book_offerses(args.ip, args.port, int(args.ledger), books, int(args.numCalls))))
            for t in tasks:
                await t

        start = datetime.datetime.now().timestamp()
        asyncio.run(runner())
        end = datetime.datetime.now().timestamp()
        num = int(args.numRunners) * int(args.numCalls)
        print("Completed " + str(num) + " in " + str(end - start) + " seconds. Throughput = " + str(num / (end - start)) + " calls per second")
    elif args.action == "ledger_entry":
        asyncio.get_event_loop().run_until_complete(
                ledger_entry(args.ip, args.port, args.index, args.ledger, args.binary))
    elif args.action == "ledger_entries":
        res = asyncio.get_event_loop().run_until_complete(
                ledger_entries(args.ip, args.port, args.ledger))
        if args.verify:
            objects = []
            for x in res:
                res2 = asyncio.get_event_loop().run_until_complete(
                        ledger_entry(args.p2pIp, args.p2pPort,x[0] , args.ledger, True))
                if res2[1] != x[1]:
                    print("mismatch!")
                    return
            print("Data matches!")
    elif args.action == "ledger_diff":
        asyncio.get_event_loop().run_until_complete(
                ledger_diff(args.ip, args.port, args.base, args.desired, args.includeBlobs))
    elif args.action == "tx":
        if args.verify:
            args.binary = True
        if args.hash is None:
            args.hash = getHashes(asyncio.get_event_loop().run_until_complete(ledger(args.ip,args.port,args.ledger,False,True,False)))[0]
        res = asyncio.get_event_loop().run_until_complete(
                tx(args.ip, args.port, args.hash, args.binary))
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    tx(args.p2pIp, args.p2pPort, args.hash, args.binary))
            print(compareTx(res,res2))
    elif args.action == "account_tx":
        if args.verify:
            args.binary=True
        if args.account is None:
            args.hash = getHashes(asyncio.get_event_loop().run_until_complete(ledger(args.ip,args.port,args.ledger,False,True,False)))[0]
        
            res = asyncio.get_event_loop().run_until_complete(tx(args.ip,args.port,args.hash,False))
            print(res)
            args.account = res["Account"]
        
        res = asyncio.get_event_loop().run_until_complete(
                account_tx(args.ip, args.port, args.account, args.binary, args.forward))
        rng = getMinAndMax(res)


        
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    account_tx(args.p2pIp, args.p2pPort, args.account, args.binary,rng[0],rng[1]))
            print(compareAccountTx(res,res2))
    elif args.action == "account_tx_full":
        if args.verify:
            args.binary=True
        if args.account is None:
            args.hash = getHashes(asyncio.get_event_loop().run_until_complete(ledger(args.ip,args.port,args.ledger,False,True,False)))[0]
        
            res = asyncio.get_event_loop().run_until_complete(tx(args.ip,args.port,args.hash,False))
            args.account = res["Account"]
        print("starting")
        res = asyncio.get_event_loop().run_until_complete(
                account_tx_full(args.ip, args.port, args.account, args.binary,args.forward,None,None))
        rng = getMinAndMax(res)
        print(len(res["transactions"]))
        print(args.account)
        txs = set()
        for x in res["transactions"]:
            txs.add((x["tx_blob"],x["ledger_index"]))
        print(len(txs))

        if args.verify:
            print("requesting p2p node")
            res2 = asyncio.get_event_loop().run_until_complete(
                    account_tx_full(args.p2pIp, args.p2pPort, args.account, args.binary,args.forward, rng[0],rng[1],int(args.numPages)))

            print(compareAccountTx(res,res2))
    elif args.action == "ledger_data":
        res = asyncio.get_event_loop().run_until_complete(
                ledger_data(args.ip, args.port, args.ledger, args.limit, args.binary, args.cursor))
        if args.verify:
            writeLedgerData(res,args.filename)
    elif args.action == "ledger_data_full":
        if args.verify:
            args.limit = 2048
            args.binary = True
            if args.filename is None:
                args.filename = str(args.port) + "." + str(args.ledger)

        res = asyncio.get_event_loop().run_until_complete(
                ledger_data_full(args.ip, args.port, args.ledger, bool(args.binary), args.limit,args.type, int(args.count), args.marker))
        print(len(res))
        if args.verify:
            writeLedgerData(res,args.filename)

    elif args.action == "ledger":
        
        if args.verify:
            args.binary = True
            args.transactions = True
            args.expand = True
        res = asyncio.get_event_loop().run_until_complete(
                ledger(args.ip, args.port, args.ledger, args.binary, args.transactions, args.expand))
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    ledger(args.p2pIp, args.p2pPort, args.ledger, args.binary, args.transactions, args.expand))
            print(compareLedger(res,res2))
            
    elif args.action == "ledger_range":
        asyncio.get_event_loop().run_until_complete(
                ledger_range(args.ip, args.port))
    elif args.action == "book_offers":
        if args.verify:
            args.binary=True
        res = asyncio.get_event_loop().run_until_complete(
                book_offers(args.ip, args.port, args.ledger, args.taker_pays_currency, args.taker_pays_issuer, args.taker_gets_currency, args.taker_gets_issuer, args.binary,args.limit))
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    book_offers(args.p2pIp, args.p2pPort, args.ledger, args.taker_pays_currency, args.taker_pays_issuer, args.taker_gets_currency, args.taker_gets_issuer, args.binary, args.limit))
            print(compare_book_offers(res,res2))
            
    else:
        print("incorrect arguments")

        


run(args)
