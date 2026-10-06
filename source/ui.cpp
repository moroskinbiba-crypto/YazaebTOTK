#include <tesla.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "explorer.hpp"

namespace ex {
namespace {
std::string g_overlayPath{};
}
void setOverlayPath(const char* path) {
    g_overlayPath = path ? path : "";
}
const std::string& overlayPath() {
    return g_overlayPath;
}
} // namespace ex

namespace {

std::string f2(float value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    return buffer;
}

std::string distanceText(const ex::Vec3& player, const ex::Point& point) {
    const float dx = point.x - player.x;
    const float dy = point.y - player.y;
    const float dz = point.z - player.z;
    const float distance = std::sqrt(dx * dx + dy * dy + 0.25f * dz * dz);

    char buffer[48]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f m", static_cast<double>(distance));
    return buffer;
}

std::string deltaText(const ex::Vec3& player, const ex::Point& point) {
    char buffer[96]{};
    std::snprintf(buffer, sizeof(buffer), "dX %.0f  dY %.0f  dZ %.0f",
                  static_cast<double>(point.x - player.x),
                  static_cast<double>(point.y - player.y),
                  static_cast<double>(point.z - player.z));
    return buffer;
}

std::string hexText(u64 value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(value));
    return buffer;
}

class CalibrationGui final : public tsl::Gui {
    tsl::elm::ListItem* stageItem{};
    tsl::elm::ListItem* progressItem{};
    tsl::elm::ListItem* candidatesItem{};
    tsl::elm::ListItem* instructionItem{};

    void refresh() {
        const auto& state = ex::state();
        stageItem->setValue(ex::stageText(state.stage));

        if (state.heapSize > 0) {
            const double pct = 100.0 * static_cast<double>(state.scanned) / static_cast<double>(state.heapSize);
            char progress[48]{};
            std::snprintf(progress, sizeof(progress), "%.0f%%", pct);
            progressItem->setValue(progress);
        } else {
            progressItem->setValue("0%");
        }

        candidatesItem->setValue(std::to_string(state.candidates));

        switch (state.stage) {
            case ex::ScanStage::Scanning:
                instructionItem->setValue("Scanning memory...");
                break;
            case ex::ScanStage::WaitMove:
                instructionItem->setValue("Walk 5-10 m, reopen overlay if needed, press X");
                break;
            case ex::ScanStage::WaitJump:
                instructionItem->setValue("Jump / change elevation, press X");
                break;
            case ex::ScanStage::Ready:
                instructionItem->setValue("Calibration complete");
                break;
            case ex::ScanStage::Failed:
                instructionItem->setValue(state.message);
                break;
            default:
                instructionItem->setValue("Press A to start calibration");
                break;
        }
    }

public:
    tsl::elm::Element* createUI() override {
        // Re-opened overlays lose dmnt:cht in exitServices(); reconnect before
        // resuming a pending WaitMove/WaitJump calibration step.
        ex::ensureMemory();

        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", "Calibration");
        auto* list = new tsl::elm::List();

        stageItem = new tsl::elm::ListItem("Step");
        progressItem = new tsl::elm::ListItem("Progress");
        candidatesItem = new tsl::elm::ListItem("Candidates");
        instructionItem = new tsl::elm::ListItem("Instruction");
        list->addItem(stageItem);
        list->addItem(progressItem);
        list->addItem(candidatesItem);
        list->addItem(instructionItem);

        auto* start = new tsl::elm::ListItem("Start / Restart");
        start->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                ex::startAutoScan();
                return true;
            }
            return false;
        });
        list->addItem(start);

        list->addItem(new tsl::elm::ListItem("X = capture step"));
        list->addItem(new tsl::elm::ListItem("Y = reset"));

        frame->setContent(list);
        refresh();
        return frame;
    }

    void update() override {
        ex::tick();
        refresh();
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & KEY_X) {
            if (ex::state().stage == ex::ScanStage::WaitMove)
                ex::captureMove();
            else if (ex::state().stage == ex::ScanStage::WaitJump)
                ex::captureJump();
            return true;
        }

        if (keysDown & KEY_Y) {
            ex::resetScan();
            return true;
        }

        if (keysDown & KEY_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }
};

class PersistentHudGui final : public tsl::Gui {
    static constexpr s32 PANEL_W = 380;
    static constexpr s32 PANEL_H = 430;
    static constexpr s32 MAP_SIZE = 280;
    static constexpr s32 MARGIN = 18;
    static constexpr float RADIUS = 1800.0f;

