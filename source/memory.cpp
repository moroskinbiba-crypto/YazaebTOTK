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
bool g_exactBuild = false;
u32 g_healthTicks = 0;

// Public 1.4.3 game-layout profile used by the exact actor resolver.
// Scene-module singleton: main + 0x1AFBE4.
// Scene -> components: +0x1E8 then +0x58.
// Resident actor manager: components[13].
// Resident manager: count +0x20, list +0x28.
// Resident link: stride 0x70, linkData +0x08, actor +0x40.
// Actor: name pointer +0x218, world position +0x2B4.
constexpr u64 TOTK_143_SCENE_MODULE = 0x1AFBE4;
constexpr u64 SCENE_FROM_MODULE = 0x1E8;
constexpr u64 SCENE_COMPONENTS = 0x58;
constexpr u64 RESIDENT_COMPONENT_INDEX = 13;
constexpr u64 RESIDENT_COUNT = 0x20;
constexpr u64 RESIDENT_LIST = 0x28;
constexpr u64 RESIDENT_STRIDE = 0x70;
constexpr u64 RESIDENT_DESCRIPTOR = 0x08;
constexpr u64 ACTOR_FROM_DESCRIPTOR = 0x40;
constexpr u64 ACTOR_NAME = 0x218;
constexpr u64 ACTOR_POSITION = 0x2B4;

// Heuristic fallback scanner.
constexpr u64 SCAN_CHUNK = 0x80000;
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

