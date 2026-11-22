#pragma once
#include "psp/types.h"
#include "video/renderer.h"
#include "entities/player.h"
#include "quantum/r4s_parser.h"
#include <string>

class Engine {
public:
    static Engine& Instance();

    bool Init(int argc, char* argv[]);
    void Run();
    void Shutdown();

    bool IsRunning() const { return m_Running; }
    void RequestExit() { m_Running = false; }

    Game::Player& GetPlayer() { return m_Player; }
    const Quantum::StratWad& GetStratWad() const { return m_StratWad; }

private:
    Engine() = default;
    ~Engine() = default;

    void MainLoop();  // Decompiled from 0x0008E298
    void Tick(float dt);
    void Render();
    void RenderDebugOverlay();

    bool m_Running = false;
    bool m_TestMode = false;
    u32  m_FrameCounter = 0;

    Camera3D m_Camera;
    std::string m_InitialLevel = "11_01";

    Game::Player m_Player;
    Quantum::StratWad m_StratWad;
};
