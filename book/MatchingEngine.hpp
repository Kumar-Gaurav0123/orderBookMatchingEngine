#pragma once

#include <functional>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <array>
#include <algorithm>
#include <numeric>
#include <cstdint>

#include "OrderBook.hpp"
#include "../order/OrderIdPool.hpp"
#include "../order/OrderTypes.hpp"
#include "../mem/MemoryPool.hpp"
#include "../Concurrency/MPSCQueue.hpp"
#include "../feed/EngineCommand.hpp"
#include "../utils/Logging.hpp"
#include "../utils/PerfTimer.hpp"

using obk::Side;
using obk::OrderType;
using obk::TimeInForce;
using obk::OrderIdPool;

// ------------------- Trade Event -------------------
struct TradeEvent {
    uint64_t makerOrderId;
    uint64_t takerOrderId;
    uint64_t price;     // integer ticks — consistent with Order::price
    uint64_t quantity;
};

// ------------------- Latency Statistics -------------------
// Tracks order-to-fill (submit → first trade event) latency in nanoseconds.
// Ring buffer of the last N samples; call snapshot() for percentile reports.
struct LatencyStats {
    static constexpr std::size_t kMaxSamples = 1024;

    std::array<int64_t, kMaxSamples> samples{};
    std::size_t count  = 0;
    std::size_t cursor = 0;   // write position (circular)

    void record(int64_t ns) noexcept {
        samples[cursor] = ns;
        cursor = (cursor + 1) % kMaxSamples;
        if (count < kMaxSamples) ++count;
    }

    // Returns {min, p50, p99, max} in nanoseconds. Call from the engine
    // thread (or after stopping) — not thread-safe.
    std::array<int64_t, 4> percentiles() const noexcept {
        if (count == 0) return {0, 0, 0, 0};
        std::array<int64_t, kMaxSamples> buf;
        std::copy_n(samples.data(), count, buf.data());
        std::sort(buf.data(), buf.data() + count);
        auto at = [&](double pct) -> int64_t {
            std::size_t idx = static_cast<std::size_t>(pct * (count - 1) / 100.0);
            return buf[idx];
        };
        return { buf[0], at(50), at(99), buf[count - 1] };
    }
};

// ------------------- Matching Engine -------------------
//
// Owns all Order objects. Order memory is managed by a MemoryPool slab,
// so allocation is O(1) with no heap fragmentation in the hot path.
//
// Two operating modes:
//   Sync  — call submitOrder/cancel/modify directly (single thread only).
//   Async — call start(), then postCommand() from any thread; a dedicated
//           engine thread drains the MPSC command queue.
class MatchingEngine {
public:
    using OrderId  = uint64_t;
    using Price    = uint64_t;   // integer ticks
    using Quantity = uint64_t;

    // Construct with pool capacity. poolCapacity is the maximum number of
    // concurrently live orders the engine can hold without heap fallback.
    explicit MatchingEngine(std::size_t poolCapacity = 64 * 1024);
    ~MatchingEngine();

    // Non-copyable, non-movable.
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    // ------------ Event Callbacks (optional hooks) -------------

    // Fired on every fill (maker/taker pair).
    std::function<void(const TradeEvent&)> onTrade;

    // Fired when a new resting limit order enters the book.
    std::function<void(const obk::Order&)> onOrderAccepted;

    // Fired when an order is removed from the engine (cancel or full fill).
    std::function<void(OrderId)> onOrderCancelled;

    // Fired after a quantity-only modify completes.
    std::function<void(const obk::Order&)> onOrderModified;

    // Fired when an order is rejected (e.g. FOK not fillable, bad qty).
    std::function<void(OrderId, const char* reason)> onOrderRejected;

    // ------------- Sync Public API ----------------

    // Submit a new order; returns assigned OrderId (0 = rejected).
    OrderId submitOrder(Side side, OrderType type, TimeInForce tif,
                        Price price, Quantity qty);

    bool cancel(OrderId id);
    bool modify(OrderId id, Quantity newQty);

    // ---------- Async API ----------
    void postCommand(const EngineCommand& cmd);
    void start();
    void stop();

    // ------------------- Queries -------------------
    Price bestBid() const noexcept { return book_.bestBid(); }
    Price bestAsk() const noexcept { return book_.bestAsk(); }
    bool  empty()   const noexcept { return book_.empty(); }

    std::vector<std::pair<Price, Quantity>> bidDepth(int n = 5) const {
        return book_.bidDepth(n);
    }
    std::vector<std::pair<Price, Quantity>> askDepth(int n = 5) const {
        return book_.askDepth(n);
    }

    // Returns order-to-fill latency percentiles {min, p50, p99, max} in ns.
    std::array<int64_t, 4> latencyPercentiles() const noexcept {
        return latency_.percentiles();
    }

private:
    OrderBook   book_;
    OrderIdPool idPool_;

    // All live orders (resting + awaiting fill). Engine owns them.
    std::unordered_map<OrderId, obk::Order*> liveOrders_;

    // O(1) per-order allocation from a pre-allocated slab.
    mem::MemoryPool orderPool_;

    // Concurrency layer (async mode)
    MPSCQueue<EngineCommand> commandQueue_;
    std::atomic<bool>        running_{false};
    std::thread              engineThread_;

    // Latency tracking
    LatencyStats latency_;

    // ---- Internals ----
    void engineLoop();
    void releaseOrder(OrderId id);
};
