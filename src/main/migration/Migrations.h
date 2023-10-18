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

#include <backend/CassandraBackend.h>
#include <config/Config.h>
#include <log/Logger.h>

#include <ripple/basics/base64.h>
#include <boost/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

struct ResumeContext
{
    std::string tag;
    boost::json::object data;

    ResumeContext(std::string tag, boost::json::object data)
        : tag{std::move(tag)}, data{std::move(data)}
    {
    }
};

class ResumeContextProvider
{
    std::filesystem::path path_;

public:
    ResumeContextProvider(std::filesystem::path path) : path_{path}
    {
        clio::LogService::info() << "Resume context path: " << path_.string();
    }

    std::optional<ResumeContext>
    load()
    {
        if (not std::filesystem::exists(path_))
            return std::nullopt;

        auto const is = std::ifstream{path_.string()};
        if (not is.is_open())
            return std::nullopt;

        auto buffer = std::stringstream{};
        buffer << is.rdbuf();

        auto const value = boost::json::parse(buffer.str());
        if (not value.is_object())
            return std::nullopt;

        auto const& obj = value.as_object();
        if (not obj.contains("step") or not obj.contains("state"))
            return std::nullopt;

        return std::make_optional<ResumeContext>(
            std::string{obj.at("step").as_string().c_str()},
            obj.at("state").as_object());
    }

    void
    write(ResumeContext ctx)
    {
        std::ofstream os(path_.string());
        if (os.good())
        {
            auto obj = boost::json::object{
                {"step", ctx.tag},
                {"state", ctx.data},
            };
            os << boost::json::serialize(obj) << std::endl;
        }
    }
};

class Step
{
    std::string tag_;
    std::function<void(
        std::string const&,  // tag
        boost::asio::yield_context,
        Backend::LedgerRange const&,
        boost::json::object)>
        worker_;

public:
    Step(std::string tag, auto&& fn)
        : tag_{std::move(tag)}, worker_{std::move(fn)}
    {
    }

    void
    perform(
        boost::asio::yield_context yield,
        Backend::LedgerRange const& ledgerRange,
        boost::json::object resume = {})
    {
        worker_(tag_, yield, ledgerRange, std::move(resume));
    }

    std::string
    tag() const
    {
        return tag_;
    }
};

class Migrator
{
    std::reference_wrapper<boost::asio::io_context> ioc_;
    std::reference_wrapper<clio::Config const> config_;
    std::shared_ptr<Backend::CassandraBackend> backend_;
    std::reference_wrapper<ResumeContextProvider> resumeProvider_;

    boost::asio::steady_timer timer_;
    std::vector<Step> steps_;

public:
    Migrator(
        boost::asio::io_context& ioc,
        clio::Config const& config,
        std::shared_ptr<Backend::CassandraBackend> backend,
        ResumeContextProvider& resumeProvider,
        std::vector<Step> steps)
        : ioc_{std::ref(ioc)}
        , config_{std::cref(config)}
        , backend_{backend}
        , resumeProvider_{std::ref(resumeProvider)}
        , timer_{ioc}
        , steps_{std::move(steps)}
    {
        boost::asio::spawn(
            ioc,
            [this, workGuard = boost::asio::make_work_guard(ioc)](auto yield) {
                run(yield);
            });
    }

private:
    void
    run(boost::asio::yield_context yield)
    {
        clio::LogService::info() << "Beginning migration";
        auto const ledgerRange = backend_->hardFetchLedgerRangeNoThrow(yield);

        /*
         * Step 0 - If we haven't downloaded the initial ledger yet, just short
         * circuit.
         */
        if (!ledgerRange)
        {
            clio::LogService::info() << "There is no data to migrate";
            return;
        }

        auto resume = resumeProvider_.get().load();

        for (auto& step : steps_)
        {
            if (resume)
            {
                if (resume->tag == step.tag())
                {
                    step.perform(
                        yield,
                        *ledgerRange,
                        resume->data);  // resume if possible
                }
                else
                {
                    clio::LogService::info() << "-- Skipping " << step.tag();
                    continue;  // skip this step
                }
            }
            else
            {
                step.perform(yield, *ledgerRange);  // start step from scratch
            }

            clio::LogService::info() << step.tag() << " done!\n";
            resume = std::nullopt;  // already used our resume state for
                                    // previous step
        }

        clio::LogService::info()
            << "Completed migration from " << ledgerRange->minSequence << " to "
            << ledgerRange->maxSequence << "!\n";
    }
};
