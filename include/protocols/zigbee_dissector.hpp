#pragma once

#include "protocols/packet.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace discan {

class ZigbeeDissector {
public:
    static bool dissect(Packet& pkt);

    static std::string format_ext_addr(const uint8_t* bytes);
    static std::string format_short_addr(uint16_t addr);
};

} // namespace discan
