#include "explorer.hpp"
#include "switch/dmntcht.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ex {
namespace {

State g{};
DmntCheatProcessMetadata g_meta{};
bool g_dmntInitialized = false;
bool g_exactBuild = false;
u32 g_healthTicks = 0;

struct GameProfile {
    const char* version;
    const char* bid;
    u64 sceneModule;
};

constexpr GameProfile kGameProfiles[] = {
    {"1.4.0", "6265F94D606242CE", 0x2C02CC},
    {"1.4.1", "965EAB9CEB8EB867", 0x4D421C},
    {"1.4.2", "5CB42B1CF25469FB", 0x6570B0},
    {"1.4.3", "277178B7DBA1B6D4", 0x1AFBE4},
};
static_assert(sizeof(kGameProfiles) / sizeof(kGameProfiles[0]) == 4);

// Shared Player layout for 1.4.0-1.4.3.
// Scene -> components: +0x1E8 then +0x58.
// Resident actor manager: components[13].
// Resident manager: count +0x20, list +0x28.
// Resident link: stride 0x70, linkData +0x08, actor +0x40.
// Actor: name pointer +0x218, world position +0x2B4.
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

constexpr u32 EXACT_RETRY_TICKS = 30;
constexpr u32 HEALTH_CHECK_TICKS = 120;
constexpr float MAX_XZ = 12000.0f;
constexpr float MAX_Y = 6000.0f;

const GameProfile* g_gameProfile = nullptr;

void fail(const char* message) {
    g.stage = ScanStage::Failed;
    g.error = message ? message : "Unknown error";
    g.message = g.error;
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
    return address >= 0x1000000ULL &&
           address < 0x8000000000ULL &&
           (address & 0x7ULL) == 0;
}

Vec3 displayFromEngine(const Vec3& raw) {
    // Engine memory convention: X, vertical Y, world Z.
    // Explorer convention: X, world Y, height Z.
    return {raw.x, raw.z, raw.y};
}

const GameProfile* matchGameProfile() {
    constexpr char HEX[] = "0123456789ABCDEF";

    for (const auto& profile : kGameProfiles) {
        bool match = true;
        for (std::size_t i = 0; i < 8; ++i) {
            const u8 byte = g_meta.main_nso_build_id[i];
            if (HEX[(byte >> 4) & 0xF] != profile.bid[i * 2] ||
                HEX[byte & 0xF] != profile.bid[i * 2 + 1]) {
                match = false;
                break;
            }
        }

        if (match)
            return &profile;
    }

    return nullptr;
}

void updateBuildInfo() {
    constexpr char HEX[] = "0123456789ABCDEF";
    char bid[17]{};

    for (std::size_t i = 0; i < 8; ++i) {
        const u8 byte = g_meta.main_nso_build_id[i];
        bid[i * 2] = HEX[(byte >> 4) & 0xF];
        bid[i * 2 + 1] = HEX[byte & 0xF];
    }

    g.buildId = bid;
    g.gameVersion = g_gameProfile ? g_gameProfile->version : "unsupported";
}

void resetPlayerState() {
    g.playerActor = 0;
    g.playerValid = false;
    g.exactPlayer = false;
}

void resetProcessState(const char* message) {
    resetPlayerState();
    g.processId = 0;
    g.mainBase = 0;
    g.mainSize = 0;
    g.heapBase = 0;
    g.heapSize = 0;
    g_gameProfile = nullptr;
    g_exactBuild = false;
    g.buildIdMatched = false;
    g.gameVersion = "unknown";
    g.buildId = "—";
    g.stage = ScanStage::Failed;
    g.error = message ? message : "Game process unavailable.";
    g.message = g.error;
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
        g_meta.heap_extents.size < 0x1000) {
        return false;
    }

    const u64 previousPid = g.processId;
    const u64 previousMainBase = g.mainBase;
    const u64 previousHeapBase = g.heapBase;
    const GameProfile* previousGameProfile = g_gameProfile;
    const bool previousExactBuild = g_exactBuild;

    g.processId = g_meta.process_id;
    g.mainBase = g_meta.main_nso_extents.base;
    g.mainSize = g_meta.main_nso_extents.size;
    g.heapBase = g_meta.heap_extents.base;
    g.heapSize = g_meta.heap_extents.size;

    g_gameProfile = matchGameProfile();
    g.buildIdMatched = g_gameProfile != nullptr;
    g_exactBuild = g.buildIdMatched;
    updateBuildInfo();

    const bool processChanged =
        previousPid != 0 && previousPid != g.processId;

    const bool layoutChanged =
        previousMainBase != 0 &&
        (previousMainBase != g.mainBase ||
         previousHeapBase != g.heapBase);

    const bool buildChanged =
        previousGameProfile != g_gameProfile ||
        previousExactBuild != g_exactBuild;

