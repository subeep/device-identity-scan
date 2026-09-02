#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

namespace discan {

class OuiDatabase {
public:
    static OuiDatabase& instance() {
        static OuiDatabase inst;
        return inst;
    }

    // Lookup vendor by standard MAC string "AA:BB:CC:DD:EE:FF" or 3-byte prefix "AABBCC"
    std::string lookup_mac(const std::string& mac_str) const;
    
    // Lookup vendor by 24-bit integer (e.g. 0x001A11)
    std::string lookup_oui(uint32_t oui) const;

private:
    OuiDatabase();
    void load_builtin_ouis();

    std::unordered_map<uint32_t, std::string> oui_map_;
};

} // namespace discan