    static tsl::Color bgColor() { return {1, 1, 2, 11}; }
    static tsl::Color panelColor() { return {1, 1, 2, 10}; }
    static tsl::Color gridColor() { return {7, 7, 9, 7}; }
    static tsl::Color textColor() { return {15, 15, 15, 15}; }
    static tsl::Color mutedColor() { return {9, 10, 12, 13}; }
    static tsl::Color playerColor() { return {5, 14, 15, 15}; }
    static tsl::Color shrineColor() { return {14, 12, 3, 15}; }
    static tsl::Color korokColor() { return {6, 13, 6, 15}; }
    static tsl::Color lightrootColor() { return {7, 10, 15, 15}; }
    static tsl::Color towerColor() { return {13, 6, 13, 15}; }
    static tsl::Color genericColor() { return {12, 12, 12, 15}; }

    static char marker(const ex::Point& point) {
        if (point.type == "Shrine") return 'S';
        if (point.type == "Korok") return 'K';
        if (point.type == "Lightroot") return 'L';
        if (point.type == "Tower") return 'T';
        return '*';
    }

    static tsl::Color markerColor(const ex::Point& point) {
        if (point.type == "Shrine") return shrineColor();
        if (point.type == "Korok") return korokColor();
        if (point.type == "Lightroot") return lightrootColor();
        if (point.type == "Tower") return towerColor();
        return genericColor();
    }

    static std::string layerText(const ex::Vec3& player) {
        return ex::layerName(player);
    }

    static std::string positionText(const ex::Vec3& player) {
        char buffer[96]{};
        std::snprintf(buffer, sizeof(buffer), "X %.1f   Y %.1f   H %.1f",
                      static_cast<double>(player.x),
                      static_cast<double>(player.y),
                      static_cast<double>(player.z));
        return buffer;
    }

    static std::string sourceText(const ex::State& state) {
        return state.exactPlayer ? "Exact Player actor"
                                 : (state.playerValid ? "Heuristic profile" : "Not detected");
    }

    static std::string nearestText(const ex::Vec3& player, const std::vector<ex::Point>& points) {
        if (points.empty())
            return "Nearest: none";

        const auto& point = points.front();
        const float dx = point.x - player.x;
        const float dy = point.y - player.y;
        const float dz = point.z - player.z;
        const float distance = std::sqrt(dx * dx + dy * dy + 0.25f * dz * dz);

        char buffer[128]{};
        std::snprintf(buffer, sizeof(buffer), "Nearest: %s  %.0fm",
                      point.name.c_str(), static_cast<double>(distance));
        return buffer;
    }

    static void drawMap(tsl::gfx::Renderer* renderer, const ex::State& state,
                        const std::vector<ex::Point>& points, s32 x, s32 y) {
        const s32 centerX = x + MAP_SIZE / 2;
        const s32 centerY = y + MAP_SIZE / 2;

        renderer->drawRoundedRect(x, y, MAP_SIZE, MAP_SIZE, 18, renderer->a(panelColor()));
        renderer->drawEmptyRect(x, y, MAP_SIZE, MAP_SIZE, renderer->a(gridColor()));

        constexpr s32 rings[] = {45, 90, 135};
        for (const s32 radius : rings)
            renderer->drawCircle(centerX, centerY, radius, false, renderer->a(gridColor()));

        renderer->drawLine(centerX, y + 8, centerX, y + MAP_SIZE - 8, renderer->a(gridColor()));
        renderer->drawLine(x + 8, centerY, x + MAP_SIZE - 8, centerY, renderer->a(gridColor()));

        const float pixelsPerMeter = static_cast<float>(MAP_SIZE / 2 - 16) / RADIUS;

        for (const auto& point : points) {
            const float dx = point.x - state.player.x;
            const float dy = point.y - state.player.y;
            if (std::fabs(dx) > RADIUS || std::fabs(dy) > RADIUS)
                continue;

            const s32 px = centerX + static_cast<s32>(std::lround(dx * pixelsPerMeter));
            const s32 py = centerY - static_cast<s32>(std::lround(dy * pixelsPerMeter));

            renderer->drawCircle(px, py, 4, true, renderer->a(markerColor(point)));

            char label[2] = {marker(point), '\0'};
            renderer->drawString(label, false, px + 5, py - 6, 11.0f,
                                  renderer->a(mutedColor()));
        }

        renderer->drawCircle(centerX, centerY, 6, true, renderer->a(playerColor()));
        renderer->drawCircle(centerX, centerY, 11, false, renderer->a(playerColor()));
    }

public:
    PersistentHudGui() {
        // Keep the HUD visible while normal controller input belongs to the game.
        tsl::disableHiding = true;
        tsl::hlp::requestForeground(false);
    }

    ~PersistentHudGui() override {
        tsl::disableHiding = false;
        tsl::hlp::requestForeground(true);
    }

