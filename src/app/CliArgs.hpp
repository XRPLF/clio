#pragma once

#include "migration/MigrationApplication.hpp"
#include "util/OverloadSet.hpp"

#include <string>
#include <variant>

/**
 * @brief This namespace contains the application-level logic: command line parsing, startup and
 * shutdown.
 */
namespace app {

/**
 * @brief Parsed command line arguments representation.
 */
class CliArgs {
public:
    /**
     * @brief Default configuration path.
     */
    static constexpr char kDefaultConfigPath[] = "/etc/opt/clio/config.json";

    /**
     * @brief An action parsed from the command line.
     */
    class Action {
    public:
        /**
         * @brief Run action.
         */
        struct Run {
            std::string configPath;  ///< Configuration file path.
            bool useNgWebServer;     ///< Whether to use a ng web server
        };

        /**
         * @brief Exit action.
         */
        struct Exit {
            int exitCode;  ///< Exit code.
        };

        /**
         * @brief Migration action.
         */
        struct Migrate {
            std::string configPath;  ///< Configuration file path.
            MigrateSubCmd subCmd;    ///< Migration subcommand to execute.
        };

        /**
         * @brief Verify Config action.
         */
        struct VerifyConfig {
            std::string configPath;  ///< Path of the configuration file to verify.
        };

        /**
         * @brief Construct an action from one of the action types.
         *
         * @tparam ActionType The concrete action type; one of Run, Exit, Migrate or VerifyConfig.
         * @param action The action to wrap.
         */
        template <typename ActionType>
            requires std::is_same_v<ActionType, Run> or std::is_same_v<ActionType, Exit> or
            std::is_same_v<ActionType, Migrate> or std::is_same_v<ActionType, VerifyConfig>
        explicit Action(ActionType&& action) : action_(std::forward<ActionType>(action))
        {
        }

        /**
         * @brief Apply a function to the action.
         *
         * @tparam Processors Action processors types. Must be callable with the action type and
         * return int.
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
        std::variant<Run, Exit, Migrate, VerifyConfig> action_;
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