    if (processChanged || layoutChanged || buildChanged) {
        resetPlayerState();

        if (g_exactBuild) {
            g.stage = ScanStage::Resolving;
            g.message = "Game process changed; resolving Player actor...";
        } else {
            g.stage = ScanStage::Failed;
            g.message = "Unsupported TOTK build. Exact coordinates unavailable.";
        }

        if (g_gameProfile) {
            char message[128]{};
            std::snprintf(
                message,
                sizeof(message),
                "Target process found; supported TOTK build %s matched.",
                g_gameProfile->version);
            logMessage(message);
        } else {
            logMessage("Target process found; unsupported TOTK build.");
        }
    }

    return true;
}

bool readMem(u64 address, void* out, std::size_t size) {
    if (!out || size == 0)
        return false;

    return R_SUCCEEDED(
        dmntchtReadCheatProcessMemory(address, out, size));
}

bool readU64(u64 address, u64& out) {
    return readMem(address, &out, sizeof(out)) &&
           plausibleAddress(out);
}

bool readVec3(u64 address, Vec3& out) {
    if (!readMem(address, &out, sizeof(out)))
        return false;

    return plausible(out);
}

bool readRemoteCString(u64 address, char* out, std::size_t capacity) {
    if (!out || capacity < 2 || !plausibleAddress(address))
        return false;

    std::memset(out, 0, capacity);

    if (R_FAILED(dmntchtReadCheatProcessMemory(
            address, out, capacity - 1))) {
        return false;
    }

    out[capacity - 1] = '\0';
    return std::memchr(out, '\0', capacity) != nullptr;
}

bool actorNameIs(u64 actor, const char* wanted) {
    if (!wanted)
        return false;

    u64 name = 0;
    if (!readU64(actor + ACTOR_NAME, name))
        return false;

    char buffer[64]{};
    if (!readRemoteCString(name, buffer, sizeof(buffer)))
        return false;

    return std::strcmp(buffer, wanted) == 0;
}

bool resolveExactPlayerActor(u64& actorOut) {
    actorOut = 0;

    if (!g_exactBuild || !g_gameProfile || g.mainBase == 0)
        return false;

    u64 sceneModule = 0;
    if (!readU64(g.mainBase + g_gameProfile->sceneModule, sceneModule))
        return false;

    u64 scene = 0;
    if (!readU64(sceneModule + SCENE_FROM_MODULE, scene))
        return false;

    u64 components = 0;
    if (!readU64(scene + SCENE_COMPONENTS, components))
        return false;

    u64 actorManager = 0;
    if (!readU64(
            components + sizeof(u64) * RESIDENT_COMPONENT_INDEX,
            actorManager)) {
        return false;
    }

    u32 count = 0;
    if (!readMem(actorManager + RESIDENT_COUNT, &count, sizeof(count)))
        return false;

    if (count == 0 || count > 256)
        return false;

    u64 list = 0;
    if (!readU64(actorManager + RESIDENT_LIST, list))
        return false;

    for (u32 index = 0; index < count; ++index) {
        const u64 link =
            list + static_cast<u64>(index) * RESIDENT_STRIDE;

        u64 descriptor = 0;
        if (!readU64(link + RESIDENT_DESCRIPTOR, descriptor))
            continue;

        u64 actor = 0;
        if (!readU64(
                descriptor + ACTOR_FROM_DESCRIPTOR,
                actor)) {
            continue;
        }

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
                g.player = displayFromEngine(value);
                g.playerValid = true;
                g.exactPlayer = true;
                g.stage = ScanStage::Ready;
                g.error.clear();
                g.message = "Exact Player actor coordinates active.";
                return true;
            }
        }

        // The cached actor is stale or temporarily unreadable. Do not walk the
        // entire resident roster from the render/update hot path.
        g.playerActor = 0;
        g.playerValid = false;
        g.exactPlayer = false;
        g.stage = ScanStage::Resolving;
        g.message = "Player actor lost; resolving again...";
        return false;
    }

    u64 actor = 0;
    if (!resolveExactPlayerActor(actor)) {
        g.playerValid = false;
        g.exactPlayer = false;
        g.stage = ScanStage::Resolving;
        return false;
    }

    Vec3 value{};
    if (!readVec3(actor + ACTOR_POSITION, value))
        return false;

    g.playerActor = actor;
    g.player = displayFromEngine(value);
    g.playerValid = true;
    g.exactPlayer = true;
    g.stage = ScanStage::Ready;
    g.error.clear();
    g.message = "Exact Player actor coordinates active.";

    {
        char message[96]{};
        std::snprintf(
            message,
            sizeof(message),
            "Exact %s Player actor resolved.",
            g_gameProfile ? g_gameProfile->version : "supported");
        logMessage(message);
    }

    return true;
}

} // namespace

State& state() {
    return g;
}

Result ensureMemory() {
    return initMemory();
}

