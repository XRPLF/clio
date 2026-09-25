#pragma once

#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/RpcSpecView.hpp>
#include <rpcspec/backends/BoostJson.hpp>

namespace rpc {

/**
 * @brief The object view the shared specs read Clio's requests through.
 *
 * The spec library names no JSON type: a consumer picks a backend and binds it once. This
 * is Clio's choice, and the counterpart of xrpld's json::Value views.
 */
using SpecObjectView = spec::BoostJsonObjectView;

/**
 * @brief A type-erased view of a handler's spec, uniform across API versions.
 */
using SpecView = spec::RpcSpecView<SpecObjectView>;

/**
 * @brief The spec-driven base every typed handler inherits.
 *
 * Binding the backend here, once, is what keeps handlers and call sites free of it:
 * @c Handler::parseInput and @c Handler::spec are ordinary static members.
 *
 * @tparam InputT The handler's request Input struct.
 */
template <typename InputT>
using HandlerFor = spec::HandlerFor<InputT, boost::json::value>;

}  // namespace rpc
