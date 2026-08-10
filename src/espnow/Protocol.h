#pragma once

#include <cstdint>

// ============================================================================
// PROTOCOL VERSION
// ============================================================================
// Increment when packet format changes. Both sides must match or reject.
constexpr uint8_t PROTOCOL_VERSION = 1;

// ============================================================================
// DEVICE IDENTIFIERS
// ============================================================================
using DeviceId = uint8_t;

constexpr DeviceId CABINET_ID  = 0;  // Main cabinet is always ID 0
constexpr DeviceId UNPAIRED_ID = 0;  // Unpaired hourglass / display

// ============================================================================
// STATION STATE
// ============================================================================
enum StationState {
    AVAILABLE,
    SELECTED,
    WAITING_FOR_PAYMENT,
    CHARGING,
    FINISHED,
    ERROR
};

// ============================================================================
// PACKET TYPE ENUM
// ============================================================================
enum PacketType : uint8_t {
    STATION_STATUS   = 0x01,  // Hourglass->Cabinet: status report (state, time, battery)
                             // Cabinet->Hourglass (quirk): StationPacket per station
    HEARTBEAT        = 0x02,  // Keepalive request
    FLIP_EVENT       = 0x03,  // Hourglass was flipped
    LOW_BATTERY      = 0x04,  // Battery below threshold
    ERROR_REPORT     = 0x05,  // Hourglass encountered an error

    HEARTBEAT_ACK    = 0x10,  // Response to heartbeat
    START_CHARGING   = 0x11,  // Begin charging session
    STOP_CHARGING    = 0x12,  // Stop charging session
    TIME_SYNC        = 0x13,  // Update remaining time
};

// ============================================================================
// PACKET HEADER — present in every transmission
// ============================================================================
struct PacketHeader {
    uint8_t     protocolVersion;
    PacketType  type;
    DeviceId    sourceId;
    DeviceId    destinationId;
    uint8_t     payloadSize;
} __attribute__((packed));

// ============================================================================
// HOURGLASS -> CABINET PACKETS
// ============================================================================
struct StationStatusPacket {
    uint8_t       stationIndex;
    StationState  state;
    uint16_t      remainingTime;
    uint8_t       batteryLevel;
} __attribute__((packed));

struct HeartbeatPacket {
    uint32_t uptimeMs;
} __attribute__((packed));

// ============================================================================
// CABINET -> HOURGLASS PACKETS
// ============================================================================
// Station status broadcast — cabinet reports each station's current state.
// Sent periodically or on state change. Sequence number enables gap detection.
struct StationPacket {
    uint8_t  stationId;
    uint8_t  portState;
    uint16_t remainingSeconds;
    uint8_t  sequenceNumber;
} __attribute__((packed));

struct HeartbeatAckPacket {
    uint32_t uptimeMs;
} __attribute__((packed));
