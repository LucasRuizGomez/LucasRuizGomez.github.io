#include "engine/platform.h"

#include "core/files.h"

#include <SDL.h>
#include <ctime>
#include <fstream>
#include <map>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace hc::platform {

namespace {
std::map<std::string, std::string> g_args;
}

void setArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.rfind("--", 0) != 0) continue;
        a = a.substr(2);
        const size_t eq = a.find('=');
        g_args[a.substr(0, eq)] = eq == std::string::npos ? "1" : a.substr(eq + 1);
    }
}

std::string timestamp() {
    const std::time_t t = std::time(nullptr);
    char buf[32] = "";
    if (const std::tm* tm = std::localtime(&t)) std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", tm);
    return buf;
}

#ifdef __EMSCRIPTEN__

EM_JS(int, js_storage_set, (const char* key, const char* value), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        return 1;
    } catch (e) { return 0; }
});
EM_JS(char*, js_storage_get, (const char* key), {
    var v = null;
    try { v = localStorage.getItem(UTF8ToString(key)); } catch (e) {}
    if (v === null) return 0;
    return stringToNewUTF8(v);
});
EM_JS(void, js_storage_remove, (const char* key), {
    try { localStorage.removeItem(UTF8ToString(key)); } catch (e) {}
});
EM_JS(char*, js_query, (const char* name), {
    var v = new URLSearchParams(location.search).get(UTF8ToString(name));
    if (v === null) return 0;
    return stringToNewUTF8(v === "" ? "1" : v);
});
EM_JS(void, js_cursor, (int cross), { Module.canvas.style.cursor = cross ? "crosshair" : "default"; });
EM_JS(void, js_page_link, (int visible), {
    var el = document.getElementById("backLink");
    if (el) el.hidden = !visible;
});

std::string dataRoot() { return "/"; }

bool storageSet(const std::string& key, const std::string& value) { return js_storage_set(key.c_str(), value.c_str()) != 0; }
std::string storageGet(const std::string& key) {
    char* v = js_storage_get(key.c_str());
    if (!v) return "";
    std::string out = v;
    free(v);
    return out;
}
void storageRemove(const std::string& key) { js_storage_remove(key.c_str()); }

namespace {
std::map<std::string, std::function<void(const std::string&)>> g_pending;
void onMusicLoaded(const char* file) {
    auto it = g_pending.find(file);
    if (it == g_pending.end()) return;
    auto cb = std::move(it->second);
    g_pending.erase(it);
    cb(file);
}
void onMusicFailed(const char* file) {
    logWarn("Could not download %s", file);
    auto it = g_pending.find(file);
    if (it == g_pending.end()) return;
    auto cb = std::move(it->second);
    g_pending.erase(it);
    cb("");
}
}   // namespace

void fetchMusic(const std::string& fileName, std::function<void(const std::string&)> done) {
    const std::string local = "/music_" + fileName;
    if (std::ifstream(local).good()) {
        done(local);
        return;
    }
    if (g_pending.count(local)) return;
    g_pending[local] = std::move(done);
    emscripten_async_wget(("music/" + fileName).c_str(), local.c_str(), onMusicLoaded, onMusicFailed);
}

void setCrosshair(bool on) { js_cursor(on ? 1 : 0); }
void showPageLink(bool visible) { js_page_link(visible ? 1 : 0); }
bool canQuit() { return false; }
std::string queryParam(const std::string& name) {
    char* v = js_query(name.c_str());
    if (!v) return "";
    std::string out = v;
    free(v);
    return out;
}

#else   // ── desktop ──

namespace {
std::string prefFile(const std::string& key) {
    static std::string dir;
    if (dir.empty()) {
        char* p = SDL_GetPrefPath("Lukeda", "HollowCrypt");
        dir = p ? p : "./";
        SDL_free(p);
    }
    return dir + key + ".json";
}
SDL_Cursor* g_cross = nullptr;
SDL_Cursor* g_arrow = nullptr;
}   // namespace

std::string dataRoot() {
#ifdef HC_DATA_ROOT
    return HC_DATA_ROOT;
#else
    return "";
#endif
}

bool storageSet(const std::string& key, const std::string& value) {
    std::ofstream f(prefFile(key), std::ios::binary);
    if (!f) return false;
    f << value;
    return f.good();
}
std::string storageGet(const std::string& key) {
    try {
        return readFile(prefFile(key));
    } catch (const std::exception&) { return ""; }
}
void storageRemove(const std::string& key) { std::remove(prefFile(key).c_str()); }

void fetchMusic(const std::string& fileName, std::function<void(const std::string&)> done) { done(dataRoot() + "assets/audio/music/" + fileName); }

void setCrosshair(bool on) {
    if (!g_cross) {
        g_cross = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        g_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    }
    SDL_SetCursor(on ? g_cross : g_arrow);
}
void showPageLink(bool) {}
bool canQuit() { return true; }
std::string queryParam(const std::string& name) {
    auto it = g_args.find(name);
    return it == g_args.end() ? "" : it->second;
}

#endif

}   // namespace hc::platform
