// Entry point. The browser drives the loop through requestAnimationFrame; desktop runs it directly.
#include "app/app.h"

#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {
hc::App* g_app = nullptr;
void tick() { g_app->frame(); }
}   // namespace

int main(int argc, char** argv) {
    static hc::App app;
    g_app = &app;
    if (!app.init(argc, argv)) return 1;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (app.running) tick();
    app.shutdown();
#endif
    return 0;
}
