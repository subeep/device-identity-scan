#pragma once

#include "common/types.hpp"
#include "protocols/packet.hpp"
#include <thread>
#include <atomic>
#include <functional>
#include <string>

namespace discan {

class WifiLiveScanner {
public:
    static WifiLiveScanner& instance() {
        static WifiLiveScanner inst;
        return inst;
    }

    void start(std::function<void(const PacketPtr&)> packet_cb);
    void stop();
    bool is_running() const { return running_; }

private:
    WifiLiveScanner() = default;
    ~WifiLiveScanner() { stop(); }

    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::function<void(const PacketPtr&)> packet_callback_;

    void scanner_loop();
    void scan_once();
};

} // namespace discan
