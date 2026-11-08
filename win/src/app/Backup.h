#pragma once
#include <string>
#include "app/App.h"

namespace sk {

struct MergeStat {
  int added = 0;
  int skipped = 0;
  int keys = 0;
};

std::string BuildBackupPayload(const AppState& st);
bool MergeBackupPayload(const std::string& json, AppState& st, MergeStat& stat, std::wstring& err);

std::string SealBackup(const std::string& payload, const std::wstring& password, bool encrypt);
bool OpenBackup(const std::string& text, const std::wstring& password, std::string& payload);

bool ReadTextFile(const std::wstring& path, std::string& out);
bool WriteTextFile(const std::wstring& path, const std::string& text);

std::wstring BackupDefaultName();
std::wstring ResolveKeyFile(const Host& h);

}
