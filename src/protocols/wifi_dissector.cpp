#include "protocols/wifi_dissector.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace discan {

std::string WifiDissector::format_mac(const uint8_t* bytes) {
    std::ostringstream ss;
    for (int i = 0; i < 6; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
        if (i < 5) ss << ":";
    }
    return ss.str();
}

bool WifiDissector::dissect(Packet& pkt) {
    if (pkt.raw_data.size() < 10) {
        return false;
    }

    pkt.protocol = ProtocolType::WIFI;
    pkt.dissection_tree.clear();

    const uint8_t* buf = pkt.raw_data.data();
    size_t len = pkt.raw_data.size();
    size_t offset = 0;

    // 1. Physical Layer / Radiotap Preamble Field
    DissectedField phy_field;
    phy_field.name = "IEEE 802.11 Physical Layer & Preamble";
    phy_field.value = "OFDM / DSSS PHY Header";
    phy_field.is_unencrypted = true;
    
    DissectedField preamble_sync;
    preamble_sync.name = "Preamble Sync Pattern";
    preamble_sync.value = "10 Short Training Symbols (STS) + 2 Long Training Symbols (LTS)";
    phy_field.subfields.push_back(preamble_sync);

    DissectedField signal_field;
    signal_field.name = "Signal Field (SIG)";
    signal_field.value = "Rate: 54 Mbps | Mod: 64-QAM | Coding: 3/4 | Length: " + std::to_string(len) + " octets";
    phy_field.subfields.push_back(signal_field);

    DissectedField rssi_field;
    rssi_field.name = "Antenna Signal Quality";
    std::ostringstream rssi_ss;
    rssi_ss << std::fixed << std::setprecision(1) << pkt.rssi_dbm << " dBm";
    rssi_field.value = rssi_ss.str();
    phy_field.subfields.push_back(rssi_field);

    pkt.dissection_tree.push_back(phy_field);

    // 2. IEEE 802.11 MAC Header
    if (len - offset < 24) {
        return true;
    }

    DissectedField mac_header;
    mac_header.name = "IEEE 802.11 MAC Header";
    mac_header.is_unencrypted = true;

    // Frame Control (2 bytes)
    uint16_t fc = buf[offset] | (buf[offset + 1] << 8);
    pkt.wifi.frame_control = fc;
    DissectedField fc_field;
    parse_frame_control(fc, fc_field, pkt.wifi);
    mac_header.subfields.push_back(fc_field);
    offset += 2;

    // Duration / ID (2 bytes)
    uint16_t duration = buf[offset] | (buf[offset + 1] << 8);
    DissectedField dur_field;
    dur_field.name = "Duration / ID";
    dur_field.value = std::to_string(duration) + " microseconds";
    mac_header.subfields.push_back(dur_field);
    offset += 2;

    // Address 1: Destination / Receiver Address (RA/DA)
    pkt.wifi.dest_mac = format_mac(&buf[offset]);
    pkt.destination_address = pkt.wifi.dest_mac;
    DissectedField addr1_field;
    addr1_field.name = "Address 1 (RA/DA)";
    addr1_field.value = pkt.wifi.dest_mac;
    mac_header.subfields.push_back(addr1_field);
    offset += 6;

    // Address 2: Source / Transmitter Address (TA/SA)
    pkt.wifi.source_mac = format_mac(&buf[offset]);
    pkt.source_address = pkt.wifi.source_mac;
    DissectedField addr2_field;
    addr2_field.name = "Address 2 (TA/SA/BSSID)";
    addr2_field.value = pkt.wifi.source_mac;
    mac_header.subfields.push_back(addr2_field);
    offset += 6;

    // Address 3: BSSID / Filtering Address
    pkt.wifi.bssid = format_mac(&buf[offset]);
    DissectedField addr3_field;
    addr3_field.name = "Address 3 (BSSID)";
    addr3_field.value = pkt.wifi.bssid;
    mac_header.subfields.push_back(addr3_field);
    offset += 6;

    // Sequence Control (2 bytes)
    uint16_t seq_ctrl = buf[offset] | (buf[offset + 1] << 8);
    pkt.wifi.sequence_number = (seq_ctrl >> 4) & 0x0FFF;
    uint8_t frag_num = seq_ctrl & 0x0F;
    DissectedField seq_field;
    seq_field.name = "Sequence Control";
    seq_field.value = "Sequence Number: " + std::to_string(pkt.wifi.sequence_number) + ", Fragment: " + std::to_string(frag_num);
    mac_header.subfields.push_back(seq_field);
    offset += 2;

    pkt.dissection_tree.push_back(mac_header);

    // 3. Management Frame Body (Beacon, Probe Request/Response, Assoc)
    if (pkt.wifi.type == 0) {
        DissectedField mgmt_body;
        mgmt_body.name = "IEEE 802.11 Wireless Management Body";
        mgmt_body.is_unencrypted = true;

        if (pkt.wifi.subtype == 8 || pkt.wifi.subtype == 5) { // Beacon (8) or Probe Response (5)
            // Fixed Parameters: Timestamp (8), Beacon Int (2), Cap (2)
            if (len - offset >= 12) {
                uint64_t ts = 0;
                for (int b = 0; b < 8; ++b) {
                    ts |= (static_cast<uint64_t>(buf[offset + b]) << (b * 8));
                }
                pkt.wifi.hardware_timestamp = ts;
                DissectedField ts_field;
                ts_field.name = "Timestamp (64-bit microsecond clock)";
                std::ostringstream ts_ss;
                ts_ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << ts << " (" << ts << " us)";
                ts_field.value = ts_ss.str();
                mgmt_body.subfields.push_back(ts_field);
                offset += 8;

                uint16_t beacon_int = buf[offset] | (buf[offset + 1] << 8);
                pkt.wifi.beacon_interval_tu = beacon_int;
                DissectedField bint_field;
                bint_field.name = "Beacon Interval";
                bint_field.value = std::to_string(beacon_int) + " TU (" + std::to_string(static_cast<int>(beacon_int * 1.024)) + " ms)";
                mgmt_body.subfields.push_back(bint_field);
                offset += 2;

                uint16_t cap = buf[offset] | (buf[offset + 1] << 8);
                pkt.wifi.capabilities = cap;
                DissectedField cap_field;
                cap_field.name = "Capability Information";
                std::string cap_str = (cap & 0x01) ? "ESS (AP) " : "IBSS (Ad-hoc) ";
                if (cap & 0x10) cap_str += "| Privacy (Encrypted) ";
                if (cap & 0x20) cap_str += "| Short Preamble ";
                if (cap & 0x400) cap_str += "| Short Slot Time ";
                if (cap & 0x1000) cap_str += "| Radio Measurement (802.11k) ";
                cap_field.value = cap_str;
                mgmt_body.subfields.push_back(cap_field);
                offset += 2;
            }

            // Tagged Parameters (IEs)
            if (offset < len) {
                DissectedField ies_field;
                ies_field.name = "Tagged Information Elements (IEs)";
                ies_field.is_unencrypted = true;
                parse_tagged_parameters(&buf[offset], len - offset, ies_field, pkt.wifi);
                mgmt_body.subfields.push_back(ies_field);
            }
        } else if (pkt.wifi.subtype == 4) { // Probe Request
            if (offset < len) {
                DissectedField ies_field;
                ies_field.name = "Tagged Information Elements (IEs)";
                ies_field.is_unencrypted = true;
                parse_tagged_parameters(&buf[offset], len - offset, ies_field, pkt.wifi);
                mgmt_body.subfields.push_back(ies_field);
            }
        }

        pkt.dissection_tree.push_back(mgmt_body);
    }

    // Set summary
    if (pkt.wifi.type == 0 && pkt.wifi.subtype == 8) {
        pkt.protocol_subtype = "Beacon";
        pkt.summary_description = "Beacon Frame, SSID: \"" + pkt.wifi.ssid + "\", Ch: " + std::to_string(pkt.wifi.channel) + ", Enc: " + pkt.wifi.encryption;
    } else if (pkt.wifi.type == 0 && pkt.wifi.subtype == 4) {
        pkt.protocol_subtype = "Probe Request";
        std::string target = pkt.wifi.ssid.empty() ? "[Broadcast]" : "\"" + pkt.wifi.ssid + "\"";
        pkt.summary_description = "Probe Request from " + pkt.wifi.source_mac + " for " + target;
    } else if (pkt.wifi.type == 0 && pkt.wifi.subtype == 5) {
        pkt.protocol_subtype = "Probe Response";
        pkt.summary_description = "Probe Response, SSID: \"" + pkt.wifi.ssid + "\", BSSID: " + pkt.wifi.bssid;
    } else if (pkt.wifi.type == 2) {
        pkt.protocol_subtype = "Data";
        pkt.summary_description = "Data Frame, " + pkt.wifi.source_mac + " -> " + pkt.wifi.dest_mac;
    } else {
        pkt.protocol_subtype = "Management (" + std::to_string(pkt.wifi.subtype) + ")";
        pkt.summary_description = "802.11 Frame, Type: " + std::to_string(pkt.wifi.type) + ", Subtype: " + std::to_string(pkt.wifi.subtype);
    }

    return true;
}

