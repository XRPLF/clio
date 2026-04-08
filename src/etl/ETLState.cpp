#include "etl/ETLState.hpp"

#include "rpc/JS.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>

namespace etl {

ETLState
tag_invoke(boost::json::value_to_tag<ETLState>, boost::json::value const& jv)
{
    ETLState state;
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(result)) &&
        jsonObject.at(JS(result)).as_object().contains(JS(info))) {
        auto const rippledInfo = jsonObject.at(JS(result)).as_object().at(JS(info)).as_object();
        if (rippledInfo.contains(JS(network_id)))
            state.networkID = boost::json::value_to<int64_t>(rippledInfo.at(JS(network_id)));
    }

    return state;
}

}  // namespace etl
