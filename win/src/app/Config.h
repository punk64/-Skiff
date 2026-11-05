#pragma once
#include <string>
#include <vector>
#include "App.h"

namespace sk {

std::wstring ExeDirectory();
std::wstring KeyDirectory();
std::vector<std::wstring> ListKeyFiles();

struct StorePath {
    std::wstring file;
    bool portable = true;
};

StorePath ResolveStorePath();
bool LoadStore(AppState& st);
bool SaveStore(const AppState& st);
std::string SerializeHosts(const AppState& st);
bool DeserializeHosts(const std::string& json, AppState& st);
std::string StorePathUtf8();
bool SystemPrefersLight();

}
