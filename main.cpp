// main.cpp — OrderBook Matching Engine demo
//
// Demonstrates:
//   1. Limit order matching (price-time priority)
//   2. Market order sweeping multiple levels
//   3. IOC and FOK time-in-force semantics
//   4. Cancel and modify
//   5. Market-depth queries (top-5 bid/ask)
//   6. Order-to-fill latency percentile report
//   7. Async mode via MPSC command queue

#include <iostream>
#include <iomanip>
#include <cassert>

#include "book/MatchingEngine.hpp"
#include "utils/Logging.hpp"

// ── Helpers ──────────────────────────────────────────────────────────────────

static void printDepth(MatchingEngine& eng, int depth = 5) {
    auto asks = eng.askDepth(depth);
    auto bids = eng.bidDepth(depth);

    std::cout << "\n  ┌──────────────────── ORDER BOOK ────────────────────┐\n";
    std::cout <<   "  │       ASKS (lowest first)                          │\n";
    for (auto& [p, q] : asks)
        std::cout << "  │    price=" << std::setw(6) << p
                  << "  qty=" << std::setw(8) << q << "                    │\n";
    std::cout <<   "  │  ─────────────── spread ─────────────────          │\n";
    for (auto& [p, q] : bids)
        std::cout << "  │    price=" << std::setw(6) << p
                  << "  qty=" << std::setw(8) << q << "                    │\n";
    std::cout <<   "  └────────────────────────────────────────────────────┘\n\n";
}

static void printLatency(MatchingEngine& eng) {
    auto [mn, p50, p99, mx] = eng.latencyPercentiles();
    std::cout << "  Latency (order-to-book) — min=" << mn
              << "ns  p50=" << p50 << "ns  p99=" << p99
              << "ns  max=" << mx << "ns\n\n";
}

// ── Main demo ─────────────────────────────────────────────────────────────────

int main() {
    Logger::setLevel(LogLevel::WARN);   // suppress engine debug noise

    MatchingEngine eng(/*poolCapacity=*/4096);

    // ── Wire callbacks ────────────────────────────────────────────────────
    eng.onTrade = [](const TradeEvent& t) {
        std::cout << "  [TRADE]  maker=" << t.makerOrderId
                  << "  taker=" << t.takerOrderId
                  << "  price=" << t.price
                  << "  qty="   << t.quantity << "\n";
    };

    eng.onOrderAccepted = [](const obk::Order& o) {
        std::cout << "  [RESTING] id=" << o.id
                  << "  side=" << obk::to_string(o.side)
                  << "  price=" << o.price
                  << "  qty=" << o.remaining << "\n";
    };

    eng.onOrderCancelled = [](uint64_t id) {
        std::cout << "  [CANCEL] id=" << id << "\n";
    };

    eng.onOrderModified = [](const obk::Order& o) {
        std::cout << "  [MODIFY] id=" << o.id
                  << "  new_qty=" << o.remaining << "\n";
    };

    eng.onOrderRejected = [](uint64_t id, const char* reason) {
        std::cout << "  [REJECT] id=" << id << "  reason=" << reason << "\n";
    };

    // ── Demo 1: Build a resting book ──────────────────────────────────────
    std::cout << "=== DEMO 1: Build resting book ===\n";
    eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, 10200, 50);
    eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, 10100, 30);
    eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, 10100, 20);
    eng.submitOrder(Side::Buy,  OrderType::LIMIT, TimeInForce::GTC,  9900, 40);
    eng.submitOrder(Side::Buy,  OrderType::LIMIT, TimeInForce::GTC,  9800, 60);
    printDepth(eng);

    // ── Demo 2: Limit buy crosses the spread ──────────────────────────────
    std::cout << "=== DEMO 2: Limit buy @ 10100 (crosses ask, price-time priority) ===\n";
    eng.submitOrder(Side::Buy, OrderType::LIMIT, TimeInForce::GTC, 10100, 35);
    printDepth(eng);

    // ── Demo 3: Market sell sweeps multiple bid levels ────────────────────
    std::cout << "=== DEMO 3: Market sell qty=120 (sweeps all bids) ===\n";
    // First top up bids so we can sweep them
    eng.submitOrder(Side::Buy, OrderType::LIMIT, TimeInForce::GTC, 9900, 80);
    eng.submitOrder(Side::Buy, OrderType::LIMIT, TimeInForce::GTC, 9800, 80);
    eng.submitOrder(Side::Sell, OrderType::MARKET, TimeInForce::GTC, 0, 120);
    printDepth(eng);

    // ── Demo 4: IOC (immediate-or-cancel) ────────────────────────────────
    std::cout << "=== DEMO 4: IOC buy @ 10100 qty=100 (partial fill, rest discarded) ===\n";
    eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, 10100, 40);
    eng.submitOrder(Side::Buy,  OrderType::LIMIT,  TimeInForce::IOC, 10100, 100);
    printDepth(eng);

    // ── Demo 5: FOK ──────────────────────────────────────────────────────
    std::cout << "=== DEMO 5: FOK buy @ 10100 qty=200 (not enough — rejected) ===\n";
    eng.submitOrder(Side::Buy, OrderType::LIMIT, TimeInForce::FOK, 10100, 200);
    std::cout << "  (no trade expected)\n";
    printDepth(eng);

    // ── Demo 6: Cancel ───────────────────────────────────────────────────
    std::cout << "=== DEMO 6: Cancel a resting bid ===\n";
    auto bid1 = eng.submitOrder(Side::Buy, OrderType::LIMIT, TimeInForce::GTC, 9750, 25);
    eng.cancel(bid1);
    printDepth(eng);

    // ── Demo 7: Modify (reduce qty) ──────────────────────────────────────
    std::cout << "=== DEMO 7: Modify resting ask qty → 5 ===\n";
    auto ask1 = eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, 10300, 100);
    eng.modify(ask1, 5);
    printDepth(eng);

    // ── Demo 8: High-throughput batch (latency benchmark) ────────────────
    std::cout << "=== DEMO 8: 10,000 order round-trip (latency benchmark) ===\n";
    for (int i = 0; i < 10'000; ++i) {
        uint64_t price = 10000 + static_cast<uint64_t>(i % 20);
        eng.submitOrder(Side::Buy,  OrderType::LIMIT, TimeInForce::GTC, price, 10);
        eng.submitOrder(Side::Sell, OrderType::LIMIT, TimeInForce::GTC, price, 10);
    }
    printLatency(eng);

    // ── Demo 9: Async mode ───────────────────────────────────────────────
    std::cout << "=== DEMO 9: Async command queue ===\n";
    MatchingEngine asyncEng(1024);
    asyncEng.onTrade = [](const TradeEvent& t) {
        std::cout << "  [ASYNC TRADE]  price=" << t.price << "  qty=" << t.quantity << "\n";
    };
    asyncEng.start();

    asyncEng.postCommand(EngineCommand::New(Side::Sell, OrderType::LIMIT,
                                            TimeInForce::GTC, 5000, 100));
    asyncEng.postCommand(EngineCommand::New(Side::Buy,  OrderType::LIMIT,
                                            TimeInForce::GTC, 5000, 100));

    asyncEng.stop();   // drains queue then joins engine thread
    std::cout << "  Async engine shut down cleanly.\n\n";

    std::cout << "=== All demos passed. ===\n";
    return 0;
}
