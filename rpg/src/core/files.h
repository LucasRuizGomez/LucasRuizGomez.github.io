// File and JSON helpers shared by the game library and the app.
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace hc {

using json = nlohmann::json;

// Whole file as a string. Throws std::runtime_error when it cannot be opened.
std::string readFile(const std::string& path);

// Parses a JSON file (comments allowed). Throws with the file name in the message on failure.
json loadJson(const std::string& path);

void logInfo(const char* fmt, ...);
void logWarn(const char* fmt, ...);

}   // namespace hc
