//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once
#include "util/SignalsHandler.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

namespace app {

/**
 * @brief The main application class
 */
class ClioApplication {
    util::config::ClioConfigDefinition const& config_;
    util::SignalsHandler signalsHandler_;

public:
    /**
     * @brief Construct a new ClioApplication object
     *
     * @param config The configuration of the application
     */
    ClioApplication(util::config::ClioConfigDefinition const& config);

    /**
     * @brief Run the application
     *
     * @return exit code
     */
    int
    run();
};

}  // namespace app
