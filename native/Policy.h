#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace zp {
constexpr uint32_t PlayerLogin = 0x03D;
constexpr uint32_t SetActionButton = 0x128;
constexpr uint32_t ActionButtons = 0x129;
constexpr uint32_t AccountDataTimes = 0x209;
constexpr uint32_t RequestAccountData = 0x20A;
constexpr uint32_t UpdateAccountData = 0x20B;
constexpr uint32_t ServerAccountData = 0x20C;
constexpr uint32_t ActionBarToggles = 0x2BF;
using Bars = std::array<uint32_t, 120>;

inline bool SettingsUpload(uint32_t opcode) {
  return opcode == SetActionButton || opcode == UpdateAccountData ||
         opcode == ActionBarToggles || opcode == RequestAccountData;
}
inline std::wstring Upper(std::wstring value) {
  for (auto &c : value) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
  return value;
}
inline bool Segment(const std::wstring &value) {
  if (value.empty() || value.size() > 100 || value == L"." || value == L".." ||
      value.back() == L'.' || value.back() == L' ') return false;
  for (auto c : value) if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) return false;
  auto stem = Upper(value.substr(0, value.find(L'.')));
  if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") return false;
  if (stem.size() == 4 && (stem.substr(0, 3) == L"COM" || stem.substr(0, 3) == L"LPT") && stem[3] >= L'0' && stem[3] <= L'9') return false;
  return true;
}
inline bool Account(const std::wstring &value) {
  if (!Segment(value)) return false;
  // Vanilla login account directory names are ASCII; avoid ambiguous casing.
  for (auto c : value) if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
      (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L'#')) return false;
  return true;
}
inline bool Within(const std::wstring &path, const std::wstring &root) {
  const auto p = Upper(path), r = Upper(root);
  return p == r || (p.size() > r.size() && p.compare(0, r.size(), r) == 0 && p[r.size()] == L'\\');
}
inline bool EditBars(Bars &bars, const uint8_t *body, size_t size) {
  if (!body || size != 5 || body[0] >= bars.size()) return false;
  std::memcpy(&bars[body[0]], body + 1, 4);
  return true;
}
} // namespace zp
