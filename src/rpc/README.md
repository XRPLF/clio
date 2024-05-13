# RPC subsystem

The RPC subsystem is where the common framework for handling incoming JSON requests is implemented.

## Components

See the [common](https://github.com/XRPLF/clio/blob/develop/src/rpc/common) subfolder.

- **AnyHandler**: The type-erased wrapper that allows for storing different handlers in one map/vector.
- **RpcSpec/FieldSpec**: The RPC specification classes, used to specify how incoming JSON is to be validated before it's parsed and passed on to individual handler implementations.
- **Validators/Modifiers**: A bunch of supported validators and modifiers that can be specified as requirements for each `FieldSpec` to make up the final `RpcSpec` of any given RPC handler.

## Implementing a handler

See [tests/unit/rpc](https://github.com/XRPLF/clio/tree/develop/tests/unit/rpc) for examples.

Handlers need to fulfil the requirements specified by the `SomeHandler` concept (see `rpc/common/Concepts.hpp`):

- Expose types:
  
  - `Input` - The POD struct which acts as input for the handler

  - `Output` - The POD struct which acts as output of a valid handler invocation

- Have a `spec(uint32_t)` member function returning a const reference to an `RpcSpec` describing the JSON input for the specified API version.

- Have a `process(Input)` member function that operates on `Input` POD and returns `HandlerReturnType<Output>`

- Implement `value_from` and `value_to` support using `tag_invoke` as per `boost::json` documentation for these functions.
