#include <tesla.hpp>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "explorer.hpp"

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

class ScanGui final : public tsl::Gui {
    tsl::elm::ListItem* stageItem{};
    tsl::elm::ListItem* statusItem{};
    tsl::elm::ListItem* progressItem{};
    tsl::elm::ListItem* candidatesItem{};
    tsl::elm::ListItem* coordsItem{};
    tsl::elm::ListItem* profileItem{};

    void refresh() {
        const auto& state = ex::state();
        stageItem->setValue(ex::stageText(state.stage));
        statusItem->setValue(state.message);

        if (state.heapSize > 0) {
            const double pct = 100.0 * static_cast<double>(state.scanned) / static_cast<double>(state.heapSize);
            char progress[48]{};
            std::snprintf(progress, sizeof(progress), "%.1f%%", pct);
            progressItem->setValue(progress);
        } else {
            progressItem->setValue("0.0%");
        }

        candidatesItem->setValue(std::to_string(state.candidates));
        coordsItem->setValue(state.playerValid
            ? f2(state.player.x) + " / " + f2(state.player.y) + " / " + f2(state.player.z)
            : "—");
        profileItem->setValue(state.profile.valid ? "yes" : "no");
    }

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", "Auto Discovery");
        auto* list = new tsl::elm::List();

        list->addItem(new tsl::elm::ListItem("Exact resolver for supported 1.4.x builds; fallback scan if needed"));
        stageItem = new tsl::elm::ListItem("Stage");
        statusItem = new tsl::elm::ListItem("Status");
        progressItem = new tsl::elm::ListItem("Scan progress");
        candidatesItem = new tsl::elm::ListItem("Candidates");
        coordsItem = new tsl::elm::ListItem("X / Y / Z");
        profileItem = new tsl::elm::ListItem("Saved profile");
        list->addItem(stageItem);
        list->addItem(statusItem);
        list->addItem(progressItem);
        list->addItem(candidatesItem);
        list->addItem(coordsItem);
        list->addItem(profileItem);

        auto* start = new tsl::elm::ListItem("Resolve Coordinates");
        start->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                ex::startAutoScan();
                return true;
            }
            return false;
        });
        list->addItem(start);

        list->addItem(new tsl::elm::ListItem("Calibration"));
        list->addItem(new tsl::elm::ListItem("Fallback scan", "Used only if exact resolver fails"));
        list->addItem(new tsl::elm::ListItem("Move", "If fallback scan is active"));
        list->addItem(new tsl::elm::ListItem("Jump", "If fallback scan is active"));

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

class MapGui final : public tsl::Gui {
    static constexpr std::size_t GRID = 9;
    static constexpr std::size_t NEARBY = 8;
    std::array<tsl::elm::ListItem*, GRID> rows{};
    tsl::elm::ListItem* layerItem{};
    tsl::elm::ListItem* buildIdItem{};
    tsl::elm::ListItem* sourceItem{};
    tsl::elm::ListItem* posItem{};
    std::array<tsl::elm::ListItem*, NEARBY> nearbyItems{};

    static char marker(const ex::Point& point) {
        if (point.type == "Shrine") return 'S';
        if (point.type == "Korok") return 'K';
        if (point.type == "Lightroot") return 'L';
        if (point.type == "Tower") return 'T';
        if (point.type == "Cave") return 'C';
        if (point.type == "Chasm") return 'H';
        return '*';
    }

    static std::string mapRow(const ex::Vec3& player, const std::vector<ex::Point>& points, int row) {
        std::string cells(GRID, '.');
        cells[GRID / 2] = '@';
        constexpr float CELL_SIZE = 250.0f;

        for (const auto& point : points) {
            const float dx = point.x - player.x;
            const float dy = point.y - player.y;
            const int gx = static_cast<int>(std::lround(dx / CELL_SIZE)) + static_cast<int>(GRID / 2);
            const int gy = static_cast<int>(std::lround(dy / CELL_SIZE)) + static_cast<int>(GRID / 2);
            if (gx < 0 || gx >= static_cast<int>(GRID) || gy < 0 || gy >= static_cast<int>(GRID) || gy != row)
                continue;
            cells[static_cast<std::size_t>(gx)] = marker(point);
        }
        return cells;
    }

    void refresh() {
        const auto& state = ex::state();
        layerItem->setValue(state.playerValid ? ex::layerName(state.player) : "Not discovered");
        posItem->setValue(state.playerValid
            ? f2(state.player.x) + ", " + f2(state.player.y) + ", " + f2(state.player.z)
            : "—");

        if (state.playerValid) {
            for (std::size_t i = 0; i < GRID; ++i)
                rows[i]->setValue(mapRow(state.player, state.points, static_cast<int>(i)));

            const auto nearbyPoints = ex::nearby(1000.0f, NEARBY);
            for (std::size_t i = 0; i < NEARBY; ++i) {
                if (i < nearbyPoints.size())
                    nearbyItems[i]->setValue(nearbyPoints[i].type + " · " + nearbyPoints[i].name + "  " + distanceText(state.player, nearbyPoints[i]));
                else
                    nearbyItems[i]->setValue("");
            }
        } else {
            for (auto* row : rows) row->setValue("—");
            for (auto* item : nearbyItems) item->setValue("");
        }
    }

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", "Dynamic Map");
        auto* list = new tsl::elm::List();

        layerItem = new tsl::elm::ListItem("Layer");
        posItem = new tsl::elm::ListItem("Position");
        list->addItem(layerItem);
        list->addItem(posItem);
        list->addItem(new tsl::elm::ListItem("Local map — @ Link  S Shrine  K Korok  L Lightroot  T Tower  C Cave  H Chasm"));

        for (std::size_t i = 0; i < GRID; ++i) {
            rows[i] = new tsl::elm::ListItem("Map " + std::to_string(i + 1));
            list->addItem(rows[i]);
        }

        list->addItem(new tsl::elm::ListItem("Nearby"));
        for (std::size_t i = 0; i < NEARBY; ++i) {
            nearbyItems[i] = new tsl::elm::ListItem("Point " + std::to_string(i + 1));
            list->addItem(nearbyItems[i]);
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

class InfoGui final : public tsl::Gui {
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
        buildIdItem = new tsl::elm::ListItem("Build ID match");
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
    tsl::elm::ListItem* memoryItem{};
    tsl::elm::ListItem* coordsItem{};

public:
    tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("TOTK EXPLORER", ex::VERSION);
        auto* list = new tsl::elm::List();

        list->addItem(new tsl::elm::ListItem("Tears of the Kingdom 1.4.0-1.4.3"));
        list->addItem(new tsl::elm::ListItem("BID", "auto-detect"));
        memoryItem = new tsl::elm::ListItem("Memory");
        coordsItem = new tsl::elm::ListItem("Coordinates");
        list->addItem(memoryItem);
        list->addItem(coordsItem);

        auto* scan = new tsl::elm::ListItem("Player Coordinates");
        scan->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<ScanGui>();
                return true;
            }
            return false;
        });
        list->addItem(scan);

        auto* map = new tsl::elm::ListItem("Dynamic Map");
        map->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<MapGui>();
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

        auto* info = new tsl::elm::ListItem("Build / Diagnostics");
        info->setClickListener([](u64 keys) {
            if (keys & KEY_A) {
                tsl::changeTo<InfoGui>();
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
        memoryItem->setValue(state.dmntReady ? "dmnt:cht connected" : "not connected");
        coordsItem->setValue(state.playerValid
            ? f2(state.player.x) + " / " + f2(state.player.y) + " / " + f2(state.player.z)
            : "Not discovered");
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
