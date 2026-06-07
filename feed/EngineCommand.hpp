#pragma once

#include <cstdint>
#include "OrderTypes.hpp"

enum class CommandType {
    NEW_ORDER,
    CANCEL_ORDER,
    MODIFY_ORDER,
    SHUTDOWN
};

struct EngineCommand {
    CommandType type;

    uint64_t    orderId   {0};
    Side        side      {};
    OrderType   orderType {};
    TimeInForce tif       {TimeInForce::GTC};
    uint64_t    price     {0};   // integer ticks — no floating-point in the hot path
    uint64_t    quantity  {0};

    // Convenience factories
    static EngineCommand New(Side s, OrderType t, TimeInForce tif,
                             uint64_t p, uint64_t q) {
        return { CommandType::NEW_ORDER, 0, s, t, tif, p, q };
    }

    static EngineCommand Cancel(uint64_t id) {
        EngineCommand c;
        c.type    = CommandType::CANCEL_ORDER;
        c.orderId = id;
        return c;
    }

    static EngineCommand Modify(uint64_t id, uint64_t q) {
        EngineCommand c;
        c.type     = CommandType::MODIFY_ORDER;
        c.orderId  = id;
        c.quantity = q;
        return c;
    }

    static EngineCommand Shutdown() {
        EngineCommand c;
        c.type = CommandType::SHUTDOWN;
        return c;
    }
};
