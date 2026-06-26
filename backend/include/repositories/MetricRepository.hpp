#ifndef METRIC_REPOSITORY_HPP
#define METRIC_REPOSITORY_HPP

#include "models/Metric.hpp"
#include <optional>
#include <vector>

class MetricRepository {
public:
    /**
     * Creates a new metric record in the database.
     * Also updates the device's status to 'online'.
     * @param metric The Metric object containing values to ingest.
     */
    void create(Metric& metric);

    /**
     * Finds the latest metric record for a given device.
     * @param deviceId The ID of the device.
     * @return An optional containing the latest Metric if found, or std::nullopt.
     */
    std::optional<Metric> findLatestForDevice(int deviceId);

    /**
     * Finds historical metric records for a given device.
     * @param deviceId The ID of the device.
     * @param limit The maximum number of records to return.
     * @return A vector of historical Metric records.
     */
    std::vector<Metric> findHistoryForDevice(int deviceId, int limit);
};


#endif // METRIC_REPOSITORY_HPP
