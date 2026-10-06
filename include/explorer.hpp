#pragma once

#include <switch.h>
#include <cstdint>
#include <string>
#include <vector>

namespace ex {

inline constexpr u64 TITLE_ID = 0x0100F2C0115B6000ULL;
inline constexpr const char* TITLE_TEXT = "0100F2C0115B6000";
inline constexpr const char* BID_TEXT = "auto-detect (1.4.0-1.4.3)";
inline constexpr const char* GAME_VERSION = "1.4.0-1.4.3";
inline constexpr const char* VERSION = "3.6.2";

struct Vec3 { float x{}, y{}, z{}; };
struct Point { std::string type, name, layer; float x{}, y{}, z{}; };
struct Candidate { u64 address{}, heapOffset{}; Vec3 value{}; int score{}; };
struct Profile { bool valid{}; u64 offset{}; Vec3 value{}; int score{}; };

enum class ScanStage { Idle, Resolving, Ready, Failed };

struct State {
    ScanStage stage{ScanStage::Idle};
    std::string message{"Ready"};
    std::string error{};
    u64 processId{};
    u64 heapBase{};
    u64 heapSize{};
    Vec3 player{};
    bool playerValid{};
    bool exactPlayer{};
    bool buildIdMatched{};
    std::string gameVersion{"unknown"};
    std::string buildId{"—"};
    bool dmntReady{};
    bool attachedByUs{};
    u64 mainBase{};
    u64 mainSize{};
    u64 playerActor{};
    std::vector<Point> points{};
    std::size_t pointsRejected{};
};

State& state();
Result ensureMemory();
Result initMemory();
void shutdownMemory();
void tick();
void startAutoScan();
void resetScan();
void refreshPlayer();
void loadPoints();
void logMessage(const char* message);
const char* stageText(ScanStage stage);
std::string layerName(const Vec3& p);
std::vector<Point> nearby(float radius, std::size_t maxCount);

} // namespace ex
