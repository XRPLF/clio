/*Script provided by Nathan Nichols*/

const xrpl = require('xrpl')

const api = new xrpl.Client('ws://s2-clio.ripple.com:51233')

async function doWork()
{
    await api.connect()
    console.log("CONNECTED")

    const request = {
    command: "account_tx",
    account: "r9m6MwViR4GnUNqoGXGa8eroBrZ9FAPHFS",
    binary: false,
    forward: false,
    ledger_index_max: 61437000,
    ledger_index_min: 61400000,
    limit: 50
}

let marker;
do
{
    if (marker)
    request.marker = marker

    const response = await api.request(request);

    console.log(response.result.transactions.length)

    marker = response.result.marker;
    console.log(marker)
} while (marker)

console.log("REQUESTING");
await api.disconnect()
}

doWork() 