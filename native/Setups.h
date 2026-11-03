#pragma once
#include "Store.h"

namespace zp {
struct Setup {
  Bars bars{};
  uint8_t toggles = 0;
  std::string bindings;
};
inline bool SetupName(const std::wstring &name) {
  if (!Segment(name) || name.size() > 32 || name.front() == L' ' || name.back() == L' ') return false;
  for (auto c : name) if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
      (c >= L'0' && c <= L'9') || c == L' ' || c == L'-' || c == L'_')) return false;
  return true;
}
inline bool HexField(const std::string &field, size_t maximum) {
  if (field.empty() || field.size() > maximum * 2 || field.size() % 2) return false;
  for (auto c : field) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  for (size_t i = 0; i < field.size(); i += 2) if (field.substr(i, 2) == "00") return false;
  return true;
}
inline void ValidateBindings(const std::string &text) {
  if (text.size() < 5 || text.size() > 256 * 1024 || text.compare(0, 5, "ZPB1\n") || text.back() != '\n')
    throw std::runtime_error("Invalid setup bindings.");
  std::set<std::string> keys;
  std::istringstream lines(text.substr(5));
  std::string row;
  while (std::getline(lines, row)) {
    auto tab = row.find('\t');
    if (tab == std::string::npos || !HexField(row.substr(0, tab), 128) || !HexField(row.substr(tab + 1), 1024) ||
        !keys.insert(row.substr(0, tab)).second || keys.size() > 4096)
      throw std::runtime_error("Invalid or duplicate setup binding.");
  }
}
inline uint32_t SetupChecksum(const std::string &bytes) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : bytes) { hash ^= c; hash *= 16777619u; }
  return hash;
}
inline fs::path SetupDirectory(Store &store, const std::wstring &account, uint64_t guid) {
  if (!guid) throw std::runtime_error("Log in to a character before managing setups.");
  store.EnsureProfile(account);
  auto path = store.data / L"setups" / account / std::to_wstring(guid);
  PlainDirectory(path);
  return path;
}
inline fs::path SetupPath(Store &store, const std::wstring &account, uint64_t guid, const std::wstring &name, bool undo = false) {
  if (!undo && !SetupName(name)) throw std::runtime_error("Use 1-32 letters, numbers, spaces, hyphens or underscores for the setup name.");
  auto path = SetupDirectory(store, account, guid) / (undo ? L".undo.zps" : name + L".zps");
  PlainDirectory(path);
  return path;
}
inline Setup ReadSetup(const fs::path &path) {
  auto raw = ReadBytes(path, 257 * 1024);
  const size_t prefix = 7 + sizeof(Bars) + 1;
  if (raw.size() < prefix + 5 + sizeof(uint32_t) || std::memcmp(raw.data(), "ZPSET1\n", 7))
    throw std::runtime_error("Invalid setup file; it was not applied.");
  std::string bytes(raw.begin(), raw.end() - sizeof(uint32_t));
  uint32_t checksum = 0;
  std::memcpy(&checksum, raw.data() + bytes.size(), sizeof(checksum));
  if (checksum != SetupChecksum(bytes)) throw std::runtime_error("Setup checksum failed; it was not applied.");
  Setup setup;
  std::memcpy(setup.bars.data(), raw.data() + 7, sizeof(Bars));
  setup.toggles = raw[7 + sizeof(Bars)];
  if (setup.toggles > 15) throw std::runtime_error("Invalid setup bar visibility.");
  setup.bindings = bytes.substr(prefix);
  ValidateBindings(setup.bindings);
  return setup;
}
inline void WriteSetup(Store &store, const fs::path &path, const Setup &setup) {
  Bypass guard;
  ValidateBindings(setup.bindings);
  if (setup.toggles > 15) throw std::runtime_error("Invalid setup bar visibility.");
  std::string bytes("ZPSET1\n");
  bytes.append(reinterpret_cast<const char *>(setup.bars.data()), sizeof(Bars));
  bytes.push_back(static_cast<char>(setup.toggles));
  bytes += setup.bindings;
  auto checksum = SetupChecksum(bytes);
  bytes.append(reinterpret_cast<const char *>(&checksum), sizeof(checksum));
  PlainDirectory(path);
  if (fs::exists(path)) fs::copy_file(path, store.Unique(L"backups", L"setup-" + path.stem().wstring()), fs::copy_options::none);
  AtomicWrite(path, bytes);
}
inline std::vector<std::wstring> ListSetups(Store &store, const std::wstring &account, uint64_t guid) {
  Bypass guard;
  auto directory = SetupDirectory(store, account, guid);
  std::vector<std::wstring> names;
  if (fs::exists(directory)) for (const auto &entry : fs::directory_iterator(directory)) {
    auto name = entry.path().stem().wstring();
    if (entry.is_regular_file() && entry.path().extension() == L".zps" && SetupName(name)) {
      PlainDirectory(entry.path());
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end(), [](const auto &a, const auto &b) { return Upper(a) < Upper(b); });
  return names;
}
inline void RenameSetup(Store &store, const fs::path &from, const fs::path &to) {
  Bypass guard;
  if (Upper(from.wstring()) == Upper(to.wstring()) || fs::exists(to)) throw std::runtime_error("That setup name already exists.");
  ReadSetup(from);
  if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Could not rename the setup.");
  (void)store;
}
inline void DeleteSetup(Store &store, const fs::path &path) {
  Bypass guard;
  if (!MoveFileExW(path.c_str(), store.Unique(L"backups", L"deleted-setup-" + path.stem().wstring()).c_str(), MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Could not archive the setup.");
}
} // namespace zp
