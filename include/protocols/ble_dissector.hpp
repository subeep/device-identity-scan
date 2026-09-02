#pragma once

#include "protocols/packet.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace discan {

class BleDissector {
public:
    static bool dissect(Packet& pkt);

    static std::string format_mac(const uint8_t* bytes);
    static void parse_adv_data(const uint8_t* data, size_t len, DissectedField& adv_tree, BleMetadata& meta);
    static void parse_manufacturer_data(const uint8_t* data, size_t len, DissectedField& mfg_field, BleMetadata& meta);
};

} // namespace discan
