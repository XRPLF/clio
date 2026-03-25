#pragma once

#include "feed/impl/SingleFeedBase.hpp"

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>

namespace feed::impl {

/**
 * @brief Feed that publishes the json object as it is.
 */
struct ForwardFeed : public SingleFeedBase {
    using SingleFeedBase::SingleFeedBase;

    /**
     * @brief Publishes the json object.
     */
    void
    pub(boost::json::object const& json)
    {
        SingleFeedBase::pub(boost::json::serialize(json));
    }
};
}  // namespace feed::impl
