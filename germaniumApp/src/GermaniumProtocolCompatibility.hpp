/**
 * @file GermaniumProtocolCompatibility.hpp
 * @brief ADGermanium release recommendations by detector protocol version.
 *
 * @author Ji Li <liji@bnl.gov>
 * @date 08/04/2026
 * 
 * @copyright
 * Copyright (c) 2026 Brookhaven National Laboratory
 * @license BSD 3-Clause License. See LICENSE file for details.
 */

//===========================================================================//

#pragma once

//===========================================================================//

#include <cstdint>

#include "GermaniumDetectorProtocol.hpp"

namespace GermaniumProtocolCompatibility
{

struct ProtocolRecommendation
{
    uint16_t major;
    uint16_t minor;
    const char* adGermaniumVersion;
};

static constexpr ProtocolRecommendation kProtocolRecommendations[] = { { GermaniumProtocol::PROTOCOL_MAJOR
                                                                       , GermaniumProtocol::PROTOCOL_MINOR
                                                                       , "ADGermanium built from this source tree (protocol 1.0)"
                                                                       },
                                                                     };

inline const char* recommendedAdGermaniumVersion(uint16_t major, uint16_t minor)
{
    for (const auto& recommendation : kProtocolRecommendations)
    {
        if (recommendation.major == major && recommendation.minor == minor)
            return recommendation.adGermaniumVersion;
    }
    return nullptr;
}

} // namespace GermaniumProtocolCompatibility