void WifiDissector::parse_frame_control(uint16_t fc, DissectedField& fc_field, WifiMetadata& meta) {
    fc_field.name = "Frame Control Field";
    
    meta.type = (fc >> 2) & 0x03;
    meta.subtype = (fc >> 4) & 0x0F;
    meta.is_protected = (fc & 0x4000) != 0;

    std::string type_name;
    switch (meta.type) {
        case 0: type_name = "Management"; break;
        case 1: type_name = "Control"; break;
        case 2: type_name = "Data"; break;
        case 3: type_name = "Extension"; break;
    }

    std::string subtype_name;
    if (meta.type == 0) {
        switch (meta.subtype) {
            case 0: subtype_name = "Association Request"; break;
            case 1: subtype_name = "Association Response"; break;
            case 4: subtype_name = "Probe Request"; break;
            case 5: subtype_name = "Probe Response"; break;
            case 8: subtype_name = "Beacon"; break;
            case 11: subtype_name = "Authentication"; break;
            case 12: subtype_name = "Deauthentication"; break;
            default: subtype_name = "Subtype " + std::to_string(meta.subtype); break;
        }
    } else if (meta.type == 2) {
        switch (meta.subtype) {
            case 0: subtype_name = "Data"; break;
            case 4: subtype_name = "Null function (No Data)"; break;
            case 8: subtype_name = "QoS Data"; break;
            default: subtype_name = "Data Subtype " + std::to_string(meta.subtype); break;
        }
    }

    fc_field.value = "Type: " + type_name + " (0x" + std::to_string(meta.type) + "), Subtype: " + subtype_name;

    DissectedField f_ver{"Protocol Version", std::to_string(fc & 0x03)};
    DissectedField f_tods{"To DS", (fc & 0x0100) ? "1 (To AP)" : "0"};
    DissectedField f_fromds{"From DS", (fc & 0x0200) ? "1 (From AP)" : "0"};
    DissectedField f_morefrag{"More Fragments", (fc & 0x0400) ? "1" : "0"};
    DissectedField f_retry{"Retry", (fc & 0x0800) ? "1 (Retransmission)" : "0"};
    DissectedField f_pwrmgmt{"Power Management", (fc & 0x1000) ? "1 (Power Save)" : "0 (Active)"};
    DissectedField f_moredata{"More Data", (fc & 0x2000) ? "1" : "0"};
    DissectedField f_protected{"Protected Frame", meta.is_protected ? "1 (Encrypted)" : "0 (Unencrypted)"};
    DissectedField f_order{"Order / HTC", (fc & 0x8000) ? "1" : "0"};

    fc_field.subfields.push_back(f_ver);
    fc_field.subfields.push_back(f_tods);
    fc_field.subfields.push_back(f_fromds);
    fc_field.subfields.push_back(f_morefrag);
    fc_field.subfields.push_back(f_retry);
    fc_field.subfields.push_back(f_pwrmgmt);
    fc_field.subfields.push_back(f_moredata);
    fc_field.subfields.push_back(f_protected);
    fc_field.subfields.push_back(f_order);
}

