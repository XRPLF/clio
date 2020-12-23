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



async def account_info(ip, port):
    address = 'ws://' + str(ip) + ':' + str(port)
    try:
        async with websockets.connect(address) as ws:
            await ws.send(json.dumps({"command":"account_info","ledger_index":60392449,"account":"rLC64xxNif3GiY9FQnbaM4kcE6VvDhwRod"}))
            res = json.loads(await ws.recv())
            print(res)
    except websockets.exceptions.ConnectionClosedError as e:
        print(e)




parser = argparse.ArgumentParser(description='test script for xrpl-reporting')
parser.add_argument('action', choices=["account_info"])
parser.add_argument('--ip', default='127.0.0.1')
parser.add_argument('--port', default='8080')



args = parser.parse_args()

def run(args):
    asyncio.set_event_loop(asyncio.new_event_loop())
    if args.action == "account_info":
        asyncio.get_event_loop().run_until_complete(
                account_info(args.ip, args.port))
    else:
        print("incorrect arguments")

run(args)

