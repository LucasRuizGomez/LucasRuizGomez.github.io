// The few things that differ between the browser and a desktop build:
// persistent storage, fetching files on demand, the clock, and page chrome.
#pragma once
#include <functional>
#include <string>

namespace hc::platform {

// Folder that assets/ and config/ are read from (ends with a slash; empty on the web, where they sit at the root).
std::string dataRoot();

// Small key/value store: localStorage in the browser, files in the user's preferences folder on desktop.
bool storageSet(const std::string& key, const std::string& value);
std::string storageGet(const std::string& key);   // empty when missing
void storageRemove(const std::string& key);

// Makes a music file available locally and calls back with its path (empty on failure).
// Desktop: the file already exists. Web: downloaded once into the in-memory file system.
void fetchMusic(const std::string& fileName, std::function<void(const std::string& path)> done);

std::string timestamp();          // "2026-11-16 14:05"
void setCrosshair(bool on);       // mouse cursor shape
void showPageLink(bool visible);  // the "back to site" link on the web page
bool canQuit();                   // false in the browser
std::string queryParam(const std::string& name);   // debug switches: URL query on the web, --name=value on desktop
void setArgs(int argc, char** argv);

}   // namespace hc::platform
