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

#include "util/async/AnyStopToken.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/impl/ErasedOperation.hpp"

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace util::async {

/**
 * @brief A type-erased execution context
 */
class AnyStrand {
public:
    /**
     * @brief Construct a new Any Strand object
     *
     * @tparam StrandType The type of the strand to wrap
     * @param strand The strand to wrap
     */
    template <typename StrandType>
        requires(not std::is_same_v<std::decay_t<StrandType>, AnyStrand>)
    /* implicit */ AnyStrand(StrandType&& strand)
        : pimpl_{std::make_unique<Model<StrandType>>(std::forward<StrandType>(strand))}
    {
    }

    ~AnyStrand() = default;

    /**
     * @brief Execute a function without a stop token on the strand
     *
     * @param fn The function to execute
     * @return The type-erased operation
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn())>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(  //
            pimpl_->execute([fn = std::forward<decltype(fn)>(fn)]() -> std::any {
                if constexpr (std::is_void_v<RetType>) {
                    fn();
                    return {};
                } else {
                    return std::make_any<RetType>(fn());
                }
            })
        );
    }

    /**
     * @brief Execute a function taking a stop token on the strand
     *
     * @param fn The function to execute
     * @return The type-erased operation
     */
    [[nodiscard]] auto
    execute(SomeHandlerWith<AnyStopToken> auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(  //
            pimpl_->execute([fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> std::any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken));
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken)));
                }
            })
        );
    }

    /**
     * @brief Execute a function taking a stop token on the strand with a timeout
     *
     * @param fn The function to execute
     * @param timeout The timeout for the function
     * @return The type-erased operation
     */
    [[nodiscard]] auto
    execute(SomeHandlerWith<AnyStopToken> auto&& fn, SomeStdDuration auto timeout)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(  //
            pimpl_->execute(
                [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> std::any {
                    if constexpr (std::is_void_v<RetType>) {
                        fn(std::move(stopToken));
                        return {};
                    } else {
                        return std::make_any<RetType>(fn(std::move(stopToken)));
                    }
                },
                std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
            )
        );
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        [[nodiscard]] virtual impl::ErasedOperation
        execute(
            std::function<std::any(AnyStopToken)>,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt
        ) = 0;
        [[nodiscard]] virtual impl::ErasedOperation execute(std::function<std::any()>) = 0;
    };

    template <typename StrandType>
    struct Model : Concept {
        StrandType strand;

        template <typename SType>
            requires std::is_same_v<SType, StrandType>
        Model(SType&& strand) : strand{std::forward<SType>(strand)}
        {
        }

        [[nodiscard]] impl::ErasedOperation
        execute(std::function<std::any(AnyStopToken)> fn, std::optional<std::chrono::milliseconds> timeout) override
        {
            return strand.execute(std::move(fn), timeout);
        }

        [[nodiscard]] impl::ErasedOperation
        execute(std::function<std::any()> fn) override
        {
            return strand.execute(std::move(fn));
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async
