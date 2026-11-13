#ifndef SENSOR_AUTH_H
#define SENSOR_AUTH_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>

// ====================================================================
// SENSOR AUTH GUARD - Link-Level Credential & Pairing Verification
// ====================================================================

class SensorAuthGuard {
public:
    SensorAuthGuard();

    void begin();

    // Credential Verification
    bool validateKey(const String& key) const;
    bool isSensorAuthorized(const String& sensorId, const String& providedKey = "");

    // WPS-style Pairing Mode
    void startPairing(uint32_t durationSec = 60);
    void stopPairing();
    bool isPairingActive() const;
    uint32_t getPairingRemainingSec() const;

    // Configuration & Allowlist
    void setPsk(const String& newPsk);
    String getPsk() const;

    void setRequireAuth(bool require);
    bool isAuthRequired() const;

    void addToAllowlist(const String& sensorId);
    void removeFromAllowlist(const String& sensorId);
    bool isInAllowlist(const String& sensorId) const;
    std::vector<String> getAllowlist() const;

    // Security Metrics
    uint32_t getRejectedCount() const;
    void resetRejectedCount();

private:
    Preferences prefs;
    String psk;
    bool requireAuth;
    bool pairingActive;
    unsigned long pairingEndTime;
    uint32_t rejectedPackets;
    std::vector<String> allowlist;

    void loadAllowlist();
    void saveAllowlist();
};

#endif // SENSOR_AUTH_H
