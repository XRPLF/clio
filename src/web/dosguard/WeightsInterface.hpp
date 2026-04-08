#pragma once

#include <boost/json/object.hpp>

#include <cstddef>

namespace web::dosguard {

/**
 * @brief Interface for determining request weights in DOS protection.
 *
 * This interface defines the contract for classes that calculate weights for incoming
 * requests, which is used for DOS protection mechanisms.
 */
class WeightsInterface {
public:
    virtual ~WeightsInterface() = default;

    /**
     * @brief Calculate the weight of a request.
     *
     * @param request The JSON object representing the request
     * @return The calculated weight of the request
     */
    virtual size_t
    requestWeight(boost::json::object const& request) const = 0;
};

}  // namespace web::dosguard
