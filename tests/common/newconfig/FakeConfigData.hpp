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

#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <gtest/gtest.h>

using namespace util::config;

inline ClioConfigDefinition
generateConfig()
{
    return ClioConfigDefinition{
        {"header.text1", ConfigValue{ConfigType::String}.defaultValue("value")},
        {"header.port", ConfigValue{ConfigType::Integer}.defaultValue(123)},
        {"header.admin", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"header.sub.sub2Value", ConfigValue{ConfigType::String}.defaultValue("TSM")},
        {"ip", ConfigValue{ConfigType::Double}.defaultValue(444.22)},
        {"array.[].sub",
         Array{
             ConfigValue{ConfigType::Double}.defaultValue(111.11), ConfigValue{ConfigType::Double}.defaultValue(4321.55)
         }},
        {"array.[].sub2",
         Array{
             ConfigValue{ConfigType::String}.defaultValue("subCategory"),
             ConfigValue{ConfigType::String}.defaultValue("temporary")
         }},
        {"higher.[].low.section", Array{ConfigValue{ConfigType::String}.defaultValue("true")}},
        {"higher.[].low.admin", Array{ConfigValue{ConfigType::Boolean}.defaultValue(false)}}
    };
}

/* The config definition above would look like this structure in config.json:
 "header": {
    "text1": "value",
    "port": 123,
    "admin": true,
    "sub": {
        "sub2Value": "TSM"
    }
  },
  "ip": 444.22,
  "array": [
    {
        "sub": 111.11,
        "sub2": "subCategory"
    },
    {
        "sub": 4321.55,
        "sub2": "temporary"
    }
  ]
  "higher": [
    {
        "low": {
            "section": "true",
            "admin": false
        }
    }
  ]
 */