    tsl::elm::Element* createUI() override {
        ex::ensureMemory();

        auto* rootFrame = new tsl::elm::OverlayFrame("", "");
        auto* drawer = new tsl::elm::CustomDrawer(
            [](tsl::gfx::Renderer* renderer, u16, u16, u16, u16) {
                const auto& state = ex::state();

                const s32 baseX = tsl::cfg::FramebufferWidth - PANEL_W - 16;
                const s32 baseY = 16;
                const s32 mapX = baseX + MARGIN + 32;
                const s32 mapY = baseY + 92;

                renderer->drawRoundedRect(baseX, baseY, PANEL_W, PANEL_H, 18, renderer->a(bgColor()));

                renderer->drawString("TOTK EXPLORER", false, baseX + MARGIN, baseY + 20, 20.0f,
                                     renderer->a(textColor()));
                renderer->drawString(layerText(state.player).c_str(), false,
                                     baseX + PANEL_W - 145, baseY + 22, 14.0f,
                                     renderer->a(mutedColor()));

                if (!state.dmntReady || !state.playerValid) {
                    renderer->drawString(state.dmntReady
                                             ? "Waiting for Player coordinates..."
                                             : "Game process unavailable",
                                         false, baseX + MARGIN, baseY + 58, 16.0f,
                                         renderer->a(mutedColor()));
                } else {
                    renderer->drawString(positionText(state.player).c_str(), false,
                                         baseX + MARGIN, baseY + 54, 14.0f,
                                         renderer->a(textColor()));
                    renderer->drawString(sourceText(state).c_str(), false,
                                         baseX + MARGIN, baseY + 74, 12.0f,
                                         renderer->a(mutedColor()));

                    const auto points = ex::nearby(RADIUS, 32);
                    drawMap(renderer, state, points, mapX, mapY);

                    char nearbyTextBuffer[64]{};
                    std::snprintf(nearbyTextBuffer, sizeof(nearbyTextBuffer),
                                  "Nearby: %zu points", points.size());
                    renderer->drawString(nearbyTextBuffer, false,
                                         baseX + MARGIN, baseY + PANEL_H - 72, 13.0f,
                                         renderer->a(textColor()));

                    renderer->drawString(nearestText(state.player, points).c_str(),
                                         false, baseX + MARGIN, baseY + PANEL_H - 48, 12.0f,
                                         renderer->a(mutedColor()));
                }

                renderer->drawString("L + R + Minus  close HUD", false,
                                     baseX + MARGIN, baseY + PANEL_H - 20, 11.0f,
                                     renderer->a(mutedColor()));
            });

        rootFrame->setContent(drawer);
        return rootFrame;
    }

    void update() override {
        ex::tick();
    }

    bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if ((keysHeld & (KEY_L | KEY_R)) == (KEY_L | KEY_R) && (keysDown & KEY_MINUS)) {
            tsl::Overlay::get()->close();
            return true;
        }
        return false;
    }
};

class NearbyGui final : public tsl::Gui {
    static constexpr std::size_t MAX = 20;
    std::array<tsl::elm::ListItem*, MAX> items{};

    void refresh() {
        const auto& state = ex::state();
        const auto points = ex::nearby(2000.0f, MAX);
        for (std::size_t i = 0; i < MAX; ++i) {
            if (!state.playerValid) {
                items[i]->setValue(i == 0 ? "Run Player Coordinates first" : "");
            } else if (i < points.size()) {
                items[i]->setValue(points[i].type + " · " + points[i].name + "  " + distanceText(state.player, points[i]) + "  " + deltaText(state.player, points[i]));
            } else {
                items[i]->setValue("");
            }
        }
    }

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", "Nearby");
        auto* list = new tsl::elm::List();
        list->addItem(new tsl::elm::ListItem("Nearest database points"));
        for (std::size_t i = 0; i < MAX; ++i) {
            items[i] = new tsl::elm::ListItem("Point " + std::to_string(i + 1));
            list->addItem(items[i]);
        }
        frame->setContent(list);
        refresh();
        return frame;
    }

    void update() override {
        ex::tick();
        refresh();
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & KEY_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }
};

class DiagnosticsGui final : public tsl::Gui {
    tsl::elm::ListItem* dmntItem{};
    tsl::elm::ListItem* pidItem{};
    tsl::elm::ListItem* heapItem{};
    tsl::elm::ListItem* pointsItem{};
    tsl::elm::ListItem* layerItem{};
    tsl::elm::ListItem* versionItem{};
    tsl::elm::ListItem* buildIdItem{};
    tsl::elm::ListItem* sourceItem{};
    tsl::elm::ListItem* actorItem{};
    tsl::elm::ListItem* rejectedItem{};

