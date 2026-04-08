#include "util/Assert.hpp"

#include "util/log/Logger.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

namespace util::impl {

OnAssert::ActionType OnAssert::action;

void
OnAssert::call(std::string_view message)
{
    if (not OnAssert::action) {
        resetAction();
    }
    OnAssert::action(message);
}

void
OnAssert::setAction(ActionType newAction)
{
    OnAssert::action = std::move(newAction);
}

void
OnAssert::resetAction()
{
    OnAssert::action = [](std::string_view m) { OnAssert::defaultAction(m); };
}

void
OnAssert::defaultAction(std::string_view message)
{
    if (LogServiceState::initialized() and LogServiceState::hasSinks()) {
        LOG(LogService::fatal()) << message;
    } else {
        std::cerr << message << std::endl;
    }
    std::exit(EXIT_FAILURE);  // std::abort does not flush gcovr output and causes uncovered lines
}

}  // namespace util::impl
