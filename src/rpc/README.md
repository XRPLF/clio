# Clio RPC subsystem
## Background
The RPC subsystem is where the common framework for handling incoming JSON requests is implemented.
Currently the NextGen RPC framework is a work in progress and the handlers are not yet implemented using the new common framework classes.

## Integration plan
- Implement base framework - **done**
- Migrate handlers one by one, making them injectable, adding unit-tests - **in progress**
- Integrate all new handlers into clio in one go
- Cover the rest with unit-tests
- Release first time with new subsystem active

## Components
See `common` subfolder.

- **AnyHandler**: The type-erased wrapper that allows for storing different handlers in one map/vector.
- **RpcSpec/FieldSpec**: The RPC specification classes, used to specify how incoming JSON is to be validated before it's parsed and passed on to individual handler implementations.
- **Validators**: A bunch of supported validators that can be specified as requirements for each **`FieldSpec`** to make up the final **`RpcSpec`** of any given RPC handler.

## Implementing a (NextGen) handler
See `unittests/rpc` for exmaples.

Handlers need to fullfil the requirements specified by the **`Handler`** concept (see `rpc/common/Concepts.h`):
- Expose types: 
    - `Input` - The POD struct which acts as input for the handler
	- `Output` - The POD struct which acts as output of a valid handler invocation
- Have a `spec()` member function returning a const reference to an **`RpcSpec`** describing the JSON input.
- Have a `process(Input)` member function that operates on `Input` POD and returns `HandlerReturnType<Output>`
- Implement `value_from` and `value_to` support using `tag_invoke` as per `boost::json` documentation for these functions.
