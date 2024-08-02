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

#include "util/OverloadSet.hpp"

#include <string>
#include <variant>

namespace app {

/**
 * @brief Parsed command line arguments representation.
 */
class CliArgs {
public:
    /**
     * @brief Default configuration path.
     */
    static constexpr char defaultConfigPath[] = "/etc/opt/clio/config.json";

    /**
     * @brief An action parsed from the command line.
     */
    class Action {
    public:
        /** @brief Run action. */
        struct Run {
            /** @brief Configuration file path. */
            std::string configPath;
        };

        /** @brief Exit action. */
        struct Exit {
            /** @brief Exit code. */
            int exitCode;
        };

        /**
         * @brief Construct an action from a Run.
         *
         * @param action Run action.
         */
        template <typename ActionType>
            requires std::is_same_v<ActionType, Run> or std::is_same_v<ActionType, Exit>
        explicit Action(ActionType&& action) : action_(std::forward<ActionType>(action))
        {
        }

        /**
         * @brief Apply a function to the action.
         *
         * @tparam Processors Action processors types. Must be callable with the action type and return int.
         * @param processors Action processors.
         * @return Exit code.
         */
        template <typename... Processors>
        int
        apply(Processors&&... processors) const
        {
            return std::visit(util::OverloadSet{std::forward<Processors>(processors)...}, action_);
        }

    private:
        std::variant<Run, Exit> action_;
    };

    /**
     * @brief Parse command line arguments.
     *
     * @param argc Number of arguments.
     * @param argv Array of arguments.
     * @return Parsed command line arguments.
     */
    static Action
    parse(int argc, char const* argv[]);
};

}  // namespace app