    void refresh() {
        const auto& state = ex::state();
        dmntItem->setValue(state.dmntReady ? "yes" : "no");
        pidItem->setValue(std::to_string(static_cast<unsigned long long>(state.processId)));
        heapItem->setValue(std::to_string(static_cast<unsigned long long>(state.heapSize)));
        pointsItem->setValue(std::to_string(state.points.size()));
        layerItem->setValue(state.playerValid ? ex::layerName(state.player) : "—");
        versionItem->setValue(state.gameVersion);
        buildIdItem->setValue(state.buildId + (state.buildIdMatched ? " (supported)" : " (unsupported)"));
        sourceItem->setValue(state.exactPlayer ? "exact Player actor" : (state.playerValid ? "heuristic profile" : "none"));
        actorItem->setValue(state.playerActor ? hexText(state.playerActor) : "—");
        rejectedItem->setValue(std::to_string(state.pointsRejected));
    }

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", "Diagnostics");
        auto* list = new tsl::elm::List();

        list->addItem(new tsl::elm::ListItem("Target"));
        list->addItem(new tsl::elm::ListItem("Title ID", ex::TITLE_TEXT));
        versionItem = new tsl::elm::ListItem("Version");
        buildIdItem = new tsl::elm::ListItem("Build ID");
        list->addItem(versionItem);
        list->addItem(buildIdItem);

        list->addItem(new tsl::elm::ListItem("Runtime"));
        dmntItem = new tsl::elm::ListItem("dmnt:cht");
        pidItem = new tsl::elm::ListItem("PID");
        heapItem = new tsl::elm::ListItem("Heap bytes");
        pointsItem = new tsl::elm::ListItem("Map points");
        layerItem = new tsl::elm::ListItem("World layer");
        sourceItem = new tsl::elm::ListItem("Coordinate source");
        list->addItem(dmntItem);
        list->addItem(pidItem);
        list->addItem(heapItem);
        list->addItem(pointsItem);
        list->addItem(layerItem);
        list->addItem(new tsl::elm::ListItem("Supported exact builds", ex::GAME_VERSION));
        list->addItem(sourceItem);
        actorItem = new tsl::elm::ListItem("Player actor");
        rejectedItem = new tsl::elm::ListItem("CSV rejected rows");
        list->addItem(actorItem);
        list->addItem(rejectedItem);

        list->addItem(new tsl::elm::ListItem("HUD Controls"));
        list->addItem(new tsl::elm::ListItem("Enable", "Main menu -> Map HUD (persistent) -> A"));
        list->addItem(new tsl::elm::ListItem("Disable", "L + R + Minus"));
        list->addItem(new tsl::elm::ListItem("HUD", "Stays visible after menu closes"));

        list->addItem(new tsl::elm::ListItem("Safety"));
        list->addItem(new tsl::elm::ListItem("Progress flags", "Not enabled"));
        list->addItem(new tsl::elm::ListItem("Memory writes", "Disabled"));

        frame->setContent(list);
        refresh();
        return frame;
    }

    void update() override {
        ex::tick();
        refresh();
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & KEY_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }
};

} // namespace

class MainGui final : public tsl::Gui {
    tsl::elm::ListItem* coordsItem{};

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", ex::VERSION);
        auto* list = new tsl::elm::List();

        coordsItem = new tsl::elm::ListItem("Player Coordinates");
        list->addItem(coordsItem);

        auto* calibration = new tsl::elm::ListItem("Calibration");
        calibration->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<CalibrationGui>();
                return true;
            }
            return false;
        });
        list->addItem(calibration);

        auto* map = new tsl::elm::ListItem("Map HUD (persistent)");
        map->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                const auto& path = ex::overlayPath();
                if (path.empty()) {
                    return false;
                }
                tsl::setNextOverlay(path, "--hud");
                tsl::Overlay::get()->close();
                return true;
            }
            return false;
        });
        list->addItem(map);

        auto* nearby = new tsl::elm::ListItem("Nearby Objects");
        nearby->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<NearbyGui>();
                return true;
            }
            return false;
        });
        list->addItem(nearby);

        auto* info = new tsl::elm::ListItem("Diagnostics");
        info->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<DiagnosticsGui>();
                return true;
            }
            return false;
        });
        list->addItem(info);

        frame->setContent(list);
        return frame;
    }

    void update() override {
        ex::tick();
        const auto& state = ex::state();
        coordsItem->setValue(state.playerValid
            ? f2(state.player.x) + " / " + f2(state.player.y) + " / " + f2(state.player.z)
            : "Not detected");
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & KEY_B) {
            tsl::Overlay::get()->close();
            return true;
        }
        return false;
    }
};

std::unique_ptr<tsl::Gui> createMainGui() {
    return std::make_unique<MainGui>();
}

std::unique_ptr<tsl::Gui> createPersistentHudGui() {
    return std::make_unique<PersistentHudGui>();
}
