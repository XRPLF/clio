#!/usr/bin/python3

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


def compareAccountInfo(aldous, p2p):
    p2p = p2p["result"]["account_data"]
    aldous = aldous["object"]
    if isSubset(p2p,aldous):
        print("Responses match!!")
    else:
        print("Response mismatch")
        print(aldous)
        print(p2p)



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

async def account_tx(ip, port, account, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"account_tx","account":account, "binary":bool(binary)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def tx(ip, port, tx_hash, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"tx","transaction":tx_hash,"binary":bool(binary)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)


async def ledger_data(ip, port, ledger, limit, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":bool(binary)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_data_full(ip, port, ledger, binary):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        
        async with websockets.connect(address) as ws:
            marker = None
            while True:
                if marker is None:
                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":bool(binary)}))
                    res = json.loads(await ws.recv())
                    
                else:

                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"cursor":marker, "binary":bool(binary)}))
                    res = json.loads(await ws.recv())
                    
                if "cursor" in res:
                    marker = res["cursor"]
                    print(marker)
                elif "result" in res and "marker" in res["result"]:
                    marker = res["result"]["marker"]
                    print(marker)
                else:
                    print("done")
                    break
    except websockets.exceptions.connectionclosederror as e:
        print(e)


async def book_offers(ip, port, ledger, pay_currency, pay_issuer, get_currency, get_issuer, binary):

    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        
        async with websockets.connect(address) as ws:
            taker_gets = json.loads("{\"currency\":\"" + get_currency+"\"}")
            if get_issuer is not None:
                taker_gets["issuer"] = get_issuer
            taker_pays = json.loads("{\"currency\":\"" + pay_currency + "\"}")
            if pay_issuer is not None:
                taker_pays["issuer"] = pay_issuer 

            await ws.send(json.dumps({"command":"book_offers","ledger_index":int(ledger), "taker_pays":taker_pays, "taker_gets":taker_gets, "binary":bool(binary)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
                    
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


def getHashes(res):
    if "result" in res:
        res = res["result"]["ledger"]

    hashes = []
    for x in res["transactions"]:
        if "hash" in x:
            hashes.append(x["hash"])
        elif "transaction" in x and "hash" in x["transaction"]:
            hashes.append(x["transaction"]["hash"])
    return hashes


async def ledger(ip, port, ledger, binary, transactions, expand):

    address = 'ws://' + str(ip) + ':' + str(port)
    hashes = []
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger","ledger_index":int(ledger),"binary":bool(binary), "transactions":bool(transactions),"expand":bool(expand)}))
            res = json.loads(await ws.recv())
            print(res)
            return res

    except websockets.exceptions.connectionclosederror as e:
        print(e)
    return hashes
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
                return res["result"]["info"]["validated_ledger"]["seq"]
            return res["ledger_index_max"]
    except websockets.exceptions.connectionclosederror as e:
        print(e)

parser = argparse.ArgumentParser(description='test script for xrpl-reporting')
parser.add_argument('action', choices=["account_info", "tx", "account_tx", "ledger_data", "ledger_data_full", "book_offers","ledger","ledger_range"])
parser.add_argument('--ip', default='127.0.0.1')
parser.add_argument('--port', default='8080')
parser.add_argument('--hash')
parser.add_argument('--account', default="rw2ciyaNshpHe7bCHo4bRWq6pqqynnWKQg")
parser.add_argument('--ledger')
parser.add_argument('--limit', default='200')
parser.add_argument('--taker_pays_issuer',default='rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B')
parser.add_argument('--taker_pays_currency',default='USD')
parser.add_argument('--taker_gets_issuer')
parser.add_argument('--taker_gets_currency',default='XRP')
parser.add_argument('--p2pIp', default='127.0.0.1')
parser.add_argument('--p2pPort', default='6005')
parser.add_argument('--verify',default=False)
parser.add_argument('--binary',default=False)
parser.add_argument('--expand',default=False)
parser.add_argument('--transactions',default=False)




args = parser.parse_args()

def run(args):
    asyncio.set_event_loop(asyncio.new_event_loop())
    if(args.ledger is None):
        args.ledger = asyncio.get_event_loop().run_until_complete(ledger_range(args.ip, args.port));
    if args.action == "account_info":
        res1 = asyncio.get_event_loop().run_until_complete(
                account_info(args.ip, args.port, args.account, args.ledger, args.binary))
        if args.verify:
            res2 = asyncio.get_event_loop().run_until_complete(
                    account_info(args.p2pIp, args.p2pPort, args.account, args.ledger, args.binary))
            print(compareAccountInfo(res1,res2))
    elif args.action == "tx":
        if args.hash is None:
            args.hash = getHashes(asyncio.get_event_loop().run_until_complete(ledger(args.ip,args.port,args.ledger,False,True,False)))[0]
        asyncio.get_event_loop().run_until_complete(
                tx(args.ip, args.port, args.hash, args.binary))
    elif args.action == "account_tx":
        asyncio.get_event_loop().run_until_complete(
                account_tx(args.ip, args.port, args.account, args.binary))
    elif args.action == "ledger_data":
        asyncio.get_event_loop().run_until_complete(
                ledger_data(args.ip, args.port, args.ledger, args.limit, args.binary))
    elif args.action == "ledger_data_full":
        asyncio.get_event_loop().run_until_complete(
                ledger_data_full(args.ip, args.port, args.ledger, args.binary))
    elif args.action == "ledger":
        
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
        asyncio.get_event_loop().run_until_complete(
                book_offers(args.ip, args.port, args.ledger, args.taker_pays_currency, args.taker_pays_issuer, args.taker_gets_currency, args.taker_gets_issuer, args.binary))
    else:
        print("incorrect arguments")

        


run(args)
