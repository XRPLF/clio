# Async framework

## Introduction

Clio uses threads intensively. Multiple parts of Clio were/are implemented by running a `std::thread` with some sort of loop inside. Every time this pattern is reimplemented in a slightly different way. State is managed using asynchronous queues, atomic flags, mutexes and other low level primitives.

On the other hand, Clio also uses `Boost.Asio` for more complex tasks such as networking, scheduling RPC handlers, and even interacting with the database is done via Asio’s coroutines.

There was a need for a simple yet powerful framework that will cover the following in a unified way:
- Exception/error handling and propagation
- Ability to return a value of any type as a result of a successful operation
- Cancellation (cooperative) of inflight operations
- Scheduled or delayed operations
- Type-erased wrappers to enable injection of executors of compatible but unknown type
- Mockability of the injectable execution context to enable unit testing

This framework attempts to cover all of the above.
It’s worth noting that this framework is merely a wrapper around `Boost.Asio` which is doing the real heavy lifting under the hood.

## High level

This section walks through each component of the async framework at a glance.

### Execution context

At the core of async framework are the execution contexts. Each execution context provides a way to execute blocks of code that can optionally return a value and/or an error.

There are multiple execution contexts to choose from, each with their own pros and cons.

#### CoroExecutionContext
This context wraps a thread pool and executes blocks of code by means of `boost::asio::spawn` which spawns coroutines.

Deep inside the framework it hides `boost::asio::yield_context` and automatically switches coroutine contexts everytime user’s code is checking `isStopRequested()` on the `StopToken` given to the user-provided lambda.

The benefit is that both timers and async operations can work concurrently on a `CoroExecutionContext` even if internally the thread pool only has 1 thread.

Users of this execution context should take care to split their work in reasonably sized batches to avoid incurring a performance penalty caused by switching coroutine contexts too often. However if the batches are too time consuming it may lead to slower cooperative cancellation.

#### PoolExecutionContext
This context wraps a thread pool but executes blocks of code without using coroutines.
Note: A downside of this execution context is that if there is only 1 thread in the thread pool, timers can not execute while the thread is busy executing user-provided code. It's up to the user of this execution context to decide how to deal with this and whether it's important for their use case.

#### SyncExecutionContext
This is a fully synchronous execution context. It runs the scheduled operations right on the caller thread. By the time `execute([]{ … })` returns the Operation it’s guaranteed to be ready (i.e. value or error can be immediately queried with `.get()`).

In order to support scheduled operations and timeout-based cancellation, this context schedules all timers on the SystemExecutionContext instead.

#### SystemExecutionContext
This context of 1 thread is always readily available system-wide and can be used for
- fire and forget operations where it makes no sense to create an entirely new context for them
- as an external context for scheduling timers (used by SyncExecutionContext automatically)

### Strand
Any execution context provides a convenient `makeStrand` member function which will return a strand object for the execution context.
The strand can then be used with the same set of APIs that the execution context provides with the difference being that everything that is executed through a strand is guaranteed to be serially executed within the strand. This is a way to avoid the need for using a mutex or other explic synchronization mechanisms.

### Outcome
An outcome is like a `std::promise` to the operations that execute on the execution context.
The framework will hold onto the outcome object internally and the user of the framework will only receive an operation object that is like the `std::future` to the outcome.

The framework will set the final value or error through the outcome object so that the user can receive it on the operation side as a `std::expected`.

### Operation
There are several different operation types available. The one used will depend on the signature of the executable lambda passed by the user of this framework.

#### Stoppable and non-stoppable operations
Stoppable operations can be cooperatively stopped via a stop token that is passed to the user-provided function/lambda. A stoppable operation is returned to the user if they specify a stop token as the first argument of the function/lambda for execution.

Regular, non-stoppable operations, can not be stopped. A non-stoppable operation is returned to the user if they did not request a stop token as the first argument of the function/lambda for execution.

#### Scheduled operations
Scheduled operations are wrappers on top of Stoppable and regular Operations and provide the functionality of a timer that needs to run out before the given block of code will finally be executed on the Execution Context.
Scheduled operations can be aborted by calling 
- `cancel` - will only cancel the timer. If the timer already fired this will have no effect
- `requestStop` - will stop the operation if it's already running or as soon as the timer runs out
- `abort` - will call `cancel` immediatelly followed by `requestStop`

