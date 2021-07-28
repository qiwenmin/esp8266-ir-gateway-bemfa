#pragma once

#include <Arduino.h>

#ifdef ENABLE_DEBUG_LOG
    extern Print* DebugLogger;
    #define DEBUG_LOG(...) DebugLogger->print(__VA_ARGS__)
    #define DEBUG_LOG_LN(...) DebugLogger->println(__VA_ARGS__)
#else
    #define DEBUG_LOG(...) (static_cast<void>(0))
    #define DEBUG_LOG_LN(...)  (static_cast<void>(0))
#endif
