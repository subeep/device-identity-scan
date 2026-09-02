#include "common/oui_database.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace discan {

OuiDatabase::OuiDatabase() {
    load_builtin_ouis();
}

std::string OuiDatabase::lookup_oui(uint32_t oui) const {
    auto it = oui_map_.find(oui & 0xFFFFFF);
    if (it != oui_map_.end()) {
        return it->second;
    }
    return "Unknown Vendor";
}

std::string OuiDatabase::lookup_mac(const std::string& mac_str) const {
    // Strip colons, dashes, dots
    std::string clean;
    for (char c : mac_str) {
        if (std::isxdigit(c)) {
            clean += std::toupper(c);
        }
    }

    if (clean.length() < 6) {
        return "Unknown";
    }

    try {
        uint32_t oui = std::stoul(clean.substr(0, 6), nullptr, 16);
        return lookup_oui(oui);
    } catch (...) {
        return "Unknown";
    }
}

void OuiDatabase::load_builtin_ouis() {
    // Major IoT, Wi-Fi, Bluetooth, Zigbee, LoRa vendors
    
    // Espressif Systems (ESP8266, ESP32, ESP32-C3, ESP32-S3)
    oui_map_[0x240AC4] = "Espressif Inc.";
    oui_map_[0x246F28] = "Espressif Inc.";
    oui_map_[0x24B2DE] = "Espressif Inc.";
    oui_map_[0x30AEA4] = "Espressif Inc.";
    oui_map_[0x3C6105] = "Espressif Inc.";
    oui_map_[0x485519] = "Espressif Inc.";
    oui_map_[0x5443B2] = "Espressif Inc.";
    oui_map_[0x68C63A] = "Espressif Inc.";
    oui_map_[0x70039F] = "Espressif Inc.";
    oui_map_[0x7C9EBD] = "Espressif Inc.";
    oui_map_[0x840D8E] = "Espressif Inc.";
    oui_map_[0x84F3EB] = "Espressif Inc.";
    oui_map_[0x9097D5] = "Espressif Inc.";
    oui_map_[0xA020A6] = "Espressif Inc.";
    oui_map_[0xAC67B2] = "Espressif Inc.";
    oui_map_[0xB4E62D] = "Espressif Inc.";
    oui_map_[0xBCDD9F] = "Espressif Inc.";
    oui_map_[0xC44F33] = "Espressif Inc.";
    oui_map_[0xC82B96] = "Espressif Inc.";
    oui_map_[0xCC50E3] = "Espressif Inc.";
    oui_map_[0xD8A01D] = "Espressif Inc.";
    oui_map_[0xDC4F22] = "Espressif Inc.";
    oui_map_[0xE09806] = "Espressif Inc.";
    oui_map_[0xE831CD] = "Espressif Inc.";
    oui_map_[0xEC94CB] = "Espressif Inc.";

    // Nordic Semiconductor (nRF51, nRF52, nRF53, nRF54 BLE/Zigbee/Thread)
    oui_map_[0x001304] = "Nordic Semiconductor";
    oui_map_[0xD88039] = "Nordic Semiconductor";
    oui_map_[0xF4CE36] = "Nordic Semiconductor";
    oui_map_[0xEC1BBD] = "Nordic Semiconductor";
    oui_map_[0xC098E5] = "Nordic Semiconductor";

    // Texas Instruments (CC2530, CC2538, CC2652 Zigbee/BLE, CC1352 LoRa/Sub-1G)
    oui_map_[0x00124B] = "Texas Instruments";
    oui_map_[0x0017EA] = "Texas Instruments";
    oui_map_[0x0021AC] = "Texas Instruments";
    oui_map_[0x0024B2] = "Texas Instruments";
    oui_map_[0x40ECF8] = "Texas Instruments";
    oui_map_[0x546C0E] = "Texas Instruments";
    oui_map_[0x68C90B] = "Texas Instruments";
    oui_map_[0x883314] = "Texas Instruments";
    oui_map_[0xB09122] = "Texas Instruments";
    oui_map_[0xF4B85E] = "Texas Instruments";

    // Silicon Labs (EFR32, EM35x Zigbee/BLE/Thread)
    oui_map_[0x000B57] = "Silicon Laboratories";
    oui_map_[0x002446] = "Silicon Laboratories";
    oui_map_[0x588E81] = "Silicon Laboratories";
    oui_map_[0x60A423] = "Silicon Laboratories";
    oui_map_[0x841826] = "Silicon Laboratories";
    oui_map_[0x90FD9F] = "Silicon Laboratories";
    oui_map_[0xCC86EC] = "Silicon Laboratories";

    // Semtech (LoRa transceivers SX1276, SX1262, LR1110)
    oui_map_[0x0016B6] = "Semtech Corporation";
    oui_map_[0x0025CA] = "Semtech Corporation";
    oui_map_[0x70B3D5] = "IEEE Registration (LoRaWAN EUI)";

    // Philips Lighting / Signify (Hue Zigbee Bulbs & Bridges)
    oui_map_[0x001788] = "Signify (Philips Hue)";
    oui_map_[0xECB5FA] = "Signify (Philips Hue)";
    oui_map_[0x001A80] = "Signify (Philips Hue)";

    // Apple Inc. (iPhone, iPad, Mac, Apple Watch, AirTag, AirPods)
    oui_map_[0x0017F2] = "Apple Inc.";
    oui_map_[0x0019E3] = "Apple Inc.";
    oui_map_[0x001C42] = "Apple Inc.";
    oui_map_[0x001E52] = "Apple Inc.";
    oui_map_[0x002312] = "Apple Inc.";
    oui_map_[0x002500] = "Apple Inc.";
    oui_map_[0x0026BB] = "Apple Inc.";
    oui_map_[0x003EE1] = "Apple Inc.";
    oui_map_[0x006171] = "Apple Inc.";
    oui_map_[0x040C56] = "Apple Inc.";
    oui_map_[0x041552] = "Apple Inc.";
    oui_map_[0x042665] = "Apple Inc.";
    oui_map_[0x044BEE] = "Apple Inc.";
    oui_map_[0x086698] = "Apple Inc.";
    oui_map_[0x087045] = "Apple Inc.";
    oui_map_[0x0C74C2] = "Apple Inc.";
    oui_map_[0x1094BB] = "Apple Inc.";
    oui_map_[0x147DDA] = "Apple Inc.";
    oui_map_[0x18AF61] = "Apple Inc.";
    oui_map_[0x1C1AC0] = "Apple Inc.";
    oui_map_[0x20768F] = "Apple Inc.";
    oui_map_[0x286A8A] = "Apple Inc.";
    oui_map_[0x38CA84] = "Apple Inc.";
    oui_map_[0x406C8F] = "Apple Inc.";
    oui_map_[0x484B68] = "Apple Inc.";
    oui_map_[0x50ED3C] = "Apple Inc.";
    oui_map_[0x5C80B6] = "Apple Inc.";
    oui_map_[0x68545A] = "Apple Inc.";
    oui_map_[0x703EAC] = "Apple Inc.";
    oui_map_[0x787B8A] = "Apple Inc.";
    oui_map_[0x88665A] = "Apple Inc.";
    oui_map_[0x907240] = "Apple Inc.";
    oui_map_[0xA483E7] = "Apple Inc.";
    oui_map_[0xAC87A3] = "Apple Inc.";
    oui_map_[0xBC5436] = "Apple Inc.";
    oui_map_[0xC869CD] = "Apple Inc.";
    oui_map_[0xDC56E7] = "Apple Inc.";
    oui_map_[0xF01898] = "Apple Inc.";
    oui_map_[0xF4F951] = "Apple Inc.";

    // Intel Corporation
    oui_map_[0x0002B3] = "Intel Corporation";
    oui_map_[0x000E0C] = "Intel Corporation";
    oui_map_[0x001302] = "Intel Corporation";
    oui_map_[0x001500] = "Intel Corporation";
    oui_map_[0x001B77] = "Intel Corporation";
    oui_map_[0x00216A] = "Intel Corporation";
    oui_map_[0x3413E8] = "Intel Corporation";
    oui_map_[0x4851B7] = "Intel Corporation";
    oui_map_[0x6805CA] = "Intel Corporation";
    oui_map_[0x848506] = "Intel Corporation";
    oui_map_[0x98AF65] = "Intel Corporation";
    oui_map_[0xA0510B] = "Intel Corporation";

    // Raspberry Pi Foundation
    oui_map_[0xB827EB] = "Raspberry Pi Foundation";
    oui_map_[0xDC2632] = "Raspberry Pi Foundation";
    oui_map_[0xE45F01] = "Raspberry Pi Foundation";
    oui_map_[0x28CDC1] = "Raspberry Pi Foundation";

    // Google LLC
    oui_map_[0x001A11] = "Google LLC";
    oui_map_[0x3C5AB4] = "Google LLC";
    oui_map_[0x546009] = "Google LLC";
    oui_map_[0x94EB2C] = "Google LLC";
    oui_map_[0xD86C63] = "Google LLC";
    oui_map_[0xF4F5D8] = "Google LLC";

    // Amazon Technologies (Echo, Ring, Kindle, Blink, Sidewalk)
    oui_map_[0x00FC8B] = "Amazon Technologies";
    oui_map_[0x380146] = "Amazon Technologies";
    oui_map_[0x44650D] = "Amazon Technologies";
    oui_map_[0x50F5DA] = "Amazon Technologies";
    oui_map_[0x6837E9] = "Amazon Technologies";
    oui_map_[0x747548] = "Amazon Technologies";
    oui_map_[0xAC63BE] = "Amazon Technologies";
    oui_map_[0xF0F002] = "Amazon Technologies";

    // Tuya Smart (IoT / Smart Home / Zigbee / Wi-Fi)
    oui_map_[0x105A17] = "Tuya Smart Inc.";
    oui_map_[0x508A06] = "Tuya Smart Inc.";
    oui_map_[0x68572D] = "Tuya Smart Inc.";
    oui_map_[0x7CDA85] = "Tuya Smart Inc.";
    oui_map_[0x84F703] = "Tuya Smart Inc.";
    oui_map_[0xD81F12] = "Tuya Smart Inc.";

    // Xiaomi Communications (Smartphones, Aqara Zigbee, Smart Home)
    oui_map_[0x04CF8C] = "Xiaomi Communications";
    oui_map_[0x14F65A] = "Xiaomi Communications";
    oui_map_[0x286C07] = "Xiaomi Communications";
    oui_map_[0x34800D] = "Xiaomi Communications";
    oui_map_[0x584498] = "Xiaomi Communications";
    oui_map_[0x640980] = "Xiaomi Communications";
    oui_map_[0x7811DC] = "Xiaomi Communications";
    oui_map_[0xACF7F3] = "Xiaomi Communications";
    oui_map_[0xC81479] = "Xiaomi Communications";

    // Samsung Electronics
    oui_map_[0x0007AB] = "Samsung Electronics";
    oui_map_[0x001247] = "Samsung Electronics";
    oui_map_[0x001632] = "Samsung Electronics";
    oui_map_[0x002119] = "Samsung Electronics";
    oui_map_[0x08EE8B] = "Samsung Electronics";
    oui_map_[0x1489FD] = "Samsung Electronics";
    oui_map_[0x244B03] = "Samsung Electronics";
    oui_map_[0x30074D] = "Samsung Electronics";
    oui_map_[0x380A94] = "Samsung Electronics";
    oui_map_[0x5001D9] = "Samsung Electronics";
    oui_map_[0x606C66] = "Samsung Electronics";
    oui_map_[0x784F43] = "Samsung Electronics";
    oui_map_[0x90F1AA] = "Samsung Electronics";
    oui_map_[0xBC4486] = "Samsung Electronics";
    oui_map_[0xC4731E] = "Samsung Electronics";

    // Cisco Systems / Linksys
    oui_map_[0x00000C] = "Cisco Systems";
    oui_map_[0x000142] = "Cisco Systems";
    oui_map_[0x000652] = "Cisco Systems";
    oui_map_[0x001469] = "Cisco Systems";
    oui_map_[0x002414] = "Cisco Systems";
    oui_map_[0x00260B] = "Cisco Systems";
    oui_map_[0x58F39C] = "Cisco Systems";
    oui_map_[0x708105] = "Cisco Systems";

    // TP-Link Technologies
    oui_map_[0x001D0F] = "TP-Link Technologies";
    oui_map_[0x14CC20] = "TP-Link Technologies";
    oui_map_[0x30B5C2] = "TP-Link Technologies";
    oui_map_[0x50C7BF] = "TP-Link Technologies";
    oui_map_[0x60E327] = "TP-Link Technologies";
    oui_map_[0x98DED0] = "TP-Link Technologies";
    oui_map_[0xC006C3] = "TP-Link Technologies";
    oui_map_[0xE848B8] = "TP-Link Technologies";

    // Netgear Inc.
    oui_map_[0x00095B] = "Netgear Inc.";
    oui_map_[0x00146C] = "Netgear Inc.";
    oui_map_[0x001E2A] = "Netgear Inc.";
    oui_map_[0x204E7F] = "Netgear Inc.";
    oui_map_[0x28C68E] = "Netgear Inc.";
    oui_map_[0xA040A0] = "Netgear Inc.";
    oui_map_[0xE0469A] = "Netgear Inc.";

    // Microchip Technology / Atmel (Zigbee, LoRa SAMR34, Wi-Fi ATWINC)
    oui_map_[0x000425] = "Microchip Technology";
    oui_map_[0x0004A3] = "Microchip Technology";
    oui_map_[0x001EC0] = "Microchip Technology";
    oui_map_[0xFC0FE7] = "Microchip Technology";

    // Ubiquiti Networks (UniFi APs, EdgeMAX)
    oui_map_[0x002722] = "Ubiquiti Networks";
    oui_map_[0x24A43C] = "Ubiquiti Networks";
    oui_map_[0x44D9E7] = "Ubiquiti Networks";
    oui_map_[0x68D79A] = "Ubiquiti Networks";
    oui_map_[0x7483C2] = "Ubiquiti Networks";
    oui_map_[0x788A20] = "Ubiquiti Networks";
    oui_map_[0xAC8BA9] = "Ubiquiti Networks";
    oui_map_[0xB4FBE4] = "Ubiquiti Networks";
    oui_map_[0xD8B370] = "Ubiquiti Networks";
    oui_map_[0xFCECDA] = "Ubiquiti Networks";

    // Sony Interactive Entertainment
    oui_map_[0x00041F] = "Sony Interactive Ent.";
    oui_map_[0x001315] = "Sony Interactive Ent.";
    oui_map_[0x0019C5] = "Sony Interactive Ent.";
    oui_map_[0x001D0D] = "Sony Interactive Ent.";
    oui_map_[0x00248D] = "Sony Interactive Ent.";
    oui_map_[0x280D5C] = "Sony Interactive Ent.";
    oui_map_[0x709E29] = "Sony Interactive Ent.";
    oui_map_[0xAC9B0A] = "Sony Interactive Ent.";
    oui_map_[0xD86B7D] = "Sony Interactive Ent.";
    oui_map_[0xFC0F4B] = "Sony Interactive Ent.";
}

} // namespace discan
