#pragma once

#include "protocols/packet.hpp"
#include <cstdint>
#include <vector>

namespace discan {

class WifiDissector {
public:
    static bool dissect(Packet& pkt);

    // Helpers to parse specific components
    static void parse_frame_control(uint16_t fc, DissectedField& fc_field, WifiMetadata& meta);
    static void parse_tagged_parameters(const uint8_t* data, size_t len, DissectedField& ies_field, WifiMetadata& meta);
    static std::string format_mac(const uint8_t* bytes);
};

} // namespace discan
