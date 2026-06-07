#pragma once
#include <cstdint>
#include "OrderTypes.hpp"

enum class FeedMessageType : uint8_t {
    NEW_ORDER,
    CANCEL_ORDER,
    MODIFY_ORDER
};

struct FeedMessage {
    FeedMessageType type;

    uint64_t    orderId   {0};
    Side        side      {};
    OrderType   orderType {};
    TimeInForce tif       {TimeInForce::GTC};
    uint64_t    price     {0};   // integer ticks
    uint64_t    quantity  {0};
};
