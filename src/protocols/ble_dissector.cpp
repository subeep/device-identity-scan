#include "protocols/ble_dissector.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace discan {

std::string BleDissector::format_mac(const uint8_t* bytes) {
    std::ostringstream ss;
    for (int i = 5; i >= 0; --i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
        if (i > 0) ss << ":";
    }
    return ss.str();
}

bool BleDissector::dissect(Packet& pkt) {
    if (pkt.raw_data.size() < 6) {
        return false;
    }

    pkt.protocol = ProtocolType::BLUETOOTH;
    pkt.dissection_tree.clear();

    const uint8_t* buf = pkt.raw_data.data();
    size_t len = pkt.raw_data.size();
    size_t offset = 0;

    // 1. Physical Layer Preamble & Access Address
    DissectedField phy_field;
    phy_field.name = "Bluetooth Low Energy Physical Layer (GFSK 1 Mbps)";
    phy_field.is_unencrypted = true;

    DissectedField preamble_field;
    preamble_field.name = "Preamble (1 octet)";
    preamble_field.value = "0xAA (Alternating bit pattern 10101010)";
    phy_field.subfields.push_back(preamble_field);

    uint32_t access_addr = 0x8E89BED6;
    if (len >= 4 && offset + 4 <= len) {
        access_addr = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    }
    pkt.ble.access_address = access_addr;

    DissectedField aa_field;
    aa_field.name = "Access Address (4 octets)";
    std::ostringstream aa_ss;
    aa_ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << access_addr;
    if (access_addr == 0x8E89BED6) {
        aa_ss << " (Advertising Channel Access Address)";
    } else {
        aa_ss << " (Data Channel Connection Access Address)";
    }
    aa_field.value = aa_ss.str();
    phy_field.subfields.push_back(aa_field);

    pkt.dissection_tree.push_back(phy_field);

    if (access_addr == 0x8E89BED6 && len >= 8) {
        offset += 4;
    }

    // 2. Link Layer Advertising PDU Header (2 bytes)
    if (len - offset < 2) return true;

    uint8_t header_byte0 = buf[offset];
    uint8_t header_byte1 = buf[offset + 1];
    offset += 2;

    uint8_t pdu_type = header_byte0 & 0x0F;
    bool tx_add = (header_byte0 & 0x40) != 0;
    bool rx_add = (header_byte0 & 0x80) != 0;
    uint8_t payload_len = header_byte1 & 0x3F;

    if (pdu_type > 7) {
        return false;
    }

    pkt.ble.pdu_type = pdu_type;
    pkt.ble.tx_add = tx_add;
    pkt.ble.rx_add = rx_add;

    std::string pdu_name;
    switch (pdu_type) {
        case 0x0: pdu_name = "ADV_IND (Connectable Undirected)"; break;
        case 0x1: pdu_name = "ADV_DIRECT_IND (Connectable Directed)"; break;
        case 0x2: pdu_name = "ADV_NONCONN_IND (Non-connectable Undirected)"; break;
        case 0x3: pdu_name = "SCAN_REQ (Scan Request)"; break;
        case 0x4: pdu_name = "SCAN_RSP (Scan Response)"; break;
        case 0x5: pdu_name = "CONNECT_IND (Connect Request)"; break;
        case 0x6: pdu_name = "ADV_SCAN_IND (Scannable Undirected)"; break;
        case 0x7: pdu_name = "ADV_EXT_IND (Extended Advertising)"; break;
        default: pdu_name = "Reserved PDU"; break;
    }
    pkt.ble.pdu_type_name = pdu_name;
    pkt.protocol_subtype = pdu_name;

    DissectedField pdu_header;
    pdu_header.name = "Link Layer PDU Header: " + pdu_name;
    pdu_header.is_unencrypted = true;

    pdu_header.subfields.push_back({"PDU Type", pdu_name + " (0x" + std::to_string(pdu_type) + ")"});
    pdu_header.subfields.push_back({"TxAdd (Address Type)", tx_add ? "Random Device Address (1)" : "Public Device Address (0)"});
    pdu_header.subfields.push_back({"RxAdd", rx_add ? "Random (1)" : "Public (0)"});
    pdu_header.subfields.push_back({"Length (octets)", std::to_string(payload_len) + " bytes"});

    pkt.dissection_tree.push_back(pdu_header);

    // 3. Payload Extraction (MAC Addresses & AD Structures)
    if (len - offset < 6) return true;

    uint8_t msb_addr = buf[offset + 5];
    std::string addr_type_str = "Public";
    if (tx_add) {
        uint8_t top2 = (msb_addr >> 6) & 0x03;
        if (top2 == 0x01) addr_type_str = "Random Resolvable (RPA)";
        else if (top2 == 0x03) addr_type_str = "Static Random";
        else addr_type_str = "Non-Resolvable Random";
    }

    // A. SCAN_REQ / CONNECT_IND
    if (pdu_type == 0x3 || pdu_type == 0x5) {
        std::string init_mac = format_mac(&buf[offset]);
        pkt.source_address = init_mac;
        pkt.ble.advertiser_mac = init_mac;

        DissectedField mac_field;
        mac_field.name = (pdu_type == 0x3 ? "Scanner Address (ScanA)" : "Initiator Address (InitA)");
        mac_field.value = init_mac + " [" + addr_type_str + "]";
        mac_field.is_unencrypted = true;
        pkt.dissection_tree.push_back(mac_field);

        if (len - offset >= 12) {
            std::string target_mac = format_mac(&buf[offset + 6]);
            pkt.destination_address = target_mac;

            DissectedField adv_field;
            adv_field.name = "Advertiser Target Address (AdvA)";
            adv_field.value = target_mac + (rx_add ? " [Random]" : " [Public]");
            adv_field.is_unencrypted = true;
            pkt.dissection_tree.push_back(adv_field);
        }
        return true;
    }

    // B. Advertising PDUs & SCAN_RSP (PDU Type 0x4)
    std::string adv_mac = format_mac(&buf[offset]);
    pkt.source_address = adv_mac;
    pkt.ble.advertiser_mac = adv_mac;
    offset += 6;

    DissectedField adv_mac_field;
    adv_mac_field.name = (pdu_type == 0x4 ? "Scan Response Address (AdvA)" : "Advertiser Address (AdvA)");
    adv_mac_field.value = adv_mac + " [" + addr_type_str + "]";
    adv_mac_field.is_unencrypted = true;
    pkt.dissection_tree.push_back(adv_mac_field);

    // 4. Advertising Data Structures (AD Structures)
    DissectedField ad_tree;
    ad_tree.name = (pdu_type == 0x4 ? "Scan Response (ScanRspData) Structures" : "Advertising Data (AdvData) Structures");
    ad_tree.is_unencrypted = true;

    size_t ad_offset = offset;
    while (ad_offset < len) {
        uint8_t ad_len = buf[ad_offset];
        if (ad_len == 0 || ad_offset + 1 + ad_len > len) {
            break;
        }

        uint8_t ad_type = buf[ad_offset + 1];
        const uint8_t* ad_data = &buf[ad_offset + 2];
        size_t data_len = ad_len - 1;

        DissectedField ad_item;

        switch (ad_type) {
            case 0x01: { // Flags
                uint8_t flags = ad_data[0];
                ad_item.name = "AD Type 0x01: Flags";
                std::ostringstream f_ss;
                f_ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(flags) << " (";
                if (flags & 0x01) f_ss << "LE Limited Discoverable, ";
                if (flags & 0x02) f_ss << "LE General Discoverable, ";
                if (flags & 0x04) f_ss << "BR/EDR Not Supported";
                f_ss << ")";
                ad_item.value = f_ss.str();
                break;
            }

            case 0x08:   // Shortened Local Name
            case 0x09: { // Complete Local Name
                std::string name;
                for (size_t i = 0; i < data_len; ++i) {
                    if (ad_data[i] >= 32 && ad_data[i] <= 126) {
                        name += static_cast<char>(ad_data[i]);
                    }
                }
                pkt.ble.complete_local_name = name;
                ad_item.name = (ad_type == 0x09 ? "AD Type 0x09: Complete Local Name" : "AD Type 0x08: Shortened Local Name");
                ad_item.value = "\"" + name + "\"";
                break;
            }

            case 0x0A: { // TX Power Level
                int8_t tx_power = static_cast<int8_t>(ad_data[0]);
                pkt.ble.tx_power_level = tx_power;
                ad_item.name = "AD Type 0x0A: TX Power Level";
                ad_item.value = std::to_string(tx_power) + " dBm";
                break;
            }

            case 0x02:
            case 0x03:
            case 0x16: { // 16-bit Service UUIDs
                ad_item.name = "AD Type 0x03: 16-bit Service UUIDs";
                std::ostringstream u_ss;
                for (size_t i = 0; i + 1 < data_len; i += 2) {
                    uint16_t uuid = ad_data[i] | (ad_data[i + 1] << 8);
                    u_ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << uuid;
                    
                    if (uuid == 0xFE9F) {
                        u_ss << " (Google Fast Pair)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "Google Fast Pair Accessory";
                        if (pkt.ble.manufacturer_name.empty()) pkt.ble.manufacturer_name = "Google LLC";
                    } else if (uuid == 0xFE95) {
                        u_ss << " (Xiaomi / Huami Profile)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "Xiaomi Mi Smart Band / Sensor";
                        if (pkt.ble.manufacturer_name.empty()) pkt.ble.manufacturer_name = "Xiaomi Inc.";
                    } else if (uuid == 0xFEAA) {
                        u_ss << " (Google Eddystone Beacon)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "Google Eddystone Beacon";
                    } else if (uuid == 0xFD6F) {
                        u_ss << " (Exposure Notification)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "COVID Exposure Notification Device";
                    } else if (uuid == 0xFEED) {
                        u_ss << " (Tile Tracker)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "Tile Asset Tracker";
                        if (pkt.ble.manufacturer_name.empty()) pkt.ble.manufacturer_name = "Tile Inc.";
                    } else if (uuid == 0xFD50) {
                        u_ss << " (Tuya Smart IoT)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "Tuya Smart Home IoT Device";
                        if (pkt.ble.manufacturer_name.empty()) pkt.ble.manufacturer_name = "Tuya Smart";
                    } else if (uuid == 0x1812) {
                        u_ss << " (HID Wireless Keyboard/Mouse)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "BLE Wireless Mouse/Keyboard";
                    } else if (uuid == 0x180D) {
                        u_ss << " (Heart Rate Service)";
                        if (pkt.ble.complete_local_name.empty()) pkt.ble.complete_local_name = "BLE Heart Rate Monitor";
                    }
                    if (i + 2 < data_len) u_ss << ", ";
                }
                ad_item.value = u_ss.str();
                break;
            }

            case 0xFF: { // Manufacturer Specific Data
                ad_item.name = "AD Type 0xFF: Manufacturer Specific Data";
                if (data_len >= 2) {
                    uint16_t company_id = ad_data[0] | (ad_data[1] << 8);
                    std::string company_name;
                    switch (company_id) {
                        case 0x004C: company_name = "Apple Inc."; break;
                        case 0x0006: company_name = "Microsoft"; break;
                        case 0x0075: company_name = "Samsung Electronics"; break;
                        case 0x00E0: company_name = "Google LLC"; break;
                        case 0x0059: company_name = "Nordic Semiconductor"; break;
                        case 0x02E5: company_name = "Espressif Systems"; break;
                        case 0x000A: company_name = "Qualcomm Technologies"; break;
                        case 0x0087: company_name = "Garmin International"; break;
                        case 0x000D: company_name = "Texas Instruments"; break;
                        case 0x0157: company_name = "Anker Innovations"; break;
                        case 0x038F: company_name = "Xiaomi Inc."; break;
                        case 0x004B: company_name = "Sony Corporation"; break;
                        case 0x009E: company_name = "Bose Corporation"; break;
                        case 0x0171: company_name = "Amazon Technologies"; break;
                        case 0x01B5: company_name = "Fitbit Inc."; break;
                        case 0x0822: company_name = "Tuya Smart"; break;
                        case 0x00D2: company_name = "Dialog Semiconductor"; break;
                        case 0x0131: company_name = "Cypress Semiconductor"; break;
                        case 0x0060: company_name = "Realtek Semiconductor"; break;
                        case 0x0002: company_name = "Intel Corporation"; break;
                        case 0x027D: company_name = "Huawei Technologies"; break;
                        default: {
                            std::ostringstream c_ss;
                            c_ss << "Vendor 0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << company_id;
                            company_name = c_ss.str();
                            break;
                        }
                    }
                    pkt.ble.manufacturer_name = company_name;

                    // Deep Apple Payload & Model Dissection
                    if (company_id == 0x004C && data_len >= 3) {
                        uint8_t apple_type = ad_data[2];
                        if (apple_type == 0x02 && data_len >= 23) { // iBeacon
                            pkt.ble.is_ibeacon = true;
                            std::ostringstream uuid_ss;
                            for (int i = 0; i < 16; ++i) {
                                uuid_ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ad_data[4 + i]);
                                if (i == 3 || i == 5 || i == 7 || i == 9) uuid_ss << "-";
                            }
                            pkt.ble.ibeacon_uuid = uuid_ss.str();
                            pkt.ble.ibeacon_major = (ad_data[20] << 8) | ad_data[21];
                            pkt.ble.ibeacon_minor = (ad_data[22] << 8) | ad_data[23];
                            pkt.ble.ibeacon_tx_power = static_cast<int8_t>(ad_data[24]);

                            std::ostringstream ib_ss;
                            ib_ss << company_name << " iBeacon [Major: " << pkt.ble.ibeacon_major
                                  << ", Minor: " << pkt.ble.ibeacon_minor
                                  << ", Calibrated TX: " << static_cast<int>(pkt.ble.ibeacon_tx_power) << " dBm]";
                            ad_item.value = ib_ss.str();

                            if (pkt.ble.complete_local_name.empty()) {
                                pkt.ble.complete_local_name = "Apple iBeacon (" + std::to_string(pkt.ble.ibeacon_major) + "/" + std::to_string(pkt.ble.ibeacon_minor) + ")";
                            }
                        } else if (apple_type == 0x12 || apple_type == 0x07 || apple_type == 0x10) {
                            ad_item.value = company_name + " (Apple AirTag / FindMy Network Tracker)";
                            if (pkt.ble.complete_local_name.empty()) {
                                pkt.ble.complete_local_name = "Apple AirTag / FindMy Tracker";
                            }
                        } else if (apple_type == 0x05) { // AirPods Proximity
                            uint16_t model_id = (data_len >= 5) ? ((ad_data[3] << 8) | ad_data[4]) : 0;
                            std::string airpods_model = "Apple AirPods";
                            if (model_id == 0x0220 || model_id == 0x0F20) airpods_model = "Apple AirPods Pro (2nd Gen)";
                            else if (model_id == 0x0E20) airpods_model = "Apple AirPods Pro (1st Gen)";
                            else if (model_id == 0x0A20) airpods_model = "Apple AirPods Max";
                            else if (model_id == 0x1320) airpods_model = "Apple AirPods (3rd Gen)";
                            else if (model_id == 0x0520) airpods_model = "Apple Powerbeats Pro";

                            ad_item.value = company_name + " (" + airpods_model + ")";
                            if (pkt.ble.complete_local_name.empty()) {
                                pkt.ble.complete_local_name = airpods_model;
                            }
                        } else {
                            ad_item.value = company_name + " (Nearby Continuity Type 0x" + std::to_string(apple_type) + ")";
                            if (pkt.ble.complete_local_name.empty()) {
                                pkt.ble.complete_local_name = "Apple iPhone / iOS Device";
                            }
                        }
                    } else if (company_id == 0x0075) { // Samsung
                        ad_item.value = company_name + " (Galaxy SmartThings / Wearable)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "Samsung Galaxy Device";
                        }
                    } else if (company_id == 0x0006) { // Microsoft
                        ad_item.value = company_name + " (Swift Pair / Surface Accessory)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "Microsoft Surface / PC";
                        }
                    } else if (company_id == 0x02E5) { // Espressif
                        ad_item.value = company_name + " (ESP32 Bluetooth Mesh / Sensor)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "ESP32 IoT Sensor Node";
                        }
                    } else if (company_id == 0x0059) { // Nordic
                        ad_item.value = company_name + " (Nordic Semiconductor SoC)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "Nordic BLE Peripheral";
                        }
                    } else if (company_id == 0x0087) { // Garmin
                        ad_item.value = company_name + " (Garmin Smartwatch / Fitness)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "Garmin Smartwatch";
                        }
                    } else if (company_id == 0x038F) { // Xiaomi
                        ad_item.value = company_name + " (Xiaomi Smart Device)";
                        if (pkt.ble.complete_local_name.empty()) {
                            pkt.ble.complete_local_name = "Xiaomi Mi Smart Band";
                        }
                    } else {
                        ad_item.value = company_name;
                        if (pkt.ble.complete_local_name.empty() && !company_name.empty()) {
                            pkt.ble.complete_local_name = company_name + " Accessory";
                        }
                    }
                } else {
                    ad_item.value = "Raw Manufacturer Data (" + std::to_string(data_len) + " bytes)";
                }
                break;
            }

            default: {
                std::ostringstream unk_ss;
                unk_ss << "AD Type 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ad_type);
                ad_item.name = unk_ss.str();
                ad_item.value = std::to_string(data_len) + " bytes";
                break;
            }
        }

        ad_tree.subfields.push_back(ad_item);
        ad_offset += (1 + ad_len);
    }

    // Only set friendly fallback name if manufacturer was identified
    if (pkt.ble.complete_local_name.empty() && !pkt.ble.manufacturer_name.empty()) {
        pkt.ble.complete_local_name = pkt.ble.manufacturer_name + " Accessory";
    }

    if (!ad_tree.subfields.empty()) {
        pkt.dissection_tree.push_back(ad_tree);
    }

    return true;
}

} // namespace discan
