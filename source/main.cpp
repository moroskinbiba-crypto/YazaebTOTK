#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "explorer.hpp"

#include <memory>

std::unique_ptr<tsl::Gui> createMainGui();

class TotkExplorerOverlay final : public tsl::Overlay {
public:
    void initServices() override {
        // Keep startup lightweight: dmnt:cht is initialized lazily by the
        // Calibration/Map GUI and remains alive while a persistent GUI is shown.
        ex::loadPoints();
    }

    void exitServices() override {
        ex::shutdownMemory();
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return createMainGui();
    }
};

int main(int argc, char** argv) {
    // Persistent HUD is a GUI mode inside the same Tesla overlay instance.
    // Do not close the overlay and relaunch the .ovl: that would tear down
    // services and destroy the Tesla layer.
    return tsl::loop<TotkExplorerOverlay>(argc, argv);
}
