#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "Protocol.h"
#include "EspNowConfig.h"

class EspNowDriver {
public:
    void begin();
    bool send(DeviceId destinationId, const uint8_t* data, uint8_t length);
    bool receive(uint8_t* buffer, uint8_t maxLength, uint8_t* actualLength, DeviceId* sourceId);

private:
    static DeviceId macToDeviceId(const uint8_t mac[6]);
    static const uint8_t* deviceIdToMac(DeviceId id);
};
