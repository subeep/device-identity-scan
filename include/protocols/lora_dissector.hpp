#pragma once

#include "protocols/packet.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace discan {

class LoraDissector {
public:
    static bool dissect(Packet& pkt);

    static std::string format_eui(const uint8_t* bytes);
    static std::string format_devaddr(const uint8_t* bytes);
};

} // namespace discan
