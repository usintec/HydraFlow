---
name: HydraCore Events Module 5
description: Design decisions behind the Events module (Event/EventDispatcher/EventQueue/EventBus/Subscription) for future consistency.
---

- Event identity uses `std::type_index(typeid(Derived))` via a CRTP base `EventType<Derived>` — simplest robust approach given RTTI is already used elsewhere in HydraCore (assert-based), avoids hand-rolled atomic type-id counters.
- `EventDispatcher` is synchronous only, keyed by `EventTypeId` -> sorted `Vector<EventListenerEntry>` (priority desc, insertion order as tiebreak via `std::stable_sort`). `Dispatch()` snapshots the listener vector under lock then invokes callbacks unlocked, so a listener can safely subscribe/unsubscribe (including itself) during dispatch without deadlock.
- `EventQueue` is a thread-safe FIFO (`std::deque<UniquePtr<Event>>`). `ProcessUpTo`/`ProcessAll` only drain what was pending *at call start* — events enqueued by a listener mid-dispatch are deferred to the next call, bounding per-frame work.
- `EventBus` is an owned instance (not a static/global facade like MemoryManager) — combines one `EventDispatcher` + one `EventQueue`; intended to live on `EngineContext`, with `ProcessQueue()` called once per frame from the app update loop.
- `Subscription` is the RAII unsubscribe handle (move-only, `Reset()`/`Release()`), consistent pattern for any future "handle that undoes registration on destruction" needs.
- Event opts into "stop propagation" per-instance via `SetStopPropagationWhenHandled(true)`; default is off so all listeners still observe an event even if one marks it handled.
