#pragma once

#include "bemfa.h"
#include "devices.h"

void register_panasonic_light_01_handler(BemfaMqtt &bemfaMqtt, const String& topicPrefix, Led *ledToggle);
