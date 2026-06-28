#pragma once

#include "Domain/Models/MetricData.h"

namespace Domain::ServiceInterfaces {

class IMetricCollector {
public:
    virtual ~IMetricCollector() = default;
    virtual Models::MetricData Collect() = 0;
};

} // namespace Domain::ServiceInterfaces
