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

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <utility>

struct TmpFile {
    std::string path;

    TmpFile(std::string_view content) : path{std::tmpnam(nullptr)}
    {
        std::ofstream ofs;
        ofs.open(path, std::ios::out);
        ofs << content;
    }

    TmpFile(TmpFile const&) = delete;
    TmpFile(TmpFile&& other) : path{std::move(other.path)}
    {
        other.path.clear();
    }
    TmpFile&
    operator=(TmpFile const&) = delete;
    TmpFile&

    operator=(TmpFile&& other)
    {
        if (this != &other)
            *this = std::move(other);
        return *this;
    }

    ~TmpFile()
    {
        if (not path.empty())
            std::filesystem::remove(path);
    }
};
