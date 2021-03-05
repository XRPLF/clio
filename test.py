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



async def account_info(ip, port, account, ledger):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            if ledger is None:
                await ws.send(json.dumps({"command":"account_info","account":account}))
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
            else:
                await ws.send(json.dumps({"command":"account_info","account":account, "ledger_index":int(ledger)}))
                res = json.loads(await ws.recv())
                print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def account_tx(ip, port, account):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"account_tx","account":account}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def tx(ip, port, tx_hash):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"tx","transaction":tx_hash}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)


async def ledger_data(ip, port, ledger, limit):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"limit":int(limit),"binary":True}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger_data_full(ip, port, ledger):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        
        async with websockets.connect(address) as ws:
            marker = None
            while True:
                if marker is None:
                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"binary":True}))
                    res = json.loads(await ws.recv())
                    print(res)
                    
                else:
                    await ws.send(json.dumps({"command":"ledger_data","ledger_index":int(ledger),"marker":marker}))
                    res = json.loads(await ws.recv())
                    
                if "marker" in res:
                    marker = int(res["marker"])
                    print(marker)
                elif "result" in res and "marker" in res["result"]:
                    marker = res["result"]["marker"]
                    print(marker)
                else:
                    print("done")
                    break
    except websockets.exceptions.connectionclosederror as e:
        print(e)


async def book_offers(ip, port, ledger, pay_currency, pay_issuer, get_currency, get_issuer):

    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        
        async with websockets.connect(address) as ws:
            taker_gets = json.loads("{\"currency\":\"" + get_currency+"\"}")
            if get_issuer is not None:
                taker_gets["issuer"] = get_issuer
            taker_pays = json.loads("{\"currency\":\"" + pay_currency + "\"}")
            if pay_issuer is not None:
                taker_pays["issuer"] = pay_issuer 

            await ws.send(json.dumps({"command":"book_offers","ledger_index":int(ledger), "taker_pays":taker_pays, "taker_gets":taker_gets}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
                    
    except websockets.exceptions.connectionclosederror as e:
        print(e)

async def ledger(ip, port, ledger):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"ledger","ledger_index":int(ledger)}))
            res = json.loads(await ws.recv())
            print(json.dumps(res,indent=4,sort_keys=True))
    except websockets.exceptions.connectionclosederror as e:
        print(e)

parser = argparse.ArgumentParser(description='test script for xrpl-reporting')
parser.add_argument('action', choices=["account_info", "tx", "account_tx", "ledger_data", "ledger_data_full", "book_offers","ledger"])
parser.add_argument('--ip', default='127.0.0.1')
parser.add_argument('--port', default='8080')
parser.add_argument('--hash')
parser.add_argument('--account', default="rLC64xxNif3GiY9FQnbaM4kcE6VvDhwRod")
parser.add_argument('--ledger')
parser.add_argument('--limit', default='200')
parser.add_argument('--taker_pays_issuer')
parser.add_argument('--taker_pays_currency')
parser.add_argument('--taker_gets_issuer')
parser.add_argument('--taker_gets_currency')




args = parser.parse_args()

def run(args):
    asyncio.set_event_loop(asyncio.new_event_loop())
    if args.action == "account_info":
        asyncio.get_event_loop().run_until_complete(
                account_info(args.ip, args.port, args.account, args.ledger))
    elif args.action == "tx":
        asyncio.get_event_loop().run_until_complete(
                tx(args.ip, args.port, args.hash))
    elif args.action == "account_tx":
        asyncio.get_event_loop().run_until_complete(
                account_tx(args.ip, args.port, args.account))
    elif args.action == "ledger_data":
        asyncio.get_event_loop().run_until_complete(
                ledger_data(args.ip, args.port, args.ledger, args.limit))
    elif args.action == "ledger_data_full":
        asyncio.get_event_loop().run_until_complete(
                ledger_data_full(args.ip, args.port, args.ledger))
    elif args.action == "ledger":
        asyncio.get_event_loop().run_until_complete(
                ledger(args.ip, args.port, args.ledger))
    elif args.action == "book_offers":
        asyncio.get_event_loop().run_until_complete(
                book_offers(args.ip, args.port, args.ledger, args.taker_pays_currency, args.taker_pays_issuer, args.taker_gets_currency, args.taker_gets_issuer))
    else:
        print("incorrect arguments")

run(args)

