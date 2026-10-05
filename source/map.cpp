#include "explorer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ex {

namespace {

bool sameLayer(const Point& point, const Vec3& player) {
    if (point.layer.empty())
        return true;

    // Depths can be separated safely by height. Surface and Sky overlap in
    // altitude, so keep both available outside the Depths instead of guessing.
    if (player.y < -100.0f)
        return point.layer == "Depths";

    return point.layer != "Depths";
}

} // namespace

void loadPoints() {
    state().points.clear();
    state().pointsRejected = 0;

    FILE* file = std::fopen("sdmc:/switch/totk_explorer/points.csv", "rb");
    if (!file) {
        logMessage("points.csv not found");
        return;
    }

    char line[512]{};
    while (std::fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char type[64]{};
        char name[192]{};
        char layer[32]{};
        float x = 0.0f, y = 0.0f, z = 0.0f;

        const int parsed = std::sscanf(
            line,
            "%63[^,],%191[^,],%f,%f,%f,%31[^\r\n]",
            type, name, &x, &y, &z, layer);

        if (parsed == 5) {
            state().points.push_back(Point{type, name, "", x, y, z});
        } else if (parsed == 6) {
            state().points.push_back(Point{type, name, layer, x, y, z});
        } else {
            ++state().pointsRejected;
        }
    }

    std::fclose(file);
    logMessage("points.csv loaded");
}

std::vector<Point> nearby(float radius, std::size_t maxCount) {
    std::vector<Point> result;
    if (!state().playerValid)
        return result;

    struct DistancePoint {
        float distance;
        Point point;
    };

    std::vector<DistancePoint> distances;
    distances.reserve(state().points.size());
    const float radiusSquared = radius * radius;

    for (const auto& point : state().points) {
        if (!sameLayer(point, state().player))
            continue;

        const float dx = point.x - state().player.x;
        const float dy = point.y - state().player.y;
        const float dz = point.z - state().player.z;
        const float d2 = dx * dx + dz * dz + 0.25f * dy * dy;

        if (d2 <= radiusSquared)
            distances.push_back({std::sqrt(d2), point});
    }

    std::sort(distances.begin(), distances.end(), [](const DistancePoint& a, const DistancePoint& b) {
        return a.distance < b.distance;
    });

    for (std::size_t i = 0; i < distances.size() && i < maxCount; ++i)
        result.push_back(distances[i].point);

    return result;
}

} // namespace ex
