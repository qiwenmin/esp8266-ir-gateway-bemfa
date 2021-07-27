#pragma once

#include <Arduino.h>

class NullPrint : public Print {
public:
    virtual size_t write(uint8_t) { return 1; };
};

class DebugPrint {
public:
    DebugPrint() : _debug_print(0) {};

    void setDebugPrint(Print *print) {
        _debug_print = print;
    };

    Print *debugPrint() {
        return _debug_print ? _debug_print : &_null_print;
    };

private:
    Print *_debug_print;
    NullPrint _null_print;
};
