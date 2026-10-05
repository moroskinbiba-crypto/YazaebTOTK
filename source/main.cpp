#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "explorer.hpp"

#include <memory>

class MainGui;
std::unique_ptr<tsl::Gui> createMainGui();

class TotkExplorerOverlay final : public tsl::Overlay {
public:
    void initServices() override {
        // Keep startup lightweight: dmnt:cht is initialized lazily from the GUI
        // when the user actually requests player-coordinate access.
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
    return tsl::loop<TotkExplorerOverlay>(argc, argv);
}
