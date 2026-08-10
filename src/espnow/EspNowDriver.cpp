#include "EspNowDriver.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

// ============================================================================
// Ring buffer — filled by recv callback, drained by receive()
// ============================================================================
static constexpr int RX_RING_SIZE = 8;
static constexpr int RX_BUF_SIZE = 64;

struct RxFrame {
    uint8_t data[RX_BUF_SIZE];
    uint8_t length;
    uint8_t sourceMac[6];
};

static RxFrame        s_rxRing[RX_RING_SIZE];
static volatile int   s_rxHead = 0;
static int            s_rxTail = 0;
static volatile int   s_rxDropped = 0;

// ============================================================================
// ESP-NOW callbacks (free functions)
// ============================================================================
static void onSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    Serial.print("[ESP-NOW] TX to ");
    for (int b = 0; b < 6; ++b) {
        if (b) Serial.print(':');
        Serial.print(mac_addr[b], HEX);
    }
    Serial.print(": ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    int next = (s_rxHead + 1) % RX_RING_SIZE;
    if (next != s_rxTail) {
        int copyLen = (len > RX_BUF_SIZE) ? RX_BUF_SIZE : len;
        s_rxRing[s_rxHead].length = copyLen;
        memcpy(s_rxRing[s_rxHead].data, data, copyLen);
        memcpy(s_rxRing[s_rxHead].sourceMac, mac, 6);
        s_rxHead = next;
    } else {
        s_rxDropped++;
    }
}

// ============================================================================
// EspNowDriver implementation
// ============================================================================
void EspNowDriver::begin() {
    WiFi.mode(WIFI_STA);
    // Force our STA MAC to match the cabinet's peer table (id=1), so the
    // cabinet's esp_now_send() actually reaches this board.
    uint8_t selfMac[6];
    memcpy(selfMac, EXPECTED_SELF_MAC, 6);
    esp_wifi_set_mac(WIFI_IF_STA, selfMac);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed");
        return;
    }

    esp_now_register_send_cb(onSend);
    esp_now_register_recv_cb(onRecv);

    for (int i = 0; i < NUM_PEERS; ++i) {
        bool allZero = true;
        for (int b = 0; b < 6; ++b) {
            if (PEER_MACS[i][b] != 0) { allZero = false; break; }
        }
        if (allZero) continue;

        esp_now_peer_info_t peer = {};
        peer.channel = ESP_NOW_CHANNEL;
        peer.encrypt = false;
        memcpy(peer.peer_addr, PEER_MACS[i], 6);

        esp_err_t err = esp_now_add_peer(&peer);
        if (err == ESP_OK) {
            Serial.print("[ESP-NOW] Added peer id=");
            Serial.print(PEER_IDS[i]);
            Serial.print(" mac=");
            for (int b = 0; b < 6; ++b) {
                if (b) Serial.print(':');
                Serial.print(PEER_MACS[i][b], HEX);
            }
            Serial.println();
        } else {
            Serial.print("[ESP-NOW] Failed to add peer id=");
            Serial.print(PEER_IDS[i]);
            Serial.print(" err=");
            Serial.println(err);
        }
    }

    Serial.println("[ESP-NOW] Ready");
}

bool EspNowDriver::send(DeviceId destinationId, const uint8_t* data, uint8_t length) {
    const uint8_t* mac = deviceIdToMac(destinationId);
    if (!mac) {
        Serial.print("[ESP-NOW] No MAC for id=");
        Serial.println(destinationId);
        return false;
    }

    esp_err_t err = esp_now_send(mac, data, length);
    if (err != ESP_OK) {
        Serial.print("[ESP-NOW] send failed err=");
        Serial.println(err);
        return false;
    }
    return true;
}

bool EspNowDriver::receive(uint8_t* buffer, uint8_t maxLength, uint8_t* actualLength, DeviceId* sourceId) {
    if (s_rxDropped > 0) {
        noInterrupts();
        int dropped = s_rxDropped;
        s_rxDropped = 0;
        interrupts();
        Serial.print("[ESP-NOW] RX ring overflow, dropped ");
        Serial.println(dropped);
    }
    if (s_rxTail == s_rxHead) {
        *actualLength = 0;
        *sourceId = UNPAIRED_ID;
        return false;
    }

    int len = (s_rxRing[s_rxTail].length > maxLength) ? maxLength : s_rxRing[s_rxTail].length;
    memcpy(buffer, s_rxRing[s_rxTail].data, len);
    *actualLength = len;
    *sourceId = macToDeviceId(s_rxRing[s_rxTail].sourceMac);
    s_rxTail = (s_rxTail + 1) % RX_RING_SIZE;
    return true;
}

DeviceId EspNowDriver::macToDeviceId(const uint8_t mac[6]) {
    for (int i = 0; i < NUM_PEERS; ++i) {
        if (memcmp(mac, PEER_MACS[i], 6) == 0) {
            return PEER_IDS[i];
        }
    }
    return UNPAIRED_ID;
}

const uint8_t* EspNowDriver::deviceIdToMac(DeviceId id) {
    for (int i = 0; i < NUM_PEERS; ++i) {
        if (PEER_IDS[i] == id) {
            return PEER_MACS[i];
        }
    }
    return nullptr;
}
