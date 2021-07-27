#pragma once

#ifdef DEV_BOARD
    #define LED_PIN (LED_BUILTIN)
    #define BTN_PIN (0)
#else
    #define LED_PIN (16)
    #define BTN_PIN (5)
#endif
