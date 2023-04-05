//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <rpc/common/Concepts.h>
#include <rpc/common/Types.h>
#include <rpc/common/impl/Processors.h>

namespace RPCng {

/**
 * @brief A type-erased Handler that can contain any (NextGen) RPC handler class
 *
 * This allows to store different handlers in one map/vector etc.
 * Support for copying was added in order to allow storing in a
 * map/unordered_map using the initializer_list constructor.
 */
class AnyHandler final
{
public:
    /**
     * @brief Type-erases any handler class.
     *
     * @tparam HandlerType The real type of wrapped handler class
     * @tparam ProcessingStrategy A strategy that implements how processing of
     * JSON is to be done
     * @param handler The handler to wrap. Required to fulfil the @ref Handler
     * concept.
     */
    template <
        Handler HandlerType,
        typename ProcessingStrategy = detail::DefaultProcessor<HandlerType>>
    /* implicit */ AnyHandler(HandlerType&& handler)
        : pimpl_{std::make_unique<Model<HandlerType, ProcessingStrategy>>(
              std::forward<HandlerType>(handler))}
    {
    }

    ~AnyHandler() = default;
    AnyHandler(AnyHandler const& other) : pimpl_{other.pimpl_->clone()}
    {
    }
    AnyHandler&
    operator=(AnyHandler const& rhs)
    {
        AnyHandler copy{rhs};
        pimpl_.swap(copy.pimpl_);
        return *this;
    }
    AnyHandler(AnyHandler&&) = default;
    AnyHandler&
    operator=(AnyHandler&&) = default;

    /**
     * @brief Process incoming JSON by the stored handler
     *
     * @param value The JSON to process
     * @return JSON result or @ref RPC::Status on error
     */
    [[nodiscard]] ReturnType
    process(boost::json::value const& value) const
    {
        return pimpl_->process(value);
    }

    /**
     * @brief Process incoming JSON by the stored handler in a provided
     * coroutine
     *
     * @param value The JSON to process
     * @return JSON result or @ref RPC::Status on error
     */
    [[nodiscard]] ReturnType
    process(boost::json::value const& value, Context const& ctx) const
    {
        return pimpl_->process(value, ctx);
    }

private:
    struct Concept
    {
        virtual ~Concept() = default;

        [[nodiscard]] virtual ReturnType
        process(boost::json::value const& value, Context const& ctx) const = 0;

        [[nodiscard]] virtual ReturnType
        process(boost::json::value const& value) const = 0;

        [[nodiscard]] virtual std::unique_ptr<Concept>
        clone() const = 0;
    };

    template <typename HandlerType, typename ProcessorType>
    struct Model : Concept
    {
        HandlerType handler;
        ProcessorType processor;

        Model(HandlerType&& handler) : handler{std::move(handler)}
        {
        }

        [[nodiscard]] ReturnType
        process(boost::json::value const& value) const override
        {
            return processor(handler, value);
        }

        [[nodiscard]] ReturnType
        process(boost::json::value const& value, Context const& ctx)
            const override
        {
            return processor(handler, value, &ctx);
        }

        [[nodiscard]] std::unique_ptr<Concept>
        clone() const override
        {
            return std::make_unique<Model>(*this);
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace RPCng
