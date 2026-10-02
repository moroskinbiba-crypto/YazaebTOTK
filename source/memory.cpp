#include "explorer.hpp"
#include "switch/dmntcht.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ex {
namespace {

State g{};
DmntCheatProcessMetadata g_meta{};
bool g_dmntInitialized = false;

// Keep per-frame memory bounded. Candidates are reservoir-sampled so the
// scanner does not stop at the first few megabytes of heap noise.
constexpr u64 SCAN_CHUNK = 0x80000; // 512 KiB per overlay update.
constexpr std::size_t MAX_CANDIDATES = 4096;
constexpr float MAX_XZ = 12000.0f;
constexpr float MAX_Y = 6000.0f;
constexpr float MOVE_EPS = 0.75f;
constexpr float JUMP_EPS = 2.0f;
constexpr float MAX_STEP = 1500.0f;

u64 rngState = 0x9E3779B97F4A7C15ULL;

const char* profilePath() {
    return "sdmc:/switch/totk_explorer/profile.txt";
}

bool finiteCoord(float v) {
    return std::isfinite(v);
}

bool plausible(Vec3 p) {
    if (!finiteCoord(p.x) || !finiteCoord(p.y) || !finiteCoord(p.z))
        return false;
    if (std::fabs(p.x) > MAX_XZ || std::fabs(p.z) > MAX_XZ || std::fabs(p.y) > MAX_Y)
        return false;
    if (std::fabs(p.x) < 0.01f && std::fabs(p.y) < 0.01f && std::fabs(p.z) < 0.01f)
        return false;
    return true;
}

u64 nextRandom() {
    u64 x = rngState;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rngState = x;
    return x;
}

void fail(const char* message) {
    g.stage = ScanStage::Failed;
    g.error = message;
    g.message = message;
}

bool findTargetProcess() {
    if (R_FAILED(dmntchtGetCheatProcessMetadata(&g_meta)))
        return false;

    if (g_meta.title_id != TITLE_ID)
        return false;

    if (g_meta.process_id == 0 || g_meta.heap_extents.base == 0 || g_meta.heap_extents.size < 0x1000)
        return false;

    g.processId = g_meta.process_id;
    g.heapBase = g_meta.heap_extents.base;
    g.heapSize = g_meta.heap_extents.size;
    return true;
}

bool readVec3(u64 address, Vec3& out) {
    if (R_FAILED(dmntchtReadCheatProcessMemory(address, &out, sizeof(out))))
        return false;
    return plausible(out);
}

bool readProfileFile(Profile& profile) {
    FILE* file = std::fopen(profilePath(), "rb");
    if (!file)
        return false;

    unsigned long long offset = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    int score = 0;
    const int count = std::fscanf(file, "%llx %f %f %f %d", &offset, &x, &y, &z, &score);
    std::fclose(file);

    if (count != 5 || offset >= g.heapSize)
        return false;
    if (!plausible({x, y, z}))
        return false;
    profile = Profile{true, static_cast<u64>(offset), {x, y, z}, score};
    return true;
}

void addReservoirCandidate(u64 address, u64 heapOffset, Vec3 value) {
    ++g.candidatesSeen;
    Candidate candidate{address, heapOffset, value, 0};

    if (g.candidatesList.size() < MAX_CANDIDATES) {
        g.candidatesList.push_back(candidate);
        return;
    }

    const u64 slot = nextRandom() % g.candidatesSeen;
    if (slot < MAX_CANDIDATES)
        g.candidatesList[static_cast<std::size_t>(slot)] = candidate;
}

void scanChunk() {
    if (g.cursor >= g.heapSize) {
        g.stage = g.candidatesList.empty() ? ScanStage::Failed : ScanStage::WaitMove;
        g.candidates = g.candidatesList.size();
        g.message = g.candidatesList.empty()
            ? "No candidates found. Run Auto Discovery again."
            : "Scan complete. Walk Link, then press X.";
        return;
    }

    const u64 remaining = g.heapSize - g.cursor;
    const std::size_t bytes = static_cast<std::size_t>(std::min<u64>(SCAN_CHUNK, remaining));
    if (bytes < sizeof(Vec3)) {
        g.cursor = g.heapSize;
        return;
    }

    static std::vector<u8> buffer;
    buffer.resize(bytes);
    if (R_FAILED(dmntchtReadCheatProcessMemory(g.heapBase + g.cursor, buffer.data(), bytes))) {
        g.cursor += bytes;
        g.scanned += bytes;
        return;
    }

    for (std::size_t i = 0; i + sizeof(Vec3) <= bytes; i += 4) {
        Vec3 value{};
        std::memcpy(&value.x, buffer.data() + i, sizeof(float));
        std::memcpy(&value.y, buffer.data() + i + 4, sizeof(float));
        std::memcpy(&value.z, buffer.data() + i + 8, sizeof(float));

        if (!plausible(value))
            continue;

        addReservoirCandidate(g.heapBase + g.cursor + i, g.cursor + i, value);
    }

    g.cursor += bytes;
    g.scanned += bytes;
    g.candidates = g.candidatesList.size();

    if (g.cursor >= g.heapSize) {
        g.stage = g.candidatesList.empty() ? ScanStage::Failed : ScanStage::WaitMove;
        g.message = g.candidatesList.empty()
            ? "No candidates found. Run Auto Discovery again."
            : "Scan complete. Walk Link, then press X.";
    } else {
        g.message = "Scanning game memory...";
    }
}

void filterMove() {
    std::vector<Candidate> filtered;
    filtered.reserve(g.candidatesList.size());

    for (const Candidate& candidate : g.candidatesList) {
        Vec3 current{};
        if (!readVec3(candidate.address, current))
            continue;

        const float dx = current.x - candidate.value.x;
        const float dz = current.z - candidate.value.z;
        const float horizontal = std::sqrt(dx * dx + dz * dz);

        if (horizontal < MOVE_EPS || horizontal > MAX_STEP)
            continue;

        Candidate updated = candidate;
        updated.value = current;
        updated.score += 3;
        filtered.push_back(updated);
    }

    g.candidatesList.swap(filtered);
    g.candidates = g.candidatesList.size();
}

void filterJump() {
    std::vector<Candidate> filtered;
    filtered.reserve(g.candidatesList.size());

    for (const Candidate& candidate : g.candidatesList) {
        Vec3 current{};
        if (!readVec3(candidate.address, current))
            continue;

        const float dx = current.x - candidate.value.x;
        const float dy = current.y - candidate.value.y;
        const float dz = current.z - candidate.value.z;
        const float horizontal = std::sqrt(dx * dx + dz * dz);

        if (std::fabs(dy) < JUMP_EPS || std::fabs(dy) > MAX_STEP)
            continue;
        if (horizontal > MAX_STEP)
            continue;

        Candidate updated = candidate;
        updated.value = current;
        updated.score += 5;
        filtered.push_back(updated);
    }

    g.candidatesList.swap(filtered);
    g.candidates = g.candidatesList.size();
}

void selectBest() {
    if (g.candidatesList.empty()) {
        fail("No stable coordinate candidate. Run Auto Discovery again.");
        return;
    }

    const auto best = std::max_element(
        g.candidatesList.begin(), g.candidatesList.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.score < b.score;
        });

