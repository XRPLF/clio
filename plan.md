# Fallback Recovery Design

## Problem

Once a node enters fallback mode (`isWriterDecidingFallback = true`), it publishes
`DbRole::Fallback`. `WriterDecider::onNewState` returns early when it sees self as
Fallback, so there is no exit path. The cluster stays in fallback forever, even after
the root cause (e.g. a rolling upgrade with old nodes) is resolved.

## Solution Overview

Add a boolean `isInRecovery` field to `ClioNode` (alongside the existing `dbRole`).
When a node has been in fallback long enough, it enters **fallback recovery** mode
(`dbRole=Fallback, isInRecovery=true`). Once all non-ReadOnly nodes are in recovery
(or already in election mode), the cluster exits fallback and runs a normal election.

No new `DbRole` value is needed. The fallback write-race continues uninterrupted
throughout the recovery coordination, so there is no write availability gap.

## State Machine

```
NotWriter / Writer
    |
    | sees any Fallback node
    |
    v
Fallback, isInRecovery=false  <---- start 30-min recovery timer
    |                    \
    | timer fires         \ sees any (Fallback, isInRecovery=true) node [contagion]
    |                      \
    v                       v
Fallback, isInRecovery=true  ---- cancel recovery timer
    |
    | all visible non-ReadOnly nodes are in
    | (Fallback, isInRecovery=true) OR (NotWriter/Writer)
    v
NotWriter  -->  normal election
```

### Transition Rules in `WriterDecider::onNewState`

1. Self is **ReadOnly** → `giveUpWriting()`, return.

2. Self is **Fallback, isInRecovery=false** → return early (timer is ticking, fallback
   write-race handles writing). Exception: if any visible node is
   `(Fallback, isInRecovery=true)`, apply contagion immediately (go to rule 3 path).

3. Self is **Fallback, isInRecovery=true** → check exit condition:
   - If any non-ReadOnly node is `(Fallback, isInRecovery=false)` → return (wait).
   - Otherwise → call `exitFallback()`, fall through to election.

4. Self is **NotWriter / Writer** and cluster has Fallback nodes:
   - If any Fallback node has `isInRecovery=true` (and none has `isInRecovery=false`)
     → call `setWriterDecidingFallback()` + `enterFallbackRecovery()` (direct contagion,
     skip timer).
   - Otherwise → call `setWriterDecidingFallback()`, start recovery timer if not already
     running.

5. Self is **NotWriter / Writer** and no Fallback nodes → normal election (existing logic).

### Timer Lifecycle

- **Started**: when `setWriterDecidingFallback()` is called and the node is not already
  in recovery (rule 4, non-contagion path).
- **Cancelled**: when the node transitions to `(Fallback, isInRecovery=true)` via either
  timer fire or contagion.
- Duration: 30 minutes (configurable constant `kRECOVERY_DELAY`).
- The timer fires and calls `enterFallbackRecovery()` on a clone of `writerState_`.

## File Changes

### `src/etl/SystemState.hpp`

Add one new field:

```cpp
/** @brief Whether this node has committed to exiting fallback mode. */
std::atomic_bool isInFallbackRecovery{false};
```

### `src/etl/WriterState.hpp` / `WriterState.cpp`

Add three methods to `WriterStateInterface` and implement in `WriterState`:

```cpp
/** Returns true if this node is in fallback recovery mode. */
[[nodiscard]] virtual bool isInFallbackRecovery() const = 0;

/** Transitions from (Fallback, no recovery) to (Fallback, recovery). */
virtual void enterFallbackRecovery() = 0;

/** Clears both isWriterDecidingFallback and isInFallbackRecovery.
    Called when transitioning back to election mode. */
virtual void exitFallback() = 0;
```

`exitFallback()` sets both `systemState_->isWriterDecidingFallback = false` and
`systemState_->isInFallbackRecovery = false`.

### `src/cluster/ClioNode.hpp`

Add field to `ClioNode`:

```cpp
bool isInRecovery;  ///< Whether this node is in fallback recovery mode
```

### `src/cluster/ClioNode.cpp`

**`ClioNode::from()`** — populate the new field:

```cpp
.isInRecovery = writerState.isInFallbackRecovery()
```

Note: a node with `dbRole == ReadOnly` cannot be in recovery, so `isInRecovery` is
always `false` for ReadOnly nodes (the existing early-return for ReadOnly in `from()`
already sets `dbRole = ReadOnly`; just ensure `isInRecovery = false` there).

**`tag_invoke` (serialize)** — add field:

```cpp
{JsonFields::kIS_IN_RECOVERY, node.isInRecovery}
```

**`tag_invoke` (deserialize)** — add field with default `false` for backward
compatibility (old nodes never published this field):

```cpp
auto const isInRecovery = obj.contains(JsonFields::kIS_IN_RECOVERY)
    ? obj.at(JsonFields::kIS_IN_RECOVERY).as_bool()
    : false;
```

Add `static constexpr std::string_view kIS_IN_RECOVERY = "is_in_recovery"` to
`JsonFields`.

### `src/cluster/WriterDecider.hpp`

Add members:

```cpp
#include "util/Mutex.hpp"
#include <boost/asio/steady_timer.hpp>
#include <chrono>

// In class WriterDecider:
static constexpr std::chrono::minutes kRECOVERY_DELAY{30};

std::shared_ptr<util::Mutex<boost::asio::steady_timer>> recoveryTimer_;
```

The constructor gains `ctx` access to schedule the timer. `recoveryTimer_` is
initialized in the constructor body:

```cpp
recoveryTimer_ = std::make_shared<util::Mutex<boost::asio::steady_timer>>(ctx_);
```

### `src/cluster/WriterDecider.cpp`

Rewrite `onNewState` according to the state machine above. Key points:

- Check `selfData->isInRecovery` in addition to `selfData->dbRole` when branching.
- **Start timer**: lock `recoveryTimer_`, call `expires_after(kRECOVERY_DELAY)` and
  `async_wait`. Capture `recoveryTimer_` (shared_ptr) and a clone of `writerState_`
  in the callback. The callback checks if the timer was cancelled (`ec ==
  boost::asio::error::operation_aborted`) before calling `enterFallbackRecovery()`.
- **Cancel timer**: lock `recoveryTimer_` and call `cancel()`.
- Guard against starting the timer multiple times: only call `expires_after` if the
  node was previously in election mode (i.e. the transition `NotWriter→Fallback` is
  happening now, not a repeated `onNewState` while already in Fallback-no-recovery).
  A simple approach: check `writerState_->isFallback()` before calling
  `setWriterDecidingFallback()` to detect the first entry.

## Backward Compatibility

Old nodes (missing `is_in_recovery` field in JSON) deserialize with `isInRecovery =
false`, meaning they appear as `(Fallback, isInRecovery=false)` to new nodes. New nodes
will wait for old nodes to either upgrade or disappear (TTL expiry) before completing
recovery. This is the correct behavior: recovery only proceeds once the entire visible
cluster is capable of participating.

## Tests

- Unit tests for the three new `WriterState` methods.
- Unit tests for `ClioNode::from()` and JSON round-trip with `isInRecovery`.
- `WriterDecider` unit tests covering:
  - Timer starts on first Fallback entry, not on repeated `onNewState` calls.
  - Contagion: node in election mode directly enters recovery when all Fallback nodes
    are already in recovery.
  - Exit condition: all nodes in recovery → transition to NotWriter and election.
  - Transition does not complete while any non-ReadOnly node is in
    `(Fallback, isInRecovery=false)`.
  - Timer cancellation when contagion fires before timer expires.
