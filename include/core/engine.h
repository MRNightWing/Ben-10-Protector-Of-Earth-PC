#pragma once
#include "psp/types.h"
#include "video/renderer.h"
#include <string>

class Engine {
public:
    static Engine& Instance();

    bool Init(int argc, char* argv[]);
    void Run();
    void Shutdown();

    bool IsRunning() const { return m_Running; }
    void RequestExit() { m_Running = false; }

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
};
