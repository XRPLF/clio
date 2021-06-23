This folder contains all of the RPC handlers.

Generally, handlers parse the request and call the appropriate method of `BackendInterface`
to read from the database.

Certain RPCs, such as `fee` and `submit`, are just forwarded to `rippled`, and the response
is propagated back to the client.

If the database returns a timeout, an error is returned to the client.
This automatically happens, and is caught at a higher level, outside of the handler.
