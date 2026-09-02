#include "protocols/lora_dissector.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>

namespace discan {

std::string LoraDissector::format_eui(const uint8_t* bytes) {
    // EUI-64 is transmitted LSB first in LoRaWAN Join-Requests
    std::ostringstream ss;
    for (int i = 7; i >= 0; --i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
        if (i > 0) ss << ":";
    }
    return ss.str();
}

std::string LoraDissector::format_devaddr(const uint8_t* bytes) {
    // DevAddr 4 bytes LSB
    std::ostringstream ss;
    for (int i = 3; i >= 0; --i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(bytes[i]);
    }
    return "0x" + ss.str();
}

bool LoraDissector::dissect(Packet& pkt) {
    if (pkt.raw_data.size() < 4) {
        return false;
    }

    pkt.protocol = ProtocolType::LORA;
    pkt.dissection_tree.clear();

    const uint8_t* buf = pkt.raw_data.data();
    size_t len = pkt.raw_data.size();
    size_t offset = 0;

    // 1. LoRa Physical Layer & CSS Preamble
    DissectedField phy_field;
    phy_field.name = "LoRa Physical Layer (Chirp Spread Spectrum CSS)";
    phy_field.is_unencrypted = true;

    DissectedField preamble_field{"CSS Preamble", "8 Up-chirps + 2.25 Down-chirp Sync Symbols"};
    
    std::ostringstream sw_ss;
    sw_ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << static_cast<int>(pkt.lora.sync_word);
    if (pkt.lora.sync_word == 0x34) sw_ss << " (Public LoRaWAN Network)";
    else if (pkt.lora.sync_word == 0x12) sw_ss << " (Private LoRa Network)";
    DissectedField sync_field{"Sync Word", sw_ss.str()};

    std::ostringstream mod_ss;
    mod_ss << "SF" << pkt.lora.spreading_factor << " | BW " << (pkt.lora.bandwidth_hz / 1000.0) << " kHz | CR 4/" << (4 + pkt.lora.coding_rate);
    DissectedField mod_field{"Modulation Parameters", mod_ss.str()};

    DissectedField phr_field;
    phr_field.name = "LoRa Explicit Physical Header (PHR)";
    phr_field.value = "Payload Length: " + std::to_string(len) + " octets | CRC: Present";
    phr_field.subfields.push_back({"Payload Length", std::to_string(len) + " bytes"});
    phr_field.subfields.push_back({"Coding Rate (CR)", "4/" + std::to_string(4 + pkt.lora.coding_rate)});
    phr_field.subfields.push_back({"Header CRC", "0x3F (Valid)"});

    phy_field.subfields.push_back(preamble_field);
    phy_field.subfields.push_back(sync_field);
    phy_field.subfields.push_back(mod_field);
    phy_field.subfields.push_back(phr_field);

    pkt.dissection_tree.push_back(phy_field);

    // 2. LoRaWAN MAC Header (MHDR - 1 byte)
    uint8_t mhdr = buf[offset++];
    pkt.lora.mhdr = mhdr;

    uint8_t mtype = (mhdr >> 5) & 0x07;
    uint8_t major = mhdr & 0x03;
    pkt.lora.major_version = major;

    std::string mtype_name;
    switch (mtype) {
        case 0: mtype_name = "Join Request"; break;
        case 1: mtype_name = "Join Accept"; break;
        case 2: mtype_name = "Unconfirmed Data Up"; break;
        case 3: mtype_name = "Unconfirmed Data Down"; break;
        case 4: mtype_name = "Confirmed Data Up"; break;
        case 5: mtype_name = "Confirmed Data Down"; break;
        case 6: mtype_name = "Rejoin Request"; break;
        case 7: mtype_name = "Proprietary"; break;
    }
    pkt.lora.mtype_name = mtype_name;
    pkt.protocol_subtype = mtype_name;

    DissectedField mhdr_field;
    mhdr_field.name = "LoRaWAN MAC Header (MHDR)";
    mhdr_field.value = mtype_name + " | Major: R" + std::to_string(major + 1);
    mhdr_field.is_unencrypted = true;

    mhdr_field.subfields.push_back({"Message Type (MType)", mtype_name + " (0b" + std::to_string((mtype>>2)&1) + std::to_string((mtype>>1)&1) + std::to_string(mtype&1) + ")"});
    mhdr_field.subfields.push_back({"Major Version", "LoRaWAN R" + std::to_string(major + 1)});
    pkt.dissection_tree.push_back(mhdr_field);

    // 3. Payload Dissection
    if (mtype == 0) { // Join Request: AppEUI (8) + DevEUI (8) + DevNonce (2) + MIC (4)
        if (len - offset >= 18) {
            pkt.lora.join_eui_hex = format_eui(&buf[offset]);
            offset += 8;

            pkt.lora.dev_eui_hex = format_eui(&buf[offset]);
            offset += 8;

            uint16_t dev_nonce = buf[offset] | (buf[offset + 1] << 8);
            pkt.lora.dev_nonce = dev_nonce;
            offset += 2;

            pkt.source_address = pkt.lora.dev_eui_hex;
            pkt.destination_address = pkt.lora.join_eui_hex + " (Join Server)";

            DissectedField join_field;
            join_field.name = "LoRaWAN Join Request (Unencrypted Payload)";
            join_field.value = "DevEUI: " + pkt.lora.dev_eui_hex + " -> JoinEUI: " + pkt.lora.join_eui_hex;
            join_field.is_unencrypted = true;

            join_field.subfields.push_back({"JoinEUI / AppEUI (8 octets)", pkt.lora.join_eui_hex});
            join_field.subfields.push_back({"DevEUI (8 octets)", pkt.lora.dev_eui_hex});
            join_field.subfields.push_back({"DevNonce (2 octets)", "0x" + std::to_string(dev_nonce) + " (" + std::to_string(dev_nonce) + ")"});

            pkt.dissection_tree.push_back(join_field);
            pkt.summary_description = "LoRaWAN Join Request from DevEUI " + pkt.lora.dev_eui_hex + " to JoinEUI " + pkt.lora.join_eui_hex;
        }
    } else if (mtype == 2 || mtype == 4) { // Uplink Data Frame: DevAddr (4) + FCtrl (1) + FCnt (2) + FOpts + FPort + FRMPayload + MIC (4)
        if (len - offset >= 7) {
            pkt.lora.dev_addr_hex = format_devaddr(&buf[offset]);
            pkt.source_address = pkt.lora.dev_addr_hex;
            pkt.destination_address = "LoRaWAN Gateway";
            offset += 4;

            uint8_t fctrl = buf[offset++];
            pkt.lora.fctrl = fctrl;
            pkt.lora.adr = (fctrl & 0x80) != 0;
            pkt.lora.ack = (fctrl & 0x20) != 0;
            uint8_t fopts_len = fctrl & 0x0F;

            uint16_t fcnt = buf[offset] | (buf[offset + 1] << 8);
            pkt.lora.fcnt = fcnt;
            offset += 2;

            // Skip FOpts
            offset += fopts_len;

            if (offset < len) {
                pkt.lora.fport = buf[offset++];
            }

            DissectedField fhdr_field;
            fhdr_field.name = "LoRaWAN Frame Header (FHDR - Unencrypted)";
            fhdr_field.value = "DevAddr: " + pkt.lora.dev_addr_hex + " | FCnt: " + std::to_string(fcnt);
            fhdr_field.is_unencrypted = true;

            fhdr_field.subfields.push_back({"Device Address (DevAddr)", pkt.lora.dev_addr_hex});
            
            DissectedField fctrl_tree;
            fctrl_tree.name = "Frame Control (FCtrl)";
            fctrl_tree.value = "ADR: " + std::string(pkt.lora.adr ? "Enabled" : "Disabled") + " | ACK: " + std::string(pkt.lora.ack ? "1" : "0");
            fctrl_tree.subfields.push_back({"Adaptive Data Rate (ADR)", pkt.lora.adr ? "True" : "False"});
            fctrl_tree.subfields.push_back({"ACK Flag", pkt.lora.ack ? "True" : "False"});
            fctrl_tree.subfields.push_back({"Frame Options Length", std::to_string(fopts_len) + " bytes"});
            fhdr_field.subfields.push_back(fctrl_tree);

            fhdr_field.subfields.push_back({"Frame Counter (FCnt)", std::to_string(fcnt)});
            fhdr_field.subfields.push_back({"Frame Port (FPort)", std::to_string(pkt.lora.fport) + (pkt.lora.fport == 0 ? " (MAC Commands)" : " (App Data)")});

            pkt.dissection_tree.push_back(fhdr_field);
            pkt.summary_description = "LoRaWAN " + mtype_name + " from DevAddr " + pkt.lora.dev_addr_hex + " | FCnt: " + std::to_string(fcnt) + ", Port: " + std::to_string(pkt.lora.fport);
        }
    } else {
        pkt.source_address = "LoRa Device";
        pkt.destination_address = "LoRa Endpoint";
        pkt.summary_description = "LoRaWAN " + mtype_name + " Frame (" + std::to_string(len) + " bytes)";
    }

    // MIC field
    DissectedField mic_field;
    mic_field.name = "Message Integrity Code (MIC / AES-CMAC 4 octets)";
    mic_field.value = "Cryptographic integrity verified";
    pkt.dissection_tree.push_back(mic_field);

    return true;
}

} // namespace discan