    g.profile = Profile{true, best->heapOffset, best->value, best->score};
    g.player = best->value;
    g.playerValid = true;
    saveProfile();
    g.stage = ScanStage::Ready;
    g.message = "Coordinate candidate selected and saved.";
}

} // namespace

State& state() {
    return g;
}

Result initMemory() {
    if (g_dmntInitialized)
        return 0;

    Result rc = dmntchtInitialize();
    if (R_FAILED(rc)) {
        fail("dmnt:cht is unavailable.");
        return rc;
    }

    g_dmntInitialized = true;
    g.dmntReady = true;

    bool hasProcess = false;
    if (R_SUCCEEDED(dmntchtHasCheatProcess(&hasProcess)) && !hasProcess) {
        rc = dmntchtForceOpenCheatProcess();
        if (R_FAILED(rc)) {
            fail("Could not open the current game process.");
            return rc;
        }
        g.attachedByUs = true;
    }

    if (!findTargetProcess()) {
        if (g.attachedByUs) {
            dmntchtForceCloseCheatProcess();
            g.attachedByUs = false;
        }
        fail("TOTK 1.4.3 process not detected.");
        return 1;
    }

    loadProfile();
    if (g.profile.valid) {
        refreshPlayer();
        if (g.playerValid) {
            g.stage = ScanStage::Ready;
            g.message = "Saved profile loaded. Use Auto Discovery if coordinates are wrong.";
        }
    }

    return 0;
}

