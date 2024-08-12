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

#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <gtest/gtest.h>

using namespace util::config;

/**
 * @brief A mock ClioConfigDefinition for testing purposes.
 *
 * In the actual Clio configuration, arrays typically hold optional values, meaning users are not required to
 * provide values for them.
 *
 * For primitive types (i.e., single specific values), some are mandatory and must be explicitly defined in the
 * user's configuration file, including both the key and the corresponding value, while some are optional
 */

inline ClioConfigDefinition
generateConfig()
{
    return ClioConfigDefinition{
        {"header.text1", ConfigValue{ConfigType::String}.defaultValue("value")},
        {"header.port", ConfigValue{ConfigType::Integer}.defaultValue(123)},
        {"header.admin", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"header.sub.sub2Value", ConfigValue{ConfigType::String}.defaultValue("TSM")},
        {"ip", ConfigValue{ConfigType::Double}.defaultValue(444.22)},
        {"array.[].sub", Array{ConfigValue{ConfigType::Double}}},
        {"array.[].sub2", Array{ConfigValue{ConfigType::String}.optional()}},
        {"higher.[].low.section", Array{ConfigValue{ConfigType::String}.withConstraint(validateChannelName)}},
        {"higher.[].low.admin", Array{ConfigValue{ConfigType::Boolean}}},
        {"dosguard.whitelist.[]", Array{ConfigValue{ConfigType::String}.optional()}},
        {"dosguard.port", ConfigValue{ConfigType::Integer}.defaultValue(55555).withConstraint(validatePort)},
        {"optional.withDefault", ConfigValue{ConfigType::Double}.defaultValue(0.0).optional()},
        {"optional.withNoDefault", ConfigValue{ConfigType::Double}.optional()},
        {"requireValue", ConfigValue{ConfigType::String}}
    };
}

/* The config definition above would look like this structure in config.json
{
    "header": {
       "text1": "value",
       "port": 321,
       "admin": true,
       "sub": {
           "sub2Value": "TSM"
       }
     },
     "ip": 444.22,
     "array": [
       {
           "sub": //optional for user to include
           "sub2": //optional for user to include
       },
     ],
     "higher": [
       {
           "low": {
               "section": //optional for user to include
               "admin": //optional for user to include
           }
       }
     ],
     "dosguard":  {
        "whitelist": [
            // mandatory for user to include
        ],
        "port" : 55555
        },
    },
    "optional" : {
        "withDefault" : 0.0,
        "withNoDefault" :  //optional for user to include
        },
    "requireValue" : // value must be provided by user
    }
*/

/* Used to test overwriting default values in ClioConfigDefinition Above */
constexpr static auto JSONData = R"JSON(
    {
    "header": {
       "text1": "value",
       "port": 321,
       "admin": false,
       "sub": {
           "sub2Value": "TSM"
       }
     },
     "array": [
       {
           "sub": 111.11,
           "sub2": "subCategory"
       },
       {
           "sub": 4321.55,
           "sub2": "temporary"
       },
       {
           "sub": 5555.44,
           "sub2": "london"
       }
     ],
      "higher": [
       {
           "low": {
               "section": "WebServer",
               "admin": false
           }
       }
     ],
     "dosguard":  {
        "whitelist": [
            "125.5.5.1", "204.2.2.1"
        ],
        "port" : 44444
        },
    "optional" : {
        "withDefault" : 0.0
        },
    "requireValue" : "required"
    }
)JSON";

/* After parsing jsonValue and populating it into ClioConfig, It will look like this below in json format;
{
    "header": {
       "text1": "value",
       "port": 321,
       "admin": false,
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
       },
       {
           "sub": 5555.44,
           "sub2": "london"
       }
     ],
     "higher": [
       {
           "low": {
               "section": "WebServer",
               "admin": false
           }
       }
     ],
     "dosguard":  {
        "whitelist": [
            "125.5.5.1", "204.2.2.1"
        ],
        "port" : 44444
        }
    },
    "optional" : {
        "withDefault" : 0.0
        },
    "requireValue" : "required"
    }
*/

// Invalid Json key/values
constexpr static auto invalidJSONData = R"JSON(
{
    "header": {
        "port": "999"
    },
    "dosguard": {
        "whitelist": [
            false
        ]
    },
    "idk": true,
    "requireValue" : "required"
}
)JSON";
