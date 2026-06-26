#ifndef ALERT_REPOSITORY_HPP
#define ALERT_REPOSITORY_HPP

#include "models/Alert.hpp"
#include <vector>
#include <string>

class AlertRepository {
public:
    /**
     * Creates a new alert record in the database.
     * @param alert The Alert object containing rule values and details. Sets the generated auto-increment id.
     */
    void create(Alert& alert);

    /**
     * Finds recent alerts in chronological order (newest first).
     * @param limit The maximum number of alerts to return.
     * @return A list of Alert objects, joining devices to get device_name.
     */
    std::vector<Alert> findAll(int limit = 50);

    /**
     * Finds active (unresolved) alerts in the database.
     * @return A list of unresolved Alert objects.
     */
    std::vector<Alert> findActive();

    /**
     * Checks if there is an active (unresolved) alert of a specific type for a device.
     * @param deviceId The ID of the device.
     * @param ruleType The alert rule type (e.g. "cpu", "ram", "disk").
     * @return true if an active alert exists, false otherwise.
     */
    bool hasActiveAlert(int deviceId, const std::string& ruleType);

    /**
     * Resolves all active alerts of a specific type for a device (marks resolved = TRUE).
     * @param deviceId The ID of the device.
     * @param ruleType The alert rule type.
     * @return The number of alerts resolved.
     */
    int resolveAlert(int deviceId, const std::string& ruleType);
};

#endif // ALERT_REPOSITORY_HPP
