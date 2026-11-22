#include "core/engine.h"
#include <cstdio>

int main(int argc, char* argv[]) {
    Engine& engine = Engine::Instance();

    if (!engine.Init(argc, argv)) {
        fprintf(stderr, "Failed to initialize Ben 10 Engine HAL.\n");
        return 1;
    }

    engine.Run();
    engine.Shutdown();

    return 0;
}
