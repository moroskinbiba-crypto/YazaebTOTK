#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "explorer.hpp"

#include <memory>

class MainGui;
std::unique_ptr<tsl::Gui> createMainGui();

class TotkExplorerOverlay final : public tsl::Overlay {
public:
    void initServices() override {
        ex::loadPoints();
        (void)ex::initMemory();
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
