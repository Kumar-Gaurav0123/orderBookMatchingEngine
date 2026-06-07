# Order Book Matching Engine

A high-performance limit order book (LOB) matching engine written in C++20, designed with HFT-grade latency characteristics.

## Features

- **Price-time priority matching** — FIFO per price level, correct maker/taker semantics
- **Order types** — `LIMIT`, `MARKET` (with `IOC` / `FOK` as time-in-force policies)
- **O(1) order allocation** — placement-new into a pre-warmed `MemoryPool` slab; no heap fragmentation in the hot path
- **Cache-line aligned `Order` struct** — `alignas(64)`, hot fields first, prevents false sharing in the pool
- **Integer tick prices** — `uint64_t` throughout; no floating-point arithmetic in the matching path
- **Lock-free async mode** — MPSC (multi-producer, single-consumer) command queue; dedicated engine thread
- **Intrusive linked lists** — zero-allocation FIFO per price level using embedded `next`/`prev` pointers
- **Event callbacks** — `onTrade`, `onOrderAccepted`, `onOrderCancelled`, `onOrderModified`, `onOrderRejected`
- **Market depth API** — `bidDepth(n)` / `askDepth(n)` returning top-N levels
- **Latency statistics** — ring-buffer of last 1 024 order-to-book timings; `latencyPercentiles()` returns `{min, p50, p99, max}` in nanoseconds

## Architecture

```
orderBookMatchingEngine/
├── order/          # Order struct, OrderTypes enums, TimeStamp, OrderIdPool
├── mem/            # MemoryPool (slab), ArenaAllocator, IntrusiveNode
├── book/           # PriceLevel, OrderBook, MatchingEngine
├── Concurrency/    # AtomicRingBuffer (SPSC), SPSCQueue, MPSCQueue
├── feed/           # CSV Parser, FeedMessage, EngineCommand, Replay
├── utils/          # Logger, PerfTimer, FixedPoint, NonCopyable
└── main.cpp        # Demo + latency benchmark
```

### Key design decisions

| Concern | Decision | Rationale |
|---|---|---|
| Price representation | `uint64_t` integer ticks | Exact arithmetic; no rounding errors at price boundaries |
| Order allocation | `MemoryPool` slab + placement-new | O(1) alloc/dealloc, cache-friendly, zero fragmentation |
| Order struct layout | `alignas(64)`, hot fields first | Prevents false sharing; `price` and `remaining` in first cache line |
| Level data structure | Intrusive doubly-linked list | O(1) insert/remove/cancel with zero extra allocation |
| Book data structure | `std::map<uint64_t, PriceLevel>` | O(log P) add/cancel; O(1) best-bid/ask via `begin()` |
| Concurrency | MPSC lock-free queue | Multiple producer threads, single engine consumer; no mutex |
| FOK enforcement | Pre-check liquidity before touching book | Rejected FOK orders leave no book state side-effects |

## Build

Requires CMake ≥ 3.16 and a C++20 compiler (GCC ≥ 10, Clang ≥ 12, MSVC 19.28+).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/OrderBookMatchingEngine
```

For a debug build with AddressSanitizer + UBSan:

```bash
cmake -S . -B build-dbg -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dbg
./build-dbg/OrderBookMatchingEngine
```

## Demo output (excerpt)

```
=== DEMO 2: Limit buy @ 10100 (crosses ask, price-time priority) ===
  [TRADE]  maker=1  taker=4  price=10100  qty=30
  [TRADE]  maker=2  taker=4  price=10100  qty=5

=== DEMO 5: FOK buy @ 10100 qty=200 (not enough — rejected) ===
  [REJECT] id=9  reason=FOK not fully fillable
  (no trade expected)

  Latency (order-to-book) — min=180ns  p50=210ns  p99=950ns  max=1840ns
```

## Feed CSV format

```
NEW,  <SIDE>, <PRICE_TICKS>, <QTY>, <ORDER_TYPE>[, <TIF>]
CANCEL, <ORDER_ID>
MODIFY, <ORDER_ID>, <NEW_QTY>
```

Example:
```
NEW,BUY,10000,100,LIMIT,GTC
NEW,SELL,10000,50,LIMIT
CANCEL,1
MODIFY,2,25
```
