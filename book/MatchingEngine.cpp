#include "MatchingEngine.hpp"
#include <new>        // placement new
#include <iostream>
#ifdef _MSC_VER
#  include <intrin.h>    // __nop / _mm_pause on MSVC
#else
#  include <immintrin.h> // _mm_pause on GCC/Clang
#endif

using obk::Side;
using obk::OrderType;
using obk::TimeInForce;
using obk::Order;

// ============================================================
//  CONSTRUCTOR / DESTRUCTOR
// ============================================================

MatchingEngine::MatchingEngine(std::size_t poolCapacity)
    : idPool_(poolCapacity)
    , orderPool_(sizeof(Order), poolCapacity)
{
    // Wire the OrderBook's trade callback into our own onTrade handler and
    // latency recorder. We capture `this` by pointer — safe because the book
    // never outlives the engine.
    book_.onTrade = [this](uint64_t makerId, uint64_t takerId,
                           uint64_t price, uint64_t qty)
    {
        if (onTrade) {
            TradeEvent evt{ makerId, takerId, price, qty };
            onTrade(evt);
        }
    };

    // When a resting order is fully consumed inside the book's matching loop
    // it is erased from orderIndex_, but the engine still holds the pointer
    // in liveOrders_. This callback closes that gap and returns the slot to
    // the memory pool, preventing an unbounded leak on a live order book.
    book_.onRestingOrderFilled = [this](uint64_t id) {
        releaseOrder(id);
    };
}

MatchingEngine::~MatchingEngine() {
    stop();
    for (auto& [id, ptr] : liveOrders_) {
        ptr->~Order();
        orderPool_.deallocate(ptr);
    }
}

// ============================================================
//  PUBLIC SYNC API
// ============================================================

MatchingEngine::OrderId MatchingEngine::submitOrder(
    Side side, OrderType type, TimeInForce tif,
    Price price, Quantity qty)
{
    if (qty == 0) return 0;

    OrderId id = idPool_.allocate();
    if (!id) return 0;

    // Allocate from the pre-warmed slab — O(1), no heap fragmentation.
    void* mem = orderPool_.allocate();
    if (!mem) {
        idPool_.release(id);
        return 0;
    }

    // Construct in-place with the new 6-argument constructor.
    Order* order = new (mem) Order{id, side, type, tif, price, qty};
    liveOrders_[id] = order;

    // Time only the matching step — callbacks (onTrade, onRestingOrderFilled)
    // execute inside addOrder and would inflate the measurement otherwise.
    // In a production engine you'd use RDTSC here instead of steady_clock.
    PerfTimer timer;
    bool accepted = book_.addOrder(order);
    int64_t elapsed = timer.ns();

    if (!accepted) {
        // FOK rejection or bad params.
        if (onOrderRejected)
            onOrderRejected(id, "FOK not fully fillable");
        releaseOrder(id);
        return 0;
    }

    latency_.record(elapsed);

    // If a resting LIMIT order entered the book, notify listeners.
    if (order->remaining > 0 &&
        type == OrderType::LIMIT &&
        tif  == TimeInForce::GTC)
    {
        if (onOrderAccepted)
            onOrderAccepted(*order);
    }

    // If a market/IOC/FOK order was completely consumed and removed from
    // liveOrders_ by the book, it won't be in the map — that's intentional.
    // Fully-filled limit orders also leave the map via releaseOrder() below.
    if (order->remaining == 0) {
        releaseOrder(id);
    }

    return id;
}

bool MatchingEngine::cancel(OrderId id) {
    if (liveOrders_.find(id) == liveOrders_.end()) return false;

    bool ok = book_.cancelOrder(id);
    if (!ok) return false;

    releaseOrder(id);

    if (onOrderCancelled)
        onOrderCancelled(id);

    return true;
}

bool MatchingEngine::modify(OrderId id, Quantity newQty) {
    auto it = liveOrders_.find(id);
    if (it == liveOrders_.end()) return false;

    if (newQty == 0) return cancel(id);

    bool ok = book_.modifyOrder(id, newQty);
    if (!ok) return false;

    if (onOrderModified)
        onOrderModified(*it->second);

    return true;
}

// ============================================================
//  ASYNC API
// ============================================================

void MatchingEngine::start() {
    if (running_.exchange(true)) return;
    engineThread_ = std::thread(&MatchingEngine::engineLoop, this);
}

void MatchingEngine::stop() {
    if (!running_.exchange(false)) return;
    postCommand(EngineCommand::Shutdown());
    if (engineThread_.joinable())
        engineThread_.join();
}

void MatchingEngine::postCommand(const EngineCommand& cmd) {
    commandQueue_.push(cmd);
}

// ============================================================
//  ENGINE MAIN LOOP
// ============================================================

void MatchingEngine::engineLoop() {
    EngineCommand cmd;

    while (running_) {
        if (!commandQueue_.pop(cmd)) {
            // _mm_pause (PAUSE instruction) tells the CPU this is a spin-wait:
            //   - reduces power consumption
            //   - avoids memory-order mis-speculation penalty on x86
            //   - far lower latency than yield() (no scheduler involvement)
            _mm_pause();
            continue;
        }

        switch (cmd.type) {
            case CommandType::NEW_ORDER:
                submitOrder(cmd.side, cmd.orderType, cmd.tif, cmd.price, cmd.quantity);
                break;

            case CommandType::CANCEL_ORDER:
                cancel(cmd.orderId);
                break;

            case CommandType::MODIFY_ORDER:
                modify(cmd.orderId, cmd.quantity);
                break;

            case CommandType::SHUTDOWN:
                running_ = false;
                break;
        }
    }

    Logger::info("MatchingEngine stopped.");
}

// ============================================================
//  INTERNAL HELPERS
// ============================================================

void MatchingEngine::releaseOrder(OrderId id) {
    auto it = liveOrders_.find(id);
    if (it == liveOrders_.end()) return;

    Order* order = it->second;
    idPool_.release(id);

    // Explicit destructor + pool dealloc — symmetric with placement new above.
    order->~Order();
    orderPool_.deallocate(order);

    liveOrders_.erase(it);
}
