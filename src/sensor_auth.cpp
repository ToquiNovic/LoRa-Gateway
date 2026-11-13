#include "sensor_auth.h"
#include "config.h"

SensorAuthGuard::SensorAuthGuard()
    : psk(DEFAULT_GATEWAY_PSK),
      requireAuth(true),
      pairingActive(false),
      pairingEndTime(0),
      rejectedPackets(0) {}

void SensorAuthGuard::begin() {
    prefs.begin("gateway_auth", false);
    psk = prefs.getString("psk", DEFAULT_GATEWAY_PSK);
    requireAuth = prefs.getBool("req_auth", true);
    loadAllowlist();
    Serial.printf("[SensorAuthGuard] Initialized. PSK: %s | Require Auth: %s | Allowed Sensors: %u\n",
                  psk.c_str(), requireAuth ? "YES" : "NO", (unsigned)allowlist.size());
}

bool SensorAuthGuard::validateKey(const String& key) const {
    if (key.length() == 0) return false;
    return (key == psk);
}

bool SensorAuthGuard::isSensorAuthorized(const String& sensorId, const String& providedKey) {
    if (!requireAuth) {
        return true;
    }

    // 1. Direct Pre-Shared Key match
    if (providedKey.length() > 0 && validateKey(providedKey)) {
        return true;
    }

    // 2. Allowlist match
    if (sensorId.length() > 0 && isInAllowlist(sensorId)) {
        return true;
    }

    // 3. Pairing Mode (WPS active)
    if (isPairingActive() && sensorId.length() > 0) {
        addToAllowlist(sensorId);
        Serial.printf("[SensorAuthGuard] PAIRED new sensor '%s' into Allowlist during pairing window\n", sensorId.c_str());
        return true;
    }

    // Unauthorized packet
    rejectedPackets++;
    Serial.printf("[SensorAuthGuard] REJECTED unauthorized sensor '%s' (Key: '%s', Total Rejections: %u)\n",
                  sensorId.c_str(), providedKey.c_str(), rejectedPackets);
    return false;
}

void SensorAuthGuard::startPairing(uint32_t durationSec) {
    pairingActive = true;
    pairingEndTime = millis() + (durationSec * 1000);
    Serial.printf("[SensorAuthGuard] Pairing Mode (WPS) ACTIVATED for %u seconds\n", durationSec);
}

void SensorAuthGuard::stopPairing() {
    pairingActive = false;
    pairingEndTime = 0;
    Serial.println("[SensorAuthGuard] Pairing Mode DEACTIVATED");
}

bool SensorAuthGuard::isPairingActive() const {
    if (!pairingActive) return false;
    return (millis() < pairingEndTime);
}

uint32_t SensorAuthGuard::getPairingRemainingSec() const {
    if (!isPairingActive()) return 0;
    return (pairingEndTime - millis()) / 1000;
}

void SensorAuthGuard::setPsk(const String& newPsk) {
    if (newPsk.length() == 0) return;
    psk = newPsk;
    prefs.putString("psk", psk);
    Serial.printf("[SensorAuthGuard] PSK updated to: %s\n", psk.c_str());
}

String SensorAuthGuard::getPsk() const {
    return psk;
}

void SensorAuthGuard::setRequireAuth(bool require) {
    requireAuth = require;
    prefs.putBool("req_auth", requireAuth);
}

bool SensorAuthGuard::isAuthRequired() const {
    return requireAuth;
}

void SensorAuthGuard::addToAllowlist(const String& sensorId) {
    if (sensorId.length() == 0 || isInAllowlist(sensorId)) return;
    allowlist.push_back(sensorId);
    saveAllowlist();
}

void SensorAuthGuard::removeFromAllowlist(const String& sensorId) {
    for (auto it = allowlist.begin(); it != allowlist.end(); ++it) {
        if (*it == sensorId) {
            allowlist.erase(it);
            saveAllowlist();
            break;
        }
    }
}

bool SensorAuthGuard::isInAllowlist(const String& sensorId) const {
    for (const auto& item : allowlist) {
        if (item == sensorId) return true;
    }
    return false;
}

std::vector<String> SensorAuthGuard::getAllowlist() const {
    return allowlist;
}

uint32_t SensorAuthGuard::getRejectedCount() const {
    return rejectedPackets;
}

void SensorAuthGuard::resetRejectedCount() {
    rejectedPackets = 0;
}

void SensorAuthGuard::loadAllowlist() {
    allowlist.clear();
    String raw = prefs.getString("allowlist", "");
    if (raw.length() == 0) return;

    int start = 0;
    int end = raw.indexOf(',');
    while (end != -1) {
        String token = raw.substring(start, end);
        token.trim();
        if (token.length() > 0) allowlist.push_back(token);
        start = end + 1;
        end = raw.indexOf(',', start);
    }
    String lastToken = raw.substring(start);
    lastToken.trim();
    if (lastToken.length() > 0) allowlist.push_back(lastToken);
}

void SensorAuthGuard::saveAllowlist() {
    String raw = "";
    for (size_t i = 0; i < allowlist.size(); i++) {
        if (i > 0) raw += ",";
        raw += allowlist[i];
    }
    prefs.putString("allowlist", raw);
}
