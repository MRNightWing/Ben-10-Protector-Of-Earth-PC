#pragma once
#include "quantum/r4m_parser.h"
#include <string>
#include <memory>

namespace Quantum {

class StreamManager {
public:
    static StreamManager& Instance();

    // Reconstructing CStreamManager_LoadMapAndStrats @ 0x00081F5C
    bool LoadMapAndStrats(const std::string& levelId);

    // CStreamManager_StartWad @ 0x0007FB14
    bool StartWad(int wadId, const std::string& wadPath);

    // CStreamManager_StopWad @ 0x0007FD20
    void StopWad(int wadId);

    const LevelMap* GetCurrentMap() const { return m_ActiveMap.get(); }
    bool IsMapLoaded() const { return m_ActiveMap != nullptr; }

    const std::string& GetCurrentLevelId() const { return m_CurrentLevelId; }

private:
    StreamManager() = default;
    ~StreamManager() = default;

    std::string m_CurrentLevelId;
    std::unique_ptr<LevelMap> m_ActiveMap;
};

// C-style function wrappers matching MIPS EBOOT function signatures
extern "C" {
    int CStreamManager_LoadMapAndStrats(const char* mapName);
    int CStreamManager_StartWad(int wadId, const char* name);
    int CStreamManager_StopWad(int wadId);
}

} // namespace Quantum
