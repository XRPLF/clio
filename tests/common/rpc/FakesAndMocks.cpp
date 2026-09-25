#include "rpc/FakesAndMocks.hpp"

#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/HandlerForDefs.hpp>      // IWYU pragma: keep
#include <rpcspec/backends/BoostJson.hpp>  // IWYU pragma: keep

template struct rpc::spec::HandlerFor<tests::common::typed_fake::TypedInput, boost::json::value>;