### Error handling
By default, exceptions that happen during the execution of user-provided code are caught and returned in the error channel of `std::expected` as an instance of the `ExecutionError` struct. The user can then extract the error message by calling `what()` or directly accessing the `message` member.

### Returned value
If the user-provided lambda returns anything but `void`, the type and value will propagate through the operation object and can be received by calling `get` which will block until a value or an error is available.

The `wait` member function can be used when the user just wants to wait for the value to become available but not necessarily getting at the value just yet.

### Type erasure
On top of the templated execution contexts, outcomes, operations, strands and stop tokens this framework provides the type-erased wrappers with (mostly) the same interface.

#### AnyExecutionContext
This provides the same interface as any other execution context in this framework.
Note: the original context is taken in by reference.

See examples of use below.

#### AnyOperation<T>
Wraps any type of operations including regular, stoppable and scheduled.

Since this wrapper does not know which operation type it's wrapping it only provides an `abort` member function that will call the correct underlying functions depending on the real type of the operation. If `abort` is called on a regular (non-stoppable and not scheduled) operation, the call will result in an assertion failure.

## Examples
This section provides some examples. For more examples take a look at `ExecutionContextBenchmarks`, `AsyncExecutionContextTests` and `AnyExecutionContextTests`.

### Regular operation
#### Awaiting and reading values
```cpp
auto res = ctx.execute([]() { return 42; });
EXPECT_EQ(res.get().value(), 42);

auto value = 0;
auto res = ctx.execute([&value]() { value = 42; });

res.wait();
ASSERT_EQ(value, 42);
```    

### Stoppable operation
#### Requesting stoppage
The stop token can be used via the `isStopRequested()` member function:
```cpp 
auto res = ctx.execute([](auto stopToken) {
    while (not stopToken.isStopRequested())
        ;

    return 42;
});

res.requestStop();
```
 
Alternatively, the stop token is implicity convertible to `bool` so you can also use it like so:
```cpp 
auto res = ctx.execute([](auto stopRequested) {
    while (not stopRequested)
        ;

    return 42;
});

res.requestStop();
```

#### Automatic stoppage on timeout
By adding an optional timeout as the last arg to `execute` you can have the framework automatically call `requestStop()`:
```cpp 
auto res = ctx.execute([](auto stopRequested) {
    while (not stopRequested)
        ;

    return 42;
}, 3s);

// Automatically calls requestStop after 3 seconds
```

### Scheduled operation
#### Cancelling an outstanding operation
```cpp
auto res = ctx.scheduleAfter(
    10ms, []([[maybe_unused]] auto stopRequested, auto cancelled) {
        if (cancelled)
            std::print("Cancelled");
    }
);

res.cancel(); // or .abort() 
```

#### Get value after stopping
```cpp 
auto res = ctx.scheduleAfter(1ms, [](auto stopRequested) {
    while (not stopRequested)
        ;

    return 42;
});

res.requestStop();
```

#### Handling an exception
```cpp
auto res =
    ctx.scheduleAfter(1s, []([[maybe_unused]] auto stopRequested, auto cancelled) {
        if (not cancelled)
            throw std::runtime_error("test");
        return 0;
    });

auto const err = res.get().error();
EXPECT_TRUE(err.message.ends_with("test"));
EXPECT_TRUE(std::string{err}.ends_with("test"));
```    

### Strand
The APIs are basically the same as with the parent `ExecutionContext`.

#### Computing a value on a strand
```cpp
auto strand = ctx.makeStrand();
auto res = strand.execute([] { return 42; });

EXPECT_EQ(res.get().value(), 42);
```

### Type erasure
#### Simple use
```cpp
auto ctx = CoroExecutionContext{4};
auto anyCtx = AnyExecutionContext{ctx};

auto op = anyCtx.execute([](auto stopToken) {
    while(not stopToken.isStopRequested())
        std::this_thread::sleep_for(1s);   
}, 3s);
```

#### Aborting the operation
Erased operations only expose the `abort` member function that can be used to both cancel an outstanding and/or stop a running operation.

```cpp
auto op = anyCtx.scheduleAfter(3s, [](auto stopToken, auto cancelled) {
    if (cancelled)
        return;
    
    while(not stopToken.isStopRequested())
        std::this_thread::sleep_for(1s);   
}, 3s);

std::this_thread::sleep_for(2s);   
op.abort(); // cancels the scheduled operation with 1s to spare
```
