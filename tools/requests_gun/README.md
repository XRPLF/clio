# 🔫 Requests gun

Requests gun is a simple tool that allows you to send multiple requests to a server with specific rps (requests per second) rate.
This tool is useful for testing the server behaviour under a specific load.
It takes a file that contains json request per line and sends them to the server in a loop with the specified rps rate.

The tool checks http status code of each request and whether the response body contains field 'error' or not.
It prints statistics for each second while running.

Run `requests_gun --help` to see the available options.
