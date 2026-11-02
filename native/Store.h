#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Policy.h"

namespace zp {
namespace fs = std::filesystem;
inline thread_local unsigned bypass = 0;
struct Bypass { Bypass() { ++bypass; } ~Bypass() { --bypass; } };
inline std::string Utf8(const std::wstring &text) {
  if (text.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (!n) throw std::runtime_error("Invalid text encoding.");
  std::string out(n, 0);
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), &out[0], n, nullptr, nullptr);
  return out;
}
inline std::wstring Wide(const std::string &text, UINT page = CP_UTF8) {
  if (text.empty()) return {};
  int n = MultiByteToWideChar(page, page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (!n) throw std::runtime_error("Invalid text encoding.");
  std::wstring out(n, 0);
  MultiByteToWideChar(page, page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, text.data(), static_cast<int>(text.size()), &out[0], n);
  return out;
}
inline std::string Ansi(const std::wstring &text) {
  BOOL changed = FALSE;
  int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, &changed);
  if (!n || changed) throw std::runtime_error("Path cannot be represented by this client.");
  std::string out(n, 0);
  WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()), &out[0], n, nullptr, &changed);
  if (changed) throw std::runtime_error("Path cannot be represented by this client.");
  return out;
}
inline std::wstring FullPath(const std::wstring &path) {
  if (path.empty() || path.size() >= 32700) throw std::runtime_error("Invalid file path.");
  auto input = path;
  std::replace(input.begin(), input.end(), L'/', L'\\');
  // The supported client uses ordinary DOS paths. Reject device aliases.
  if (input.compare(0, 4, L"\\\\?\\") == 0 || input.compare(0, 4, L"\\\\.\\") == 0)
    throw std::runtime_error("Device paths are unsupported.");
  DWORD n = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
  if (!n || n >= 32700) throw std::runtime_error("Cannot resolve file path.");
  std::wstring out(n, 0);
  DWORD written = GetFullPathNameW(input.c_str(), n, &out[0], nullptr);
  if (!written || written >= n) throw std::runtime_error("Cannot resolve file path.");
  out.resize(written);
  while (out.size() > 3 && out.back() == L'\\') out.pop_back();
  return out;
}
inline std::vector<uint8_t> ReadBytes(const fs::path &path, size_t maximum = 16 * 1024 * 1024) {
  Bypass guard;
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Cannot read a profile file.");
  auto n = input.tellg();
  if (n < 0 || static_cast<uint64_t>(n) > maximum) throw std::runtime_error("Invalid profile file size.");
  std::vector<uint8_t> data(static_cast<size_t>(n));
  input.seekg(0);
  if (!data.empty() && !input.read(reinterpret_cast<char *>(data.data()), data.size())) throw std::runtime_error("Cannot read a profile file.");
  return data;
}
inline void AtomicWrite(const fs::path &path, const void *data, size_t size) {
  Bypass guard;
  fs::create_directories(path.parent_path());
  auto temp = path;
  temp += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetCurrentThreadId());
  HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open profile checkpoint.");
  DWORD written = 0;
  bool ok = size <= MAXDWORD && WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) && written == size && FlushFileBuffers(file);
  CloseHandle(file);
  if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Profile checkpoint failed; the previous checkpoint is retained.");
}
inline void AtomicWrite(const fs::path &path, const std::string &data) { AtomicWrite(path, data.data(), data.size()); }
inline void PlainDirectory(const fs::path &path) {
  Bypass guard;
  auto current = path.root_path();
  for (const auto &part : path.relative_path()) {
    current /= part;
    const DWORD attrs = GetFileAttributesW(current.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
      throw std::runtime_error("Profile folders must not contain junctions or symbolic links.");
  }
}
inline void CopyTree(const fs::path &source, const fs::path &destination) {
  Bypass guard;
  PlainDirectory(source);
  PlainDirectory(destination);
  fs::create_directories(destination);
  if (!fs::exists(source)) return;
  if (!fs::is_directory(source)) throw std::runtime_error("Expected an account or character folder.");
  for (const auto &entry : fs::recursive_directory_iterator(source)) {
    const DWORD attrs = GetFileAttributesW(entry.path().c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) throw std::runtime_error("Cannot clone a linked or unreadable profile file.");
    auto target = destination / entry.path().lexically_relative(source);
    if (entry.is_directory()) fs::create_directories(target);
    else if (entry.is_regular_file()) {
      fs::create_directories(target.parent_path());
      fs::copy_file(entry.path(), target, fs::copy_options::none);
    } else throw std::runtime_error("Unsupported profile file type.");
  }
}

class Store {
 public:
  fs::path game, original, data;
  std::recursive_mutex mutex;
  std::set<std::wstring> active, desired;
  std::map<std::wstring, HANDLE> locks;
  std::vector<std::wstring> muted;
  std::string problem;
  uint64_t blocked = 0;

  explicit Store(fs::path root) : game(FullPath(root.wstring())), original(game / L"WTF" / L"Account"), data(game / L"WTF" / L"zProfiles") {
    Bypass guard;
    PlainDirectory(data);
    desired = ReadModes(); active = desired;
    if (!active.empty()) PlainDirectory(original);
  }
  ~Store() { Bypass guard; for (auto &entry : locks) CloseHandle(entry.second); }
  fs::path Profile(const std::wstring &account) const { return data / L"profiles" / account; }
  std::set<std::wstring> ReadModes() const {
    Bypass guard;
    std::set<std::wstring> out;
    auto file = data / L"modes.txt";
    if (!fs::exists(file)) return out;
    auto bytes = ReadBytes(file, 65536);
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    std::string line;
    if (!std::getline(input, line) || line != "ZPROFILES 1") throw std::runtime_error("Invalid modes.txt; refusing to disable isolation implicitly.");
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      auto name = Wide(line);
      if (!Account(name) || Upper(name) != name || !out.insert(name).second) throw std::runtime_error("Invalid account registry.");
    }
    return out;
  }
  std::vector<std::wstring> Accounts() const {
    Bypass guard;
    std::set<std::wstring> names;
    if (fs::exists(original)) for (const auto &entry : fs::directory_iterator(original)) {
      auto name = entry.path().filename().wstring();
      if (entry.is_directory() && Account(name)) names.insert(Upper(name));
    }
    names.insert(active.begin(), active.end()); names.insert(desired.begin(), desired.end());
    return {names.begin(), names.end()};
  }
  bool Isolated(const std::wstring &account) const { return active.count(Upper(account)) != 0; }
  void Mode(std::wstring account, bool isolate) {
    Bypass guard;
    account = Upper(account);
    auto visible = Accounts();
    if (!Account(account) || std::find(visible.begin(), visible.end(), account) == visible.end()) throw std::runtime_error("Choose an account visible on this computer.");
    fs::create_directories(data);
    HANDLE lock = CreateFileW((data / L"registry.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("Another client is changing account modes. Try again.");
    try {
      auto updated = ReadModes();
      if (isolate) updated.insert(account); else updated.erase(account);
      std::string text = "ZPROFILES 1\n";
      for (const auto &name : updated) text += Utf8(name) + "\n";
      AtomicWrite(data / L"modes.txt", text);
      desired = std::move(updated);
      CloseHandle(lock);
    } catch (...) { CloseHandle(lock); throw; }
  }
  void EnsureProfile(const std::wstring &account) {
    Bypass guard;
    if (!Account(account) || !Isolated(account)) throw std::runtime_error("Account is not isolated in this client session.");
    if (locks.count(account)) return;
    fs::create_directories(data / L"locks");
    HANDLE lock = CreateFileW((data / L"locks" / (account + L".lock")).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("This isolated account is already open in another client.");
    try {
      auto profile = Profile(account);
      PlainDirectory(profile);
      if (!fs::exists(profile)) {
        auto staging = Unique(L"staging", account);
        CopyTree(original / account, staging);
        AtomicWrite(staging / L".zprofiles-ready", "ZPROFILES 1\n");
        fs::create_directories(profile.parent_path());
        if (!MoveFileExW(staging.c_str(), profile.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot activate profile clone.");
      }
      auto marker = ReadBytes(profile / L".zprofiles-ready", 64);
      if (std::string(marker.begin(), marker.end()) != "ZPROFILES 1\n") throw std::runtime_error("Profile clone is incomplete.");
      locks.emplace(account, lock);
    } catch (...) { CloseHandle(lock); throw; }
  }
  fs::path Unique(const std::wstring &kind, const std::wstring &label) const {
    Bypass guard;
    static uint64_t serial = 0;
    FILETIME t; GetSystemTimeAsFileTime(&t);
    auto stamp = (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    auto path = data / kind / (label + L"-" + std::to_wstring(stamp) + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial));
    fs::create_directories(path.parent_path());
    return path;
  }
  // Returns unchanged input for Default accounts. Writes during a current-
  // character revert go to a discard area until the fresh UI finishes loading.
  std::wstring Route(const std::wstring &input, bool writing) {
    if (bypass) return input;
    Bypass guard;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto absolute = FullPath(input);
    const auto root = original.wstring();
    if (!Within(absolute, root) || absolute.size() <= root.size() + 1) return input;
    auto tail = absolute.substr(root.size() + 1);
    auto slash = tail.find(L'\\');
    auto account = Upper(tail.substr(0, slash));
    if (!Isolated(account)) return input;
    if (!Account(account)) throw std::runtime_error("Invalid isolated account path.");
    EnsureProfile(account);
    fs::path mapped = Profile(account);
    auto relative = slash == std::wstring::npos ? L"" : tail.substr(slash + 1);
    if (!relative.empty()) mapped /= relative;
    PlainDirectory(mapped);
    if (writing) for (const auto &scope : muted) if (Within(absolute, scope)) {
      mapped = data / L"discard" / std::to_wstring(GetCurrentProcessId()) / account / relative;
      fs::create_directories(mapped.parent_path());
      break;
    }
    return mapped.wstring();
  }
  std::vector<std::pair<std::wstring, std::wstring>> Characters(const std::wstring &account) const {
    Bypass guard;
    if (!Account(account)) return {};
    std::set<std::pair<std::wstring, std::wstring>> names;
    for (auto root : {original / account, Profile(account)}) {
      if (!fs::exists(root)) continue;
      for (const auto &realm : fs::directory_iterator(root)) {
        auto r = realm.path().filename().wstring();
        if (!realm.is_directory() || Upper(r) == L"SAVEDVARIABLES" || !Segment(r)) continue;
        for (const auto &character : fs::directory_iterator(realm.path())) {
          auto c = character.path().filename().wstring();
          if (character.is_directory() && Segment(c)) names.insert({r, c});
        }
      }
    }
    return {names.begin(), names.end()};
  }
  fs::path Journal(const std::wstring &account, uint64_t guid, bool official) const {
    return data / L"bars" / account / (std::to_wstring(guid) + (official ? L".official" : L".local"));
  }
  bool LoadBars(const std::wstring &account, uint64_t guid, bool official, Bars &bars) const {
    Bypass guard;
    auto file = Journal(account, guid, official);
    if (!fs::exists(file)) return false;
    auto bytes = ReadBytes(file, sizeof(Bars));
    if (bytes.size() != sizeof(Bars)) throw std::runtime_error("Invalid action-bar checkpoint; use Revert.");
    std::memcpy(bars.data(), bytes.data(), sizeof(Bars));
    return true;
  }
  void SaveBars(const std::wstring &account, uint64_t guid, bool official, const Bars &bars) {
    if (!guid) throw std::runtime_error("No character identity for action-bar checkpoint.");
    EnsureProfile(account);
    AtomicWrite(Journal(account, guid, official), bars.data(), sizeof(Bars));
  }
  fs::path Identity(const std::wstring &account, const std::wstring &realm, const std::wstring &character) const {
    if (!Account(account) || !Segment(realm) || !Segment(character)) throw std::runtime_error("Invalid character identity.");
    return data / L"identities" / account / realm / (character + L".txt");
  }
  void Identify(const std::wstring &account, const std::wstring &realm, const std::wstring &character, uint64_t guid) {
    if (!Isolated(account) || !guid) return;
    AtomicWrite(Identity(account, realm, character), std::to_string(guid));
  }
  uint64_t Guid(const std::wstring &account, const std::wstring &realm, const std::wstring &character) const {
    Bypass guard;
    auto path = Identity(account, realm, character);
    if (!fs::exists(path)) return 0;
    auto bytes = ReadBytes(path, 32);
    auto text = std::string(bytes.begin(), bytes.end());
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error("Invalid character checkpoint identity.");
    return std::stoull(text);
  }
  void RestoreBars(const std::wstring &account, uint64_t guid) {
    Bypass guard;
    if (!guid) return;
    auto local = Journal(account, guid, false);
    if (fs::exists(local)) fs::copy_file(local, Unique(L"backups", account + L"-bars-" + std::to_wstring(guid)), fs::copy_options::none);
    Bars official{};
    if (LoadBars(account, guid, true, official)) AtomicWrite(local, official.data(), sizeof(Bars));
    else if (fs::exists(local)) {
      auto archive = Unique(L"backups", account + L"-bars-unseeded");
      if (!MoveFileExW(local.c_str(), archive.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot archive action-bar checkpoint.");
    }
    auto localFlags = Journal(account, guid, false); localFlags += L".toggles";
    auto officialFlags = Journal(account, guid, true); officialFlags += L".toggles";
    if (fs::exists(localFlags)) fs::copy_file(localFlags, Unique(L"backups", account + L"-bar-flags"), fs::copy_options::none);
    if (fs::exists(officialFlags)) {
      auto bytes = ReadBytes(officialFlags, 1);
      if (bytes.size() != 1) throw std::runtime_error("Invalid official bar-layout checkpoint.");
      AtomicWrite(localFlags, bytes.data(), 1);
    } else if (fs::exists(localFlags)) {
      if (!MoveFileExW(localFlags.c_str(), Unique(L"backups", account + L"-bar-flags-unseeded").c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot archive bar-layout checkpoint.");
    }
  }
  void Revert(const std::wstring &account, const std::wstring &realm, const std::wstring &character, bool current) {
    Bypass guard;
    EnsureProfile(account);
    bool whole = realm.empty() && character.empty();
    fs::path relative;
    if (!whole) {
      if (!Segment(realm) || !Segment(character)) throw std::runtime_error("Choose a valid character.");
      auto visible = Characters(account);
      if (std::find(visible.begin(), visible.end(), std::make_pair(realm, character)) == visible.end()) throw std::runtime_error("Character folder was not found.");
      relative = fs::path(realm) / character;
    }
    auto target = Profile(account);
    auto source = original / account;
    if (!whole) { target /= relative; source /= relative; }
    auto replacement = Unique(L"staging", account + L"-revert");
    CopyTree(source, replacement);
    if (whole) AtomicWrite(replacement / L".zprofiles-ready", "ZPROFILES 1\n");
    auto backup = Unique(L"backups", account + L"-revert");
    if (fs::exists(target) && !MoveFileExW(target.c_str(), backup.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot back up isolated profile. Close other clients first.");
    fs::create_directories(target.parent_path());
    if (!MoveFileExW(replacement.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
      if (fs::exists(backup)) MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH);
      throw std::runtime_error("Cannot activate reverted profile; prior copy retained.");
    }
    // Mute even if a later checkpoint operation fails: stale Lua saves must
    // never overwrite the newly restored files.
    if (current) muted.push_back(source.wstring());
    if (whole) {
      auto directory = data / L"bars" / account;
      if (fs::exists(directory)) {
        std::vector<uint64_t> guids;
        for (const auto &entry : fs::directory_iterator(directory)) if (entry.path().extension() == L".local") guids.push_back(std::stoull(entry.path().stem().wstring()));
        for (auto guid : guids) RestoreBars(account, guid);
      }
    } else RestoreBars(account, Guid(account, realm, character));
  }
};
} // namespace zp
