#include "rpc/FakesAndMocks.hpp"

#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/HandlerForDefs.hpp>  // IWYU pragma: keep

template struct rpc::spec::HandlerFor<tests::common::typed_fake::TypedInput>;