void shutdownMemory() {
    if (g.attachedByUs) {
        dmntchtForceCloseCheatProcess();
        g.attachedByUs = false;
    }

    if (g_dmntInitialized) {
        dmntchtExit();
        g_dmntInitialized = false;
    }

    g.dmntReady = false;
}

void startAutoScan() {
    if (!g.dmntReady && R_FAILED(initMemory()))
        return;

    g.candidatesList.clear();
    g.candidatesList.reserve(MAX_CANDIDATES);
    g.candidates = 0;
    g.candidatesSeen = 0;
    g.cursor = 0;
    g.scanned = 0;
    g.profile.valid = false;
    g.playerValid = false;
    g.error.clear();
    rngState = 0x9E3779B97F4A7C15ULL ^ g.heapBase;
    g.stage = ScanStage::Scanning;
    g.message = "Scanning game memory...";
}

void captureMove() {
    if (g.stage != ScanStage::WaitMove)
        return;

    filterMove();
    if (g.candidatesList.empty()) {
        fail("No moving candidates. Walk farther and restart the scan.");
        return;
    }

    g.stage = ScanStage::WaitJump;
    g.message = "Now jump or change elevation, then press X.";
}

void captureJump() {
    if (g.stage != ScanStage::WaitJump)
        return;

    filterJump();
    selectBest();
}

void resetScan() {
    g.stage = ScanStage::Idle;
    g.message = "Ready";
    g.error.clear();
    g.candidatesList.clear();
    g.candidates = 0;
    g.candidatesSeen = 0;
    g.cursor = 0;
    g.scanned = 0;
    g.playerValid = false;
}

void refreshPlayer() {
    if (!g.profile.valid || g.heapBase == 0 || g.profile.offset >= g.heapSize) {
        g.playerValid = false;
        return;
    }

    Vec3 value{};
    if (!readVec3(g.heapBase + g.profile.offset, value)) {
        g.playerValid = false;
        return;
    }

    g.player = value;
    g.playerValid = true;
}

void tick() {
    if (!g.dmntReady)
        return;

    if (g.stage == ScanStage::Scanning)
        scanChunk();
    else if (g.stage == ScanStage::Ready)
        refreshPlayer();
}

void saveProfile() {
    if (!g.profile.valid)
        return;

    FILE* file = std::fopen(profilePath(), "wb");
    if (!file)
        return;

    std::fprintf(
        file,
        "%llx %.7g %.7g %.7g %d\n",
        static_cast<unsigned long long>(g.profile.offset),
        static_cast<double>(g.profile.value.x),
        static_cast<double>(g.profile.value.y),
        static_cast<double>(g.profile.value.z),
        g.profile.score);

    std::fclose(file);
}

void loadProfile() {
    Profile profile{};
    if (readProfileFile(profile))
        g.profile = profile;
}

const char* stageText(ScanStage stage) {
    switch (stage) {
        case ScanStage::Idle: return "Ready";
        case ScanStage::Scanning: return "Scanning";
        case ScanStage::WaitMove: return "Walk Link / press X";
        case ScanStage::WaitJump: return "Jump / press X";
        case ScanStage::Ready: return "Ready";
        case ScanStage::Failed: return "Failed";
        default: return "Unknown";
    }
}

std::string layerName(const Vec3& p) {
    if (p.y > 500.0f) return "Sky";
    if (p.y < -100.0f) return "Depths";
    return "Surface";
}

} // namespace ex
