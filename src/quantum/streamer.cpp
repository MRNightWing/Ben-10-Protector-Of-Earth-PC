#include "quantum/streamer.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace Quantum {

StreamManager& StreamManager::Instance() {
    static StreamManager s_Instance;
    return s_Instance;
}

bool StreamManager::LoadMapAndStrats(const std::string& levelId) {
    std::cout << "[StreamManager] Loading Map and Strats for: \"" << levelId << "\"..." << std::endl;

    // Search candidate paths
    std::vector<std::string> candidatePaths = {
        "assets/levels/" + levelId + "/" + levelId + ".r4m",
        "assets/PSP_GAME/USRDIR/" + levelId + ".r4m",
        "assets/" + levelId + ".r4m",
        levelId + ".r4m"
    };

    std::string foundPath;
    for (const auto& path : candidatePaths) {
        if (fs::exists(path)) {
            foundPath = path;
            break;
        }
    }

    if (foundPath.empty()) {
        std::cerr << "[StreamManager] Could not find .r4m file for level: " << levelId << std::endl;
        return false;
    }

    auto newMap = std::make_unique<LevelMap>();
    if (!ParseR4M(foundPath, *newMap)) {
        std::cerr << "[StreamManager] Failed to parse: " << foundPath << std::endl;
        return false;
    }

    m_ActiveMap = std::move(newMap);
    m_CurrentLevelId = levelId;
    std::cout << "[StreamManager] Successfully streamed level: " << m_ActiveMap->levelName << std::endl;
    return true;
}

bool StreamManager::StartWad(int wadId, const std::string& wadPath) {
    std::cout << "[StreamManager] Starting Wad " << wadId << ": \"" << wadPath << "\"" << std::endl;
    return true;
}

void StreamManager::StopWad(int wadId) {
    std::cout << "[StreamManager] Stopping Wad " << wadId << std::endl;
}

extern "C" {

int CStreamManager_LoadMapAndStrats(const char* mapName) {
    if (!mapName) return 0;
    return StreamManager::Instance().LoadMapAndStrats(mapName) ? 1 : 0;
}

int CStreamManager_StartWad(int wadId, const char* name) {
    if (!name) return 0;
    return StreamManager::Instance().StartWad(wadId, name) ? 1 : 0;
}

int CStreamManager_StopWad(int wadId) {
    StreamManager::Instance().StopWad(wadId);
    return 1;
}

} // extern "C"

} // namespace Quantum
