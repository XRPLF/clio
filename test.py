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
                print(res)
            else:
                await ws.send(json.dumps({"command":"account_info","account":account, "ledger_index":int(ledger)}))
                res = json.loads(await ws.recv())
                print(res)
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def account_tx(ip, port, account):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"account_tx","account":account}))
            res = json.loads(await ws.recv())
            print(res)
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)

async def tx(ip, port, tx_hash):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"tx","transaction":tx_hash}))
            res = json.loads(await ws.recv())
            print(res)
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)




parser = argparse.ArgumentParser(description='test script for xrpl-reporting')
parser.add_argument('action', choices=["account_info", "tx", "account_tx"])
parser.add_argument('--ip', default='127.0.0.1')
parser.add_argument('--port', default='8080')
parser.add_argument('--hash')
parser.add_argument('--account', default="rLC64xxNif3GiY9FQnbaM4kcE6VvDhwRod")
parser.add_argument('--ledger')



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
    else:
        print("incorrect arguments")

run(args)

