#pragma once

#include <xrpl/protocol/jss.h>

/** @brief Helper macro for borrowing from ripple::jss static (J)son (S)trings. */
#define JS(x) ripple::jss::x.c_str()

/** @brief Access the lower case copy of a static (J)son (S)tring. */
#define JSL(x) util::toLower(JS(x))

/** @brief Provides access to (SF)ield name (S)trings. */
#define SFS(x) ripple::x.jsonName.c_str()
