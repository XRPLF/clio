#pragma once

#include <xrpl/protocol/jss.h>

/** @brief Helper macro for borrowing from xrpl::jss static (J)son (S)trings. */
#define JS(x) xrpl::jss::x.cStr()

/** @brief Access the lower case copy of a static (J)son (S)tring. */
#define JSL(x) util::toLower(JS(x))

/** @brief Provides access to (SF)ield name (S)trings. */
#define SFS(x) xrpl::x.jsonName.cStr()
