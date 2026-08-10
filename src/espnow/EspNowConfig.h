#pragma once

#include <cstdint>
#include "Protocol.h"

constexpr uint8_t ESP_NOW_CHANNEL = 1;

// ============================================================================
// THIS HOURGLASS
// ============================================================================
// Device ID on the cabinet network (registers as id=1 on the cabinet side).
constexpr DeviceId HOURGLASS_SELF_ID = 1;

// MAC the cabinet expects for id=1 (see cabinet EspNowConfig.h).
// If WiFi.macAddress() does not match, update the cabinet's PEER_MACS table.
static constexpr uint8_t EXPECTED_SELF_MAC[6] = {0x20, 0x50, 0x0D, 0x08, 0x25, 0x90};

// ============================================================================
// PEER MAC ADDRESS TABLE
// ============================================================================
// On the hourglass build, the sole peer is the cabinet.
static constexpr DeviceId PEER_IDS[] = { CABINET_ID };
static constexpr uint8_t  PEER_MACS[][6] = {
    {0x8C, 0x94, 0xDF, 0x47, 0xEE, 0xB8},  // Cabinet MAC
};
constexpr int NUM_PEERS = sizeof(PEER_IDS) / sizeof(PEER_IDS[0]);
