#pragma once

#include <concepts>

namespace cluster {

/**
 * @brief Tag type for cluster communication service implementations.
 *
 * This tag is used to identify types that implement cluster communication functionality.
 * Types should inherit from this tag to be recognized as cluster communication services.
 */
struct ClusterCommunicationServiceTag {
    virtual ~ClusterCommunicationServiceTag() = default;
};

template <typename T>
concept SomeClusterCommunicationService = std::derived_from<T, ClusterCommunicationServiceTag>;

}  // namespace cluster
