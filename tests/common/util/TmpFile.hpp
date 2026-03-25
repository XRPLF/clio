#pragma once

#include <fmt/format.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
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

    static TmpFile
    empty()
    {
        return TmpFile{""};
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
