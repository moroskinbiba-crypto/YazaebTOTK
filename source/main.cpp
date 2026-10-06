#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include <memory>

class Stage1Gui final : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK Explorer", "Stage 1");
        auto* list = new tsl::elm::List();
        frame->setContent(list);
        return frame;
    }

    bool handleInput(
        u64 keysDown,
        u64,
        const HidTouchState&,
        HidAnalogStickState,
        HidAnalogStickState) override {
        if (keysDown & KEY_B) {
            tsl::Overlay::get()->close();
            return true;
        }

        return false;
    }
};

class TotkExplorerOverlay final : public tsl::Overlay {
public:
    void initServices() override {}
    void exitServices() override {}

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return std::make_unique<Stage1Gui>();
    }
};

int main(int argc, char** argv) {
    return tsl::loop<TotkExplorerOverlay>(argc, argv);
}