Result initMemory() {
    if (g_dmntInitialized)
        return 0;

    Result rc = dmntchtInitialize();
    if (R_FAILED(rc)) {
        g.dmntReady = false;
        fail("dmnt:cht is unavailable.");
        return rc;
    }

    g_dmntInitialized = true;
    g.dmntReady = true;
    logMessage("dmnt:cht initialized.");

    auto cleanupAfterFailure = [&](const char* message, Result result) {
        if (g.attachedByUs) {
            dmntchtForceCloseCheatProcess();
            g.attachedByUs = false;
        }

        dmntchtExit();
        g_dmntInitialized = false;
        g.dmntReady = false;
        resetProcessState(message);
        fail(message);
        return result;
    };

    bool hasProcess = false;
    rc = dmntchtHasCheatProcess(&hasProcess);
    if (R_FAILED(rc))
        return cleanupAfterFailure(
            "Could not query the current game process.", rc);

    if (!hasProcess) {
        rc = dmntchtForceOpenCheatProcess();
        if (R_FAILED(rc))
            return cleanupAfterFailure(
                "Could not open the current game process.", rc);

        g.attachedByUs = true;
    }

    if (!findTargetProcess())
        return cleanupAfterFailure(
            "TOTK process not detected.", 1);

    if (!g_exactBuild) {
        fail("Unsupported TOTK build. Supported: 1.4.0-1.4.3.");
        return 0;
    }

    g.stage = ScanStage::Resolving;
    g.message = "Looking for exact Player actor...";

    if (!refreshExactPlayer(true))
        logMessage("Player actor not available yet; waiting for resolver retry.");

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
    resetProcessState("Game process unavailable.");
}

void startAutoScan() {
    g_healthTicks = 0;

    if (R_FAILED(initMemory()))
        return;

    if (!g_exactBuild) {
        fail("Unsupported TOTK build. Calibration supports 1.4.0-1.4.3 only.");
        return;
    }

    g.error.clear();

    // Idempotent restart: if Player is already resolved, never tear down the
    // valid actor just to perform the full resident-roster walk again.
    if (g.playerActor != 0 && refreshExactPlayer(false))
        return;

    g.stage = ScanStage::Resolving;
    g.message = "Looking for exact Player actor...";
    refreshExactPlayer(true);
}

void resetScan() {
    g_healthTicks = 0;
    resetPlayerState();
    g.error.clear();

    if (g_exactBuild) {
        g.stage = ScanStage::Resolving;
        g.message = "Looking for exact Player actor...";
    } else {
        g.stage = ScanStage::Idle;
        g.message = "Ready";
    }
}

void refreshPlayer() {
    if (!g_exactBuild) {
        g.playerValid = false;
        g.exactPlayer = false;
        g.playerActor = 0;
        return;
    }

    // Once an actor is resolved, the hot path only reads that actor's position.
    // If it disappears, refreshExactPlayer(false) transitions to Resolving; the
    // full roster lookup is then performed only by the 30-tick retry path.
    if (g.playerActor != 0) {
        refreshExactPlayer(false);
    } else {
        g.playerValid = false;
        g.exactPlayer = false;
        g.stage = ScanStage::Resolving;
        g.message = "Looking for exact Player actor...";
    }
}

void tick() {
    if (!g.dmntReady)
        return;

    ++g_healthTicks;

    if (g_healthTicks >= HEALTH_CHECK_TICKS) {
        g_healthTicks = 0;

        bool hasProcess = false;
        if (R_FAILED(dmntchtHasCheatProcess(&hasProcess)))
            hasProcess = false;

        if (!hasProcess) {
            g.attachedByUs = false;
            if (R_SUCCEEDED(dmntchtForceOpenCheatProcess())) {
                g.attachedByUs = true;
                logMessage("Re-attached to game process.");
            } else {
                resetProcessState("Game process unavailable.");
                return;
            }
        }

        if (!findTargetProcess()) {
            resetProcessState("Target process unavailable.");
            return;
        }

        if (g_exactBuild)
            refreshExactPlayer(true);
        else
            fail("Unsupported TOTK build. Exact coordinates unavailable.");
    }

    if (g.stage == ScanStage::Resolving &&
        g_exactBuild &&
        (g_healthTicks % EXACT_RETRY_TICKS) == 0) {
        if (!refreshExactPlayer(true))
            g.message = "Looking for exact Player actor...";
    } else if (g.stage == ScanStage::Ready) {
        refreshPlayer();
    }
}

const char* stageText(ScanStage stage) {
    switch (stage) {
        case ScanStage::Idle: return "Ready";
        case ScanStage::Resolving: return "Resolving Player actor";
        case ScanStage::Ready: return "Ready";
        case ScanStage::Failed: return "Failed";
        default: return "Unknown";
    }
}

std::string layerName(const Vec3& p) {
    if (p.z < -100.0f)
        return "Depths";
    return "Surface/Sky";
}

} // namespace ex
