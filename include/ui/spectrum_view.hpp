#pragma once

namespace discan {

class SpectrumView {
public:
    static void render();

private:
    static bool show_waterfall_;
    static float floor_dbm_;
    static float ceiling_dbm_;
};

} // namespace discan