void WifiDissector::parse_tagged_parameters(const uint8_t* data, size_t len, DissectedField& ies_field, WifiMetadata& meta) {
    size_t i = 0;
    while (i + 2 <= len) {
        uint8_t tag_num = data[i];
        uint8_t tag_len = data[i + 1];
        i += 2;

        if (i + tag_len > len) break;

        const uint8_t* tag_data = &data[i];
        DissectedField ie;
        ie.is_unencrypted = true;

        if (tag_num == 0) { // SSID
            std::string ssid_str(reinterpret_cast<const char*>(tag_data), tag_len);
            if (tag_len == 0 || ssid_str.find_first_not_of('\0') == std::string::npos) {
                meta.ssid = "<Hidden SSID>";
                meta.is_hidden_ssid = true;
            } else {
                meta.ssid = ssid_str;
                meta.is_hidden_ssid = false;
            }
            ie.name = "Tag 0: SSID parameter set";
            ie.value = "\"" + meta.ssid + "\" (Length: " + std::to_string(tag_len) + (meta.is_hidden_ssid ? ", [HIDDEN CLOAKED]" : "") + ")";
        } else if (tag_num == 1) { // Supported Rates
            ie.name = "Tag 1: Supported Rates";
            std::ostringstream ss;
            for (int r = 0; r < tag_len; ++r) {
                float rate = (tag_data[r] & 0x7F) * 0.5f;
                bool is_basic = (tag_data[r] & 0x80) != 0;
                ss << rate << (is_basic ? "(B) " : " ");
                meta.supported_rates.push_back(std::to_string(static_cast<int>(rate)) + (is_basic ? " (Basic)" : ""));
            }
            ss << "Mbps";
            ie.value = ss.str();
        } else if (tag_num == 50) { // Extended Supported Rates
            ie.name = "Tag 50: Extended Supported Rates";
            std::ostringstream ss;
            for (int r = 0; r < tag_len; ++r) {
                float rate = (tag_data[r] & 0x7F) * 0.5f;
                ss << rate << " ";
                meta.extended_rates.push_back(std::to_string(static_cast<int>(rate)) + " Mbps");
            }
            ss << "Mbps";
            ie.value = ss.str();
        } else if (tag_num == 3) { // DS Parameter Set (Current Channel)
            if (tag_len >= 1) {
                meta.channel = tag_data[0];
                ie.name = "Tag 3: DSSS Parameter Set";
                ie.value = "Current Channel: " + std::to_string(meta.channel);
            }
        } else if (tag_num == 5) { // TIM (Traffic Indication Map)
            if (tag_len >= 4) {
                meta.dtim_count = tag_data[0];
                meta.dtim_period = tag_data[1];
                ie.name = "Tag 5: Traffic Indication Map (TIM)";
                ie.value = "DTIM Period: " + std::to_string(meta.dtim_period) + ", DTIM Count: " + std::to_string(meta.dtim_count);
            }
        } else if (tag_num == 45) { // HT Capabilities (802.11n)
            meta.phy_standard = "802.11n / Wi-Fi 4 (HT)";
            ie.name = "Tag 45: HT Capabilities (802.11n)";
            ie.value = "MIMO 2x2 / 40 MHz Capable, SGI 400ns";
        } else if (tag_num == 61) { // HT Operation
            if (tag_len >= 2) {
                uint8_t sec_offset = tag_data[1] & 0x03;
                if (sec_offset == 1) meta.channel_width = "40 MHz (HT40+)";
                else if (sec_offset == 3) meta.channel_width = "40 MHz (HT40-)";
                else meta.channel_width = "20 MHz";
            }
            ie.name = "Tag 61: HT Operation";
            ie.value = "Primary Ch: " + std::to_string(meta.channel) + ", Width: " + meta.channel_width;
        } else if (tag_num == 191) { // VHT Capabilities (802.11ac)
            meta.phy_standard = "802.11ac / Wi-Fi 5 (VHT)";
            meta.channel_width = "80 MHz";
            ie.name = "Tag 191: VHT Capabilities (802.11ac)";
            ie.value = "MU-MIMO, 80/160 MHz Support, 256-QAM";
        } else if (tag_num == 48) { // RSN Information (WPA2 / WPA3)
            ie.name = "Tag 48: RSN Information (WPA2/WPA3)";
            meta.encryption = "WPA2-PSK (AES-CCMP)";
            meta.akm_suite = "PSK (00-0F-AC:2)";
            meta.cipher_suite = "AES-CCMP-128 (00-0F-AC:4)";
            meta.group_cipher = "AES-CCMP-128";

            if (tag_len >= 2) {
                uint16_t rsn_ver = tag_data[0] | (tag_data[1] << 8);
                ie.subfields.push_back({"RSN Version", std::to_string(rsn_ver)});
            }
            if (tag_len >= 6) { // Group Cipher
                std::string grp_str = (tag_data[5] == 0x04) ? "AES-CCMP (00-0F-AC:4)" : (tag_data[5] == 0x02 ? "TKIP" : "GCMP");
                meta.group_cipher = grp_str;
                ie.subfields.push_back({"Group Cipher Suite", grp_str});
            }
            if (tag_len >= 12) { // Pairwise Cipher
                uint16_t p_count = tag_data[6] | (tag_data[7] << 8);
                std::string pw_str = (tag_data[11] == 0x04) ? "AES-CCMP (00-0F-AC:4)" : (tag_data[11] == 0x08 ? "GCMP-256" : "TKIP");
                meta.cipher_suite = pw_str;
                ie.subfields.push_back({"Pairwise Cipher Suite", pw_str + " (Count: " + std::to_string(p_count) + ")"});
            }
            if (tag_len >= 16) { // AKM Suite
                uint16_t a_count = tag_data[12] | (tag_data[13] << 8);
                uint8_t akm_type = tag_data[17];
                std::string akm_str = "PSK (00-0F-AC:2)";
                if (akm_type == 0x08) {
                    akm_str = "SAE Dragonfly (00-0F-AC:8)";
                    meta.encryption = "WPA3-SAE (Personal)";
                } else if (akm_type == 0x01) {
                    akm_str = "802.1X (00-0F-AC:1)";
                    meta.encryption = "WPA2-Enterprise";
                } else if (akm_type == 0x04) {
                    akm_str = "FT-PSK (00-0F-AC:4)";
                }
                meta.akm_suite = akm_str;
                ie.subfields.push_back({"Auth Key Mgmt (AKM)", akm_str + " (Count: " + std::to_string(a_count) + ")"});
            }
            if (tag_len >= 18) { // RSN Capabilities (MFP / PMF)
                uint16_t rsn_cap = tag_data[18] | (tag_data[19] << 8);
                meta.pmf_capable = (rsn_cap & 0x0080) != 0;
                meta.pmf_required = (rsn_cap & 0x0040) != 0;
                std::string pmf_str = meta.pmf_required ? "PMF Required (802.11w)" : (meta.pmf_capable ? "PMF Capable / Optional" : "PMF Disabled");
                meta.rsn_capabilities = pmf_str;
                ie.subfields.push_back({"RSN Capabilities", pmf_str});
            }
            ie.value = meta.encryption + " | AKM: " + meta.akm_suite + " | Cipher: " + meta.cipher_suite;
        } else if (tag_num == 221) { // Vendor Specific (WPS, WPA1, Microsoft, Broadcom)
            ie.name = "Tag 221: Vendor Specific Information Element";
            if (tag_len >= 4) {
                uint32_t oui = (tag_data[0] << 16) | (tag_data[1] << 8) | tag_data[2];
                uint8_t oui_type = tag_data[3];

                if (oui == 0x0050F2 && oui_type == 0x04) { // WPS (Wi-Fi Protected Setup)
                    meta.has_wps = true;
                    ie.name = "Tag 221: Wi-Fi Protected Setup (WPS 2.0)";
                    ie.value = "WPS 2.0 Supported";

                    size_t wps_i = 4;
                    while (wps_i + 4 <= tag_len) {
                        uint16_t wps_tag = (tag_data[wps_i] << 8) | tag_data[wps_i + 1];
                        uint16_t wps_sublen = (tag_data[wps_i + 2] << 8) | tag_data[wps_i + 3];
                        wps_i += 4;

                        if (wps_i + wps_sublen > tag_len) break;
                        const uint8_t* wps_val = &tag_data[wps_i];

                        if (wps_tag == 0x1023) { // Model Name
                            meta.vendor_wps_model = std::string(reinterpret_cast<const char*>(wps_val), wps_sublen);
                            ie.subfields.push_back({"WPS Model Name", meta.vendor_wps_model});
                        } else if (wps_tag == 0x1011) { // Device Name
                            meta.vendor_wps_device_name = std::string(reinterpret_cast<const char*>(wps_val), wps_sublen);
                            ie.subfields.push_back({"WPS Device Name", meta.vendor_wps_device_name});
                        } else if (wps_tag == 0x1021) { // Manufacturer
                            meta.wps_manufacturer = std::string(reinterpret_cast<const char*>(wps_val), wps_sublen);
                            ie.subfields.push_back({"WPS Manufacturer", meta.wps_manufacturer});
                        } else if (wps_tag == 0x1024) { // Model Number
                            meta.wps_model_number = std::string(reinterpret_cast<const char*>(wps_val), wps_sublen);
                            ie.subfields.push_back({"WPS Model Number", meta.wps_model_number});
                        } else if (wps_tag == 0x1042) { // Serial Number
                            meta.wps_serial_number = std::string(reinterpret_cast<const char*>(wps_val), wps_sublen);
                            ie.subfields.push_back({"WPS Serial Number", meta.wps_serial_number});
                        } else if (wps_tag == 0x1057) { // AP Setup Locked
                            if (wps_sublen >= 1) {
                                meta.wps_locked = (wps_val[0] != 0);
                                ie.subfields.push_back({"AP Setup Locked (WPS Lockout)", meta.wps_locked ? "LOCKED (Secure)" : "UNLOCKED [VULNERABLE]"});
                            }
                        }
                        wps_i += wps_sublen;
                    }
                } else if (oui == 0x0050F2 && oui_type == 0x01) { // WPA1 (TKIP)
                    if (meta.encryption == "Open") meta.encryption = "WPA-PSK (TKIP)";
                    ie.value = "Microsoft WPA1 (TKIP)";
                } else if (oui == 0x001018) {
                    meta.vendor_specific_ies.push_back("Broadcom WiFi");
                    ie.value = "Broadcom Proprietary Extension";
                } else if (oui == 0x00037F) {
                    meta.vendor_specific_ies.push_back("Qualcomm Atheros");
                    ie.value = "Qualcomm Atheros Extension";
                } else if (oui == 0x000142) {
                    meta.vendor_specific_ies.push_back("Cisco Systems");
                    ie.value = "Cisco / Meraki Enterprise Extension";
                } else if (oui == 0x0017F2) {
                    meta.vendor_specific_ies.push_back("Apple Inc.");
                    ie.value = "Apple AirPort / iOS Extension";
                } else {
                    std::ostringstream v_ss;
                    v_ss << "Vendor OUI 0x" << std::hex << std::setw(6) << std::setfill('0') << oui;
                    ie.value = v_ss.str();
                }
            }
        } else {
            std::ostringstream unk_ss;
            unk_ss << "Tag " << static_cast<int>(tag_num) << " (" << static_cast<int>(tag_len) << " bytes)";
            ie.name = unk_ss.str();
            ie.value = "Raw Tag Data";
        }

        ies_field.subfields.push_back(ie);
        i += tag_len;
    }
}

} // namespace discan
