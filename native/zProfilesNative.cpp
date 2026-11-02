#include "FileHooks.h"
#include "Packets.h"
#include <memory>

namespace {
uintptr_t image = 0;
std::unique_ptr<zp::Store> store;
std::map<void *, zp::Session> sessions;
void *currentConnection = nullptr;
volatile LONG installState = 0;
DWORD installResult = 1;

template<class T> T At(uintptr_t address) { return reinterpret_cast<T>(image + address - 0x400000); }
using CFunction = int(__fastcall *)(void *);
using Register = void(__fastcall *)(const char *, CFunction);
using LuaType = int(__fastcall *)(void *, int);
using LuaString = const char *(__fastcall *)(void *, int);
using PushString = void(__fastcall *)(void *, const char *);
using PushNumber = void(__fastcall *)(void *, double);
Register originalRegister = nullptr;
CFunction originalGetToggles = nullptr;
struct Packet { void *vtable; uint8_t *buffer; uint32_t base, allocated, size, read; };
static_assert(sizeof(Packet) == 24, "WoW 1.12 is x86 only");
using Send = void(__fastcall *)(void *, void *, Packet *);
using Dispatch = void(__fastcall *)(void *, void *, uint32_t, Packet *);
Send originalSend = nullptr;
Dispatch originalDispatch = nullptr;
Packet replayPrototype{};
void *replayConnection = nullptr;
uint32_t replayArgument = 0;
uint64_t replayGuid = 0;

void Log(const std::string &text) noexcept {
  try {
    zp::Bypass guard;
    wchar_t file[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, file, MAX_PATH)) return;
    std::ofstream out(zp::fs::path(file).parent_path() / L"zProfilesNative.log", std::ios::app);
    SYSTEMTIME time; GetLocalTime(&time);
    out << time.wYear << '-' << time.wMonth << '-' << time.wDay << ' ' << time.wHour << ':' << time.wMinute << ':' << time.wSecond << " " << text << '\n';
  } catch (...) {}
}
bool ReadAccount(char (&output)[101]) noexcept {
  __try {
    const char *name = *At<const char **>(0x00BE1C0C);
    if (!name) return false;
    for (size_t i = 0; i < sizeof(output); ++i) {
      output[i] = name[i];
      if (!output[i]) return i != 0;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
  output[0] = 0; return false;
}
// Verified against this executable's Script_GetActionBarToggles at 0x4E7660.
// SetActionBarToggles only sends a packet, so the isolated client must also
// update its local field when that packet is suppressed.
bool PlayerFlags(uint64_t expectedGuid, uint8_t *value, bool write) noexcept {
  using Lookup = uintptr_t(__stdcall *)(uint64_t);
  __try {
    if (!expectedGuid || !*At<uint8_t *>(0x00B4B424)) return false;
    auto manager = *At<uintptr_t *>(0x00B41414);
    if (!manager || *reinterpret_cast<uint64_t *>(manager + 0xC0) != expectedGuid) return false;
    auto player = At<Lookup>(0x00464870)(expectedGuid);
    if (!player) return false;
    auto fields = *reinterpret_cast<uintptr_t *>(player + 0xE68);
    if (!fields) return false;
    auto flags = reinterpret_cast<uint8_t *>(fields + 0x102A);
    if (write) *flags = (*flags & 0xF0) | (*value & 0x0F);
    else *value = *flags & 0x0F;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::wstring CurrentAccount() {
  char account[101]{};
  if (!ReadAccount(account)) return {};
  auto out = zp::Upper(zp::Wide(account, CP_ACP));
  return zp::Account(out) ? out : L"";
}
bool View(Packet *packet, uint8_t **data, size_t *length) noexcept {
  __try {
    if (!packet || !packet->buffer || packet->read < packet->base || packet->read > packet->size || packet->allocated > 16 * 1024 * 1024) return false;
    const uint32_t offset = packet->read - packet->base;
    const uint32_t remaining = packet->size - packet->read;
    if (offset > packet->allocated || remaining > packet->allocated - offset) return false;
    *data = packet->buffer + offset;
    *length = remaining;
    return remaining != 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ReadOpcode(const uint8_t *data, size_t n, size_t width, uint32_t *opcode) noexcept {
  __try {
    if (n < width) return false;
    *opcode = 0; std::memcpy(opcode, data, width); return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void Fault(const char *message) noexcept {
  try { std::lock_guard<std::recursive_mutex> lock(store->mutex); store->problem = message; Log(message); } catch (...) {}
}
zp::Session &Session(void *connection) {
  auto &session = sessions[connection];
  if (session.account.empty()) session.account = CurrentAccount();
  return session;
}
void __fastcall SendHook(void *connection, void *edx, Packet *packet) {
  bool forward = true;
  uint32_t opcode = 0;
  try {
    std::lock_guard<std::recursive_mutex> lock(store->mutex);
    uint8_t *data = nullptr; size_t length = 0;
    if (!View(packet, &data, &length) || !ReadOpcode(data, length, 4, &opcode)) {
      // An undecodable message cannot be safely classified as gameplay.
      if (store->Isolated(Session(connection).account)) { forward = false; Fault("Unreadable outgoing packet blocked for an isolated account."); }
    } else if (opcode == 0x1ED) { // CMSG_AUTH_SESSION: build,u32 serverId,account\0
      zp::Session fresh;
      if (length >= 13) {
        auto begin = data + 12;
        size_t limit = std::min<size_t>(length - 12, 101), n = 0;
        while (n < limit && begin[n]) ++n;
        if (n && n < limit) {
          auto account = zp::Upper(zp::Wide(std::string(reinterpret_cast<char *>(begin), n), CP_ACP));
          if (zp::Account(account)) fresh.account = account;
        }
      }
      if (fresh.account.empty()) fresh.account = CurrentAccount();
      sessions[connection] = std::move(fresh);
      currentConnection = connection;
      replayConnection = nullptr; replayGuid = 0;
    } else if (opcode == zp::PlayerLogin || zp::SettingsUpload(opcode)) {
      if (zp::SettingsUpload(opcode)) forward = false;
      auto &session = Session(connection);
      if (session.account.empty() && !store->active.empty() && zp::SettingsUpload(opcode)) {
        forward = false; Fault("Cannot identify the account; settings upload blocked.");
      } else {
        // Set before disk I/O so any exception still blocks the upload.
        forward = !(store->Isolated(session.account) && zp::SettingsUpload(opcode));
        forward = session.Send(*store, opcode, data + 4, length - 4);
        if (!forward && opcode == zp::ActionBarToggles && session.guid && session.togglesSeeded && length == 5) {
          auto flags = data[4];
          if (!PlayerFlags(session.guid, &flags, true)) Fault("Bar layout saved locally; live field update failed.");
        }
        if (opcode == zp::PlayerLogin) currentConnection = connection;
      }
    }
  } catch (const std::exception &error) { Fault(error.what()); }
    catch (...) { forward = !zp::SettingsUpload(opcode); Fault("Unexpected outgoing settings failure."); }
  if (forward) originalSend(connection, edx, packet);
}
void __fastcall DispatchHook(void *connection, void *edx, uint32_t argument, Packet *packet) {
  bool forward = true;
  uint32_t opcode = 0;
  try {
    std::lock_guard<std::recursive_mutex> lock(store->mutex);
    uint8_t *data = nullptr; size_t length = 0;
    if (!View(packet, &data, &length) || !ReadOpcode(data, length, 2, &opcode)) {
      if (store->Isolated(Session(connection).account)) { forward = false; Fault("Unreadable incoming packet blocked for an isolated account."); }
    } else if (opcode == 0x04D) { // SMSG_LOGOUT_COMPLETE
      auto &session = Session(connection);
      session.guid = 0; session.ready = false; session.togglesSeeded = false;
      session.realm.clear(); session.character.clear();
      if (connection == replayConnection) { replayConnection = nullptr; replayGuid = 0; }
    } else if (opcode == zp::ActionButtons || opcode == zp::AccountDataTimes || opcode == zp::ServerAccountData) {
      auto &session = Session(connection);
      if (store->Isolated(session.account)) {
        forward = false; // a failed import must not replace the local profile
        forward = session.Receive(*store, opcode, data + 2, length - 2);
        if (opcode == zp::ActionButtons && forward) {
          replayPrototype = *packet; replayConnection = connection;
          replayArgument = argument; replayGuid = session.guid;
        }
      }
    }
  } catch (const std::exception &error) { Fault(error.what()); }
    catch (...) { forward = false; Fault("Unexpected incoming settings failure."); }
  if (forward) originalDispatch(connection, edx, argument, packet);
}
zp::Session *CurrentSession() {
  auto found = sessions.find(currentConnection);
  return found == sessions.end() ? nullptr : &found->second;
}
bool Replay(zp::Session &session) {
  if (replayConnection != currentConnection || replayGuid != session.guid || !replayPrototype.vtable) return false;
  zp::Bars official{};
  if (!store->LoadBars(session.account, session.guid, true, official)) return false;
  std::array<uint8_t, 2 + sizeof(zp::Bars)> bytes{};
  uint16_t opcode = static_cast<uint16_t>(zp::ActionButtons);
  std::memcpy(bytes.data(), &opcode, 2);
  std::memcpy(bytes.data() + 2, official.data(), sizeof(official));
  Packet packet = replayPrototype;
  packet.buffer = bytes.data(); packet.base = packet.read = 0;
  packet.allocated = packet.size = static_cast<uint32_t>(bytes.size());
  session.bars = official; session.ready = true;
  originalDispatch(replayConnection, nullptr, replayArgument, &packet);
  auto flagsFile = store->Journal(session.account, session.guid, true); flagsFile += L".toggles";
  {
    zp::Bypass guard;
    if (zp::fs::exists(flagsFile)) {
      auto flagBytes = zp::ReadBytes(flagsFile, 1);
      if (flagBytes.size() != 1 || !PlayerFlags(session.guid, flagBytes.data(), true)) return false;
      session.toggles = flagBytes[0] & 0x0F;
    }
  }
  return true;
}
std::string Argument(void *L, int index) {
  if (At<LuaType>(0x006F3400)(L, index) != 4) return {};
  auto value = At<LuaString>(0x006F3690)(L, index);
  return value ? value : "";
}
void Text(void *L, const std::string &text) { At<PushString>(0x006F3890)(L, text.c_str()); }
void Number(void *L, double number) { At<PushNumber>(0x006F3810)(L, number); }
int Reply(void *L, bool ok, const std::string &text, bool current = false) {
  Number(L, ok ? 1 : 0); Text(L, text); Number(L, current ? 1 : 0); return 3;
}
int __fastcall Bridge(void *L) {
  try {
    auto command = Argument(L, 1);
    if (command == "version") { Number(L, 1); return 1; }
    std::lock_guard<std::recursive_mutex> lock(store->mutex);
    auto current = CurrentSession();
    auto account = current ? current->account : CurrentAccount();
    if (command == "status") {
      Text(L, zp::Utf8(account));
      Text(L, store->Isolated(account) ? "isolated" : "default");
      store->desired = store->ReadModes();
      Text(L, store->desired.count(account) ? "isolated" : "default");
      Text(L, store->problem.empty() ? "ok" : store->problem);
      Number(L, static_cast<double>(store->blocked)); return 5;
    }
    if (command == "accounts") {
      store->desired = store->ReadModes();
      std::string rows;
      for (const auto &name : store->Accounts()) rows += zp::Utf8(name) + "\t" + (store->Isolated(name) ? "isolated" : "default") + "\t" + (store->desired.count(name) ? "isolated" : "default") + "\n";
      Text(L, rows); return 1;
    }
    auto selected = zp::Upper(zp::Wide(Argument(L, 2)));
    if (command == "characters") {
      std::string rows;
      for (auto &entry : store->Characters(selected)) rows += zp::Utf8(entry.first) + "\t" + zp::Utf8(entry.second) + "\t" +
        (current && current->account == selected && current->realm == entry.first && current->character == entry.second ? "1" : "0") + "\n";
      Text(L, rows); return 1;
    }
    if (command == "setMode") {
      auto mode = Argument(L, 3);
      if (mode != "isolated" && mode != "default") return Reply(L, false, "Unknown account mode.");
      store->Mode(selected, mode == "isolated");
      return Reply(L, true, "Mode saved. Fully exit and restart WoW to apply it.");
    }
    if (command == "identify") {
      if (!current) return Reply(L, false, "No active character session.");
      auto realm = zp::Wide(Argument(L, 2)), character = zp::Wide(Argument(L, 3));
      if (!zp::Segment(realm) || !zp::Segment(character)) return Reply(L, false, "Invalid character identity.");
      current->realm = realm; current->character = character;
      store->Identify(current->account, realm, character, current->guid);
      if (store->Isolated(current->account) && !current->togglesSeeded) {
        uint8_t flags = 0;
        if (!PlayerFlags(current->guid, &flags, false)) return Reply(L, false, "Character layout is not available yet.");
        auto officialPath = store->Journal(current->account, current->guid, true); officialPath += L".toggles";
        zp::AtomicWrite(officialPath, &flags, 1);
        current->toggles = flags;
        auto localPath = store->Journal(current->account, current->guid, false); localPath += L".toggles";
        zp::Bypass guard;
        if (zp::fs::exists(localPath)) {
          auto bytes = zp::ReadBytes(localPath, 1);
          if (bytes.size() != 1 || !PlayerFlags(current->guid, bytes.data(), true)) throw std::runtime_error("Cannot restore isolated bar layout.");
          current->toggles = bytes[0] & 0x0F;
        }
        current->togglesSeeded = true;
      }
      return Reply(L, true, "Character identified.");
    }
    if (command == "resume") {
      // Called after PLAYER_ENTERING_WORLD; all old SavedVariables writes
      // from ReloadUI have finished by this point.
      store->muted.clear();
      return Reply(L, true, "Profile writes resumed.");
    }
    if (command == "toggles") {
      if (!current || !store->Isolated(current->account) || !current->guid) { Number(L, -1); return 1; }
      zp::Bypass guard;
      auto path = store->Journal(current->account, current->guid, false); path += L".toggles";
      if (!zp::fs::exists(path)) { Number(L, -1); return 1; }
      auto bytes = zp::ReadBytes(path, 1);
      if (bytes.size() != 1) throw std::runtime_error("Invalid bar-layout checkpoint.");
      Number(L, bytes[0]); return 1;
    }
    if (command == "revertCharacter" || command == "revertAccount") {
      if (!zp::Account(selected) || !store->Isolated(selected)) return Reply(L, false, "Revert is available for active isolated accounts.");
      bool whole = command == "revertAccount";
      auto realm = whole ? L"" : zp::Wide(Argument(L, 3));
      auto character = whole ? L"" : zp::Wide(Argument(L, 4));
      bool isCurrent = current && current->account == selected && (whole || (current->realm == realm && current->character == character));
      // Restoring a live character without its official bar seed could mix
      // original macros with isolated slots. Require a fresh login instead.
      if (isCurrent && (!current->ready || replayGuid != current->guid || replayConnection != currentConnection))
        return Reply(L, false, "Official action bars are unavailable. Log out and back in before reverting.");
      try {
        store->Revert(selected, realm, character, isCurrent);
        if (isCurrent && !Replay(*current)) throw std::runtime_error("Files restored, but action bars need a fresh character login.");
      } catch (const std::exception &error) {
        Log(error.what());
        return Reply(L, false, error.what(), isCurrent && !store->muted.empty());
      }
      store->problem.clear();
      return Reply(L, true, isCurrent ? "Official local files and server action bars restored. Reloading UI." : "Character/profile restored. Previous isolated settings are in the local backups folder.", isCurrent);
    }
    return Reply(L, false, "Unknown zProfiles command.");
  } catch (const std::exception &error) { Log(error.what()); return Reply(L, false, error.what()); }
    catch (...) { Log("Unexpected native command failure."); return Reply(L, false, "Unexpected native command failure."); }
}
int __fastcall GetTogglesHook(void *L) {
  try {
    std::lock_guard<std::recursive_mutex> lock(store->mutex);
    auto session = CurrentSession();
    if (session && store->Isolated(session->account) && session->togglesSeeded)
      PlayerFlags(session->guid, &session->toggles, true);
  } catch (...) { Fault("Cannot apply local bar-layout flags."); }
  return originalGetToggles(L);
}
void __fastcall RegisterHook(const char *name, CFunction function) {
  if (name && !std::strcmp(name, "GetActionBarToggles") && function == At<CFunction>(0x004E7660)) {
    originalGetToggles = function;
    originalRegister(name, &GetTogglesHook);
    return;
  }
  originalRegister(name, function);
  // Register in both newly created glue and world Lua environments. Repeating
  // these cold registrations handles ReloadUI and requires no other DLL.
  if (name && (!std::strcmp(name, "GetBuildInfo") || !std::strcmp(name, "GetCVar") || !std::strcmp(name, "UnitName")))
    originalRegister("zProfilesNative", &Bridge);
}
bool Compatible() noexcept {
  __try {
    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x100000) return false;
    const auto nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != 0x4510B6DB || nt->OptionalHeader.SizeOfImage != 0x0092B000) return false;
    static const uint8_t send[] = {0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x83,0x7E,0x70,0x06,0x57,0x75,0x78};
    static const uint8_t dispatch[] = {0x55,0x8B,0xEC,0x8B,0x15,0x6C,0xD4,0xC0,0x00,0x53,0x8B,0x5D,0x0C,0x56,0x57,0x8D};
    static const uint8_t registration[] = {0x56,0x57,0x8B,0xF9,0xE8,0xA7,0xFF,0xFF,0xFF,0x8B,0xF0,0x6A,0x00,0x8B,0xCE};
    static const uint8_t flags[] = {0x8B,0x80,0x68,0x0E,0x00,0x00,0x8A,0x98,0x2A,0x10,0x00,0x00};
    return std::memcmp(At<void *>(0x005379A0), send, sizeof(send)) == 0 &&
      std::memcmp(At<void *>(0x00537AA0), dispatch, sizeof(dispatch)) == 0 &&
      std::memcmp(At<void *>(0x00704120), registration, sizeof(registration)) == 0 &&
      std::memcmp(At<void *>(0x004E7686), flags, sizeof(flags)) == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
DWORD Install() {
  image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  wchar_t path[32768]{};
  if (!GetModuleFileNameW(nullptr, path, 32768)) return 2;
  if (!Compatible()) throw std::runtime_error("Unsupported or already hooked client. Load zProfilesNative.dll FIRST in dlls.txt; only WoW 1.12.1.5875 is supported.");
  HMODULE pinned = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(&Install), &pinned)) throw std::runtime_error("Cannot pin the native protection module.");
  store = std::make_unique<zp::Store>(zp::fs::path(path).parent_path());
  zp::files::reportFailure = &Fault;
  if (MH_Initialize() != MH_OK) throw std::runtime_error("Cannot initialize native hooks.");
  if (MH_CreateHook(At<void *>(0x005379A0), reinterpret_cast<void *>(&SendHook), reinterpret_cast<void **>(&originalSend)) != MH_OK ||
      MH_CreateHook(At<void *>(0x00537AA0), reinterpret_cast<void *>(&DispatchHook), reinterpret_cast<void **>(&originalDispatch)) != MH_OK ||
      MH_CreateHook(At<void *>(0x00704120), reinterpret_cast<void *>(&RegisterHook), reinterpret_cast<void **>(&originalRegister)) != MH_OK ||
      !zp::files::Install(store.get()) || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
    throw std::runtime_error("Protection could not be installed completely. Startup stopped before login.");
  }
  Log("zProfiles 0.1.0 installed; account modes default unless explicitly isolated.");
  return 0;
}
} // namespace

extern "C" __declspec(dllexport) DWORD Load() {
  LONG previous = InterlockedCompareExchange(&installState, 1, 0);
  if (previous != 0) {
    while (InterlockedCompareExchange(&installState, 0, 0) == 1) Sleep(1);
    return installResult;
  }
  try { installResult = Install(); }
  catch (const std::exception &error) {
    installResult = 3;
    Log(error.what());
    MessageBoxA(nullptr, error.what(), "zProfiles - protection unavailable", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
  } catch (...) { installResult = 4; Log("Native initialization failed."); }
  InterlockedExchange(&installState, 2);
  // Continuing a configured client after a partial install could upload
  // settings unprotected. This runs at launcher startup, before login.
  if (installResult) ExitProcess(installResult);
  return installResult;
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
  // Keep routing state alive through other DLLs' process-exit destructors.
  // The OS retires the locks/allocation with the process. The module is pinned
  // at startup, so unsupported mid-session FreeLibrary cannot strand hooks.
  if (reason == DLL_PROCESS_DETACH && reserved) store.release();
  return TRUE; // VanillaFixes calls Load synchronously, outside loader lock.
}
