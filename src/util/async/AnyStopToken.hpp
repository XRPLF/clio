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

#include "util/async/Concepts.hpp"

#include <memory>
#include <type_traits>

namespace util::async {

/**
 * @brief A type-erased stop token
 */
class AnyStopToken {
public:
    template <SomeStopToken TokenType>
        requires(not std::is_same_v<std::decay_t<TokenType>, AnyStopToken>)
    /* implicit */ AnyStopToken(TokenType&& token)
        : pimpl_{std::make_unique<Model<TokenType>>(std::forward<TokenType>(token))}
    {
    }

    ~AnyStopToken() = default;

    AnyStopToken(AnyStopToken const& other) : pimpl_{other.pimpl_->clone()}
    {
    }

    AnyStopToken&
    operator=(AnyStopToken const& rhs)
    {
        AnyStopToken copy{rhs};
        pimpl_.swap(copy.pimpl_);
        return *this;
    }

    AnyStopToken(AnyStopToken&&) = default;
    AnyStopToken&
    operator=(AnyStopToken&&) = default;

    /** @returns true if stop is requested; false otherwise */
    [[nodiscard]] bool
    isStopRequested() const noexcept
    {
        return pimpl_->isStopRequested();
    }

    /** @returns true if stop is requested; false otherwise */
    [[nodiscard]] operator bool() const noexcept
    {
        return isStopRequested();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        [[nodiscard]] virtual bool
        isStopRequested() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<Concept>
        clone() const = 0;
    };

    template <SomeStopToken TokenType>
    struct Model : Concept {
        TokenType token;

        Model(TokenType&& token) : token{std::move(token)}
        {
        }

        [[nodiscard]] bool
        isStopRequested() const noexcept override
        {
            return token.isStopRequested();
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

}  // namespace util::async
