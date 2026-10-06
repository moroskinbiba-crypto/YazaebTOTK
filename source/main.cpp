#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "explorer.hpp"

#include <cstring>
#include <memory>

class MainGui;
std::unique_ptr<tsl::Gui> createMainGui();
std::unique_ptr<tsl::Gui> createPersistentHudGui();

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

class TotkExplorerHudOverlay final : public tsl::Overlay {
public:
    void initServices() override {
        ex::loadPoints();
        ex::ensureMemory();
    }

    void exitServices() override {
        ex::shutdownMemory();
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return createPersistentHudGui();
    }
};

int main(int argc, char** argv) {
    ex::setOverlayPath(argc > 0 ? argv[0] : nullptr);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hud") == 0) {
            return tsl::loop<TotkExplorerHudOverlay>(argc, argv);
        }
    }

    return tsl::loop<TotkExplorerOverlay>(argc, argv);
}