bool plausibleAddress(u64 address) {
    return address >= 0x1000000ULL && address < 0x8000000000ULL && (address & 0x7ULL) == 0;
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

bool buildIdMatches143() {
    // Atmosphere exposes a 0x20-byte NSO build ID. The cheat/BID convention
    // used by TOTK stores the first 8 bytes as the 16-hex-digit BID.
    constexpr char HEX[] = "0123456789ABCDEF";
    constexpr char BID[] = "277178B7DBA1B6D4";
    for (std::size_t i = 0; i < 8; ++i) {
        const u8 byte = g_meta.main_nso_build_id[i];
        if (HEX[(byte >> 4) & 0xF] != BID[i * 2] ||
            HEX[byte & 0xF] != BID[i * 2 + 1])
            return false;
    }
    return true;
}

bool findTargetProcess() {
    if (R_FAILED(dmntchtGetCheatProcessMetadata(&g_meta)))
        return false;

    if (g_meta.title_id != TITLE_ID)
        return false;

    if (g_meta.process_id == 0 ||
        g_meta.main_nso_extents.base == 0 ||
        g_meta.main_nso_extents.size < 0x1000 ||
        g_meta.heap_extents.base == 0 ||
        g_meta.heap_extents.size < 0x1000)
        return false;

    const u64 previousPid = g.processId;
    g.processId = g_meta.process_id;
    if (previousPid != 0 && previousPid != g.processId) {
        g.playerActor = 0;
        g.playerValid = false;
        logMessage("Game process changed; refreshing Player actor.");
    }
    g.mainBase = g_meta.main_nso_extents.base;
    g.mainSize = g_meta.main_nso_extents.size;
    g.heapBase = g_meta.heap_extents.base;
    g.heapSize = g_meta.heap_extents.size;
    g.buildIdMatched = buildIdMatches143();
    g_exactBuild = g.buildIdMatched;
    logMessage(g.buildIdMatched
        ? "Target process found; Build ID matched 1.4.3."
        : "Target process found; Build ID mismatch.");
    return true;
}

bool readMem(u64 address, void* out, std::size_t size) {
    if (!out || size == 0)
        return false;
    return R_SUCCEEDED(dmntchtReadCheatProcessMemory(address, out, size));
}

bool readU64(u64 address, u64& out) {
    return readMem(address, &out, sizeof(out)) && plausibleAddress(out);
}

bool readVec3(u64 address, Vec3& out) {
    if (!readMem(address, &out, sizeof(out)))
        return false;
    return plausible(out);
}

bool readRemoteCString(u64 address, char* out, std::size_t capacity) {
    if (!out || capacity == 0 || !plausibleAddress(address))
        return false;
    std::memset(out, 0, capacity);
    if (R_FAILED(dmntchtReadCheatProcessMemory(address, out, capacity - 1)))
        return false;
    out[capacity - 1] = '\0';
    return std::memchr(out, '\0', capacity) != nullptr;
}

bool actorNameIs(u64 actor, const char* wanted) {
    u64 name = 0;
    if (!readU64(actor + ACTOR_NAME, name))
        return false;

    char buffer[64]{};
    if (!readRemoteCString(name, buffer, sizeof(buffer)))
        return false;
    return std::strncmp(buffer, wanted, sizeof(buffer)) == 0;
}

bool resolveExactPlayerActor(u64& actorOut) {
    actorOut = 0;
    if (!g_exactBuild || g.mainBase == 0)
        return false;

    u64 sceneModule = 0;
    if (!readU64(g.mainBase + TOTK_143_SCENE_MODULE, sceneModule))
        return false;

    u64 scene = 0;
    if (!readU64(sceneModule + SCENE_FROM_MODULE, scene))
        return false;

    u64 components = 0;
    if (!readU64(scene + SCENE_COMPONENTS, components))
        return false;

    u64 actorManager = 0;
    if (!readU64(components + sizeof(u64) * RESIDENT_COMPONENT_INDEX, actorManager))
        return false;

    u32 count = 0;
    if (!readMem(actorManager + RESIDENT_COUNT, &count, sizeof(count)))
        return false;
    if (count == 0 || count > 512)
        return false;

    u64 list = 0;
    if (!readU64(actorManager + RESIDENT_LIST, list))
        return false;

    for (u32 index = 0; index < count; ++index) {
        const u64 link = list + static_cast<u64>(index) * RESIDENT_STRIDE;

        u64 descriptor = 0;
        if (!readU64(link + RESIDENT_DESCRIPTOR, descriptor))
            continue;

        u64 actor = 0;
        if (!readU64(descriptor + ACTOR_FROM_DESCRIPTOR, actor))
            continue;

        if (actorNameIs(actor, "Player")) {
            actorOut = actor;
            return true;
        }
    }

    return false;
}

bool refreshExactPlayer(bool validateActor) {
    if (!g_exactBuild)
        return false;

    if (g.playerActor != 0) {
        if (!validateActor || actorNameIs(g.playerActor, "Player")) {
            Vec3 value{};
            if (readVec3(g.playerActor + ACTOR_POSITION, value)) {
                g.player = value;
                g.playerValid = true;
                g.exactPlayer = true;
                return true;
            }
        }
    }

    u64 actor = 0;
    if (!resolveExactPlayerActor(actor))
        return false;

    Vec3 value{};
    if (!readVec3(actor + ACTOR_POSITION, value))
        return false;

    g.playerActor = actor;
    g.player = value;
    g.playerValid = true;
    g.exactPlayer = true;
    logMessage("Exact 1.4.3 Player actor resolved.");
    return true;
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
        const float score = std::min(horizontal, 100.0f) / 100.0f;
        updated.score += 3 + static_cast<int>(score * 2.0f);
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
        const float verticalScore = std::min(std::fabs(dy), 100.0f) / 100.0f;
        updated.score += 5 + static_cast<int>(verticalScore * 2.0f);
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
    g.exactPlayer = false;
    g.playerActor = 0;
    saveProfile();
    g.stage = ScanStage::Ready;
    g.message = "Heuristic coordinate candidate selected and saved.";
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
    logMessage("dmnt:cht initialized.");

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

    if (g_exactBuild && refreshExactPlayer(true)) {
        g.stage = ScanStage::Ready;
        g.message = "Exact 1.4.3 Player actor coordinates active.";
        return 0;
    }

    loadProfile();
    if (g.profile.valid) {
        refreshPlayer();
        if (g.playerValid) {
            g.stage = ScanStage::Ready;
            g.message = g.buildIdMatched
                ? "Exact actor not resolved; using saved heuristic profile."
                : "Build ID mismatch; using saved heuristic profile.";
        }
    } else {
        g.message = g.buildIdMatched
            ? "Build ID matched, but Player actor was not resolved."
            : "Target title found, but Build ID did not match 1.4.3.";
    }

    return 0;
}

void shutdownMemory() {
    g_healthTicks = 0;
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
    g_healthTicks = 0;
    if (!g.dmntReady && R_FAILED(initMemory()))
        return;

    // Exact 1.4.3 resolver is always preferred. The fallback scanner should
    // only run when the exact Player actor cannot currently be resolved.
    if (g_exactBuild && refreshExactPlayer(true)) {
        g.stage = ScanStage::Ready;
        g.message = "Exact 1.4.3 Player actor coordinates active.";
        return;
    }

    g.candidatesList.clear();
    g.candidatesList.reserve(MAX_CANDIDATES);
    g.candidates = 0;
    g.candidatesSeen = 0;
    g.cursor = 0;
    g.scanned = 0;
    g.error.clear();
    rngState = 0x9E3779B97F4A7C15ULL ^ g.heapBase;
    g.stage = ScanStage::Scanning;
    g.message = "Scanning game memory (heuristic fallback)...";
    g.exactPlayer = false;
    g.playerActor = 0;
    g.playerValid = false;
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
    g_healthTicks = 0;
    g.stage = ScanStage::Idle;
    g.message = "Ready";
    g.error.clear();
    g.candidatesList.clear();
    g.candidates = 0;
    g.candidatesSeen = 0;
    g.cursor = 0;
    g.scanned = 0;
    g.playerValid = false;
    g.exactPlayer = false;
    g.playerActor = 0;
}

void refreshPlayer() {
    if (g_exactBuild && refreshExactPlayer(false)) {
        g.stage = ScanStage::Ready;
        g.message = "Exact 1.4.3 Player actor coordinates active.";
        return;
    }

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
    g.exactPlayer = false;
    g.playerActor = 0;
}

void tick() {
    if (!g_dmntReady)
        return;

    ++g_healthTicks;
    if (g_healthTicks >= 120) {
        g_healthTicks = 0;

        bool hasProcess = false;
        if (R_FAILED(dmntchtHasCheatProcess(&hasProcess)))
            hasProcess = false;

        if (!hasProcess) {
            if (R_SUCCEEDED(dmntchtForceOpenCheatProcess())) {
                g.attachedByUs = true;
                logMessage("Re-attached to game process.");
            } else {
                g.playerValid = false;
                g.message = "Game process unavailable.";
                return;
            }
        }

        if (!findTargetProcess()) {
            g.playerValid = false;
            g.playerActor = 0;
            g.message = "Target process unavailable.";
            return;
        }
    }

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