// Include the production adapter so tests exercise its fail-closed decisions,
// not a second implementation. No game process or real account is accessed.
#include "../native/zProfilesNative.cpp"
#include <iostream>

namespace {
int checks = 0, sent = 0, received = 0;
zp::Bars delivered{};
void Check(bool condition, const char *label) {
  ++checks;
  if (!condition) throw std::runtime_error(label);
}
void __fastcall Sent(void *, void *, Packet *) { ++sent; }
void __fastcall Received(void *, void *, uint32_t, Packet *packet) {
  ++received;
  uint8_t *data = nullptr; size_t n = 0;
  if (View(packet, &data, &n) && n == 2 + sizeof(zp::Bars)) std::memcpy(delivered.data(), data + 2, sizeof(zp::Bars));
}
std::string Contents(const zp::fs::path &path) {
  auto bytes = zp::ReadBytes(path);
  return {bytes.begin(), bytes.end()};
}
void Write(const zp::fs::path &path, const std::string &text, bool ansi = false) {
  HANDLE handle = ansi ? CreateFileA(zp::Ansi(path.wstring()).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) :
    CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  Check(handle != INVALID_HANDLE_VALUE, "opening a client settings file");
  DWORD written = 0;
  bool ok = WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
  CloseHandle(handle);
  Check(ok, "writing a client settings file");
}
std::vector<uint8_t> Message(uint32_t opcode, const void *body, size_t length, size_t opcodeSize) {
  std::vector<uint8_t> bytes(opcodeSize + length);
  std::memcpy(bytes.data(), &opcode, opcodeSize);
  if (length) std::memcpy(bytes.data() + opcodeSize, body, length);
  return bytes;
}
Packet Wrap(std::vector<uint8_t> &bytes) {
  return {reinterpret_cast<void *>(1), bytes.data(), 0, static_cast<uint32_t>(bytes.size()), static_cast<uint32_t>(bytes.size()), 0};
}
struct HookScope {
  HookScope() {
    Check(MH_Initialize() == MH_OK, "MinHook initializes");
    Check(zp::files::Install(store.get()), "production file hooks install");
    Check(MH_EnableHook(MH_ALL_HOOKS) == MH_OK, "production file hooks enable");
  }
  ~HookScope() { MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); zp::files::store = nullptr; }
};
void Fixture(const zp::fs::path &root) {
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/macros-cache.txt", "official account macro");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Testchar/layout-cache.txt", "official character layout");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Testchar/macros-cache.txt", "official character macro");
  zp::AtomicWrite(root / L"WTF/Account/TESTACCOUNT/Realm/Otherchar/layout-cache.txt", "official other layout");
  zp::AtomicWrite(root / L"WTF/Account/NORMALACCOUNT/bindings-cache.wtf", "default bindings");
  zp::Store initial(root);
  Check(initial.active.empty(), "no profile means Default");
  initial.Mode(L"testaccount", true);
  Check(!initial.Isolated(L"TESTACCOUNT"), "mode changes do not enable isolation mid-session");
}
void Tests(const zp::fs::path &root) {
  Check(!zp::Segment(L"..") && !zp::Segment(L"CON.txt") && !zp::Segment(L"x\\y"), "unsafe path segments rejected");
  Check(zp::Segment(L"Realm Name") && zp::Account(L"TEST-1"), "normal identities accepted");
  Check(!zp::Within(L"C:\\WTF\\AccountElse\\A", L"C:\\WTF\\Account"), "directory boundary respected");
  store = std::make_unique<zp::Store>(root);
  auto original = store->original / L"TESTACCOUNT";
  auto character = original / L"Realm/Testchar";
  auto profile = store->Profile(L"TESTACCOUNT");
  Check(store->Isolated(L"TESTACCOUNT"), "isolation activates in the next process");
  Check(store->Route((original / L"macros-cache.txt").wstring(), false) == (profile / L"macros-cache.txt").wstring(), "isolated reads select clone");
  Check(Contents(profile / L"macros-cache.txt") == "official account macro", "clone seeded from original");
  Check(store->Route((store->original / L"NORMALACCOUNT/bindings-cache.wtf").wstring(), true) == (store->original / L"NORMALACCOUNT/bindings-cache.wtf").wstring(), "Default files pass through");
  {
    zp::Store other(root);
    bool refused = false;
    try { other.EnsureProfile(L"TESTACCOUNT"); } catch (...) { refused = true; }
    Check(refused, "a second client cannot share an isolated account");
  }
  HookScope hooks;
  Write(original / L"macros-cache.txt", "isolated macro", true);
  Write(character / L"layout-cache.txt", "isolated layout");
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "ANSI writes preserve official macros");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "ANSI writes reach clone");
  {
    std::ofstream stream(original / L"macros-cache.txt", std::ios::binary | std::ios::trunc);
    stream << "isolated macro";
  }
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "C runtime stream writes preserve official macros");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "C runtime stream writes reach clone");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "Unicode writes preserve official layout");
  Write(store->original / L"NORMALACCOUNT/bindings-cache.wtf", "changed default");
  Check(Contents(store->original / L"NORMALACCOUNT/bindings-cache.wtf") == "changed default", "Default writes keep normal behavior");
  Check(CreateDirectoryW((character / L"NewFolder").c_str(), nullptr) != 0, "new folders route to clone");
  Write(character / L"NewFolder/inside.txt", "new file");
  Check(!zp::fs::exists(profile / L"Realm/Testchar/missing.txt"), "unrelated paths are not fabricated");
  Check(CopyFileW((character / L"layout-cache.txt").c_str(), (character / L"copy.txt").c_str(), TRUE) != 0, "copy routes both paths");
  Check(MoveFileExW((character / L"copy.txt").c_str(), (character / L"renamed.txt").c_str(), MOVEFILE_WRITE_THROUGH) != 0, "rename stays in clone");
  Check(DeleteFileW((character / L"renamed.txt").c_str()) != 0, "delete stays in clone");
  Write(character / L"replacement.txt", "atomic replacement");
  Check(ReplaceFileW((character / L"layout-cache.txt").c_str(), (character / L"replacement.txt").c_str(), nullptr, 0, nullptr, nullptr) != 0, "atomic file replacement stays in clone");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "atomic file replacement preserves original");
  Check(CopyFile2((character / L"layout-cache.txt").c_str(), (character / L"copy2.txt").c_str(), nullptr) == S_OK, "modern CopyFile2 stays in clone");
  Check(GetFileAttributesW((character / L"NewFolder/inside.txt").c_str()) != INVALID_FILE_ATTRIBUTES, "attribute queries use clone");
  WIN32_FIND_DATAW found{};
  HANDLE find = FindFirstFileW((character / L"NewFolder/*").c_str(), &found);
  Check(find != INVALID_HANDLE_VALUE, "directory enumeration sees clone");
  FindClose(find);
  wchar_t previous[32768]{}; GetCurrentDirectoryW(32768, previous);
  Check(SetCurrentDirectoryW(character.c_str()) != 0, "client can change working directory");
  Write(L"layout-cache.txt", "relative isolated layout");
  Check(SetCurrentDirectoryW(previous) != 0, "working directory restored");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "bare relative writes preserve original");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "relative isolated layout", "bare relative writes reach clone");

  originalSend = &Sent; originalDispatch = &Received;
  int connection = 0;
  currentConnection = &connection;
  sessions.clear(); sessions[&connection].account = L"TESTACCOUNT";
  auto &session = sessions[&connection];
  uint64_t guid = 42;
  auto loginBytes = Message(zp::PlayerLogin, &guid, sizeof(guid), 4); auto login = Wrap(loginBytes);
  SendHook(&connection, nullptr, &login);
  Check(sent == 1 && session.guid == guid, "login forwards and captures exact character GUID");
  zp::Bars official{}; official[0] = 133; official[1] = 0x40000007;
  auto barsBytes = Message(zp::ActionButtons, official.data(), sizeof(official), 2); auto barsPacket = Wrap(barsBytes);
  DispatchHook(&connection, nullptr, 7, &barsPacket);
  Check(received == 1 && session.ready && delivered == official, "server action bars seed first profile");
  uint8_t edit[5]{0}; uint32_t action = 116;
  std::memcpy(edit + 1, &action, 4);
  auto editBytes = Message(zp::SetActionButton, edit, sizeof(edit), 4); auto editPacket = Wrap(editBytes);
  SendHook(&connection, nullptr, &editPacket);
  Check(sent == 1 && session.bars[0] == 116, "action edit is checkpointed and never forwarded");
  zp::Bars saved{};
  Check(store->LoadBars(L"TESTACCOUNT", guid, false, saved) && saved[0] == 116 && saved[1] == official[1], "action checkpoint preserves unedited macro slots");
  auto spellBytes = Message(0x12E, &action, 4, 4); auto spellPacket = Wrap(spellBytes);
  SendHook(&connection, nullptr, &spellPacket);
  Check(sent == 2, "normal casting remains untouched");
  auto uploadBytes = Message(zp::UpdateAccountData, &action, 4, 4); auto uploadPacket = Wrap(uploadBytes);
  SendHook(&connection, nullptr, &uploadPacket);
  Check(sent == 2, "account-data uploads blocked");
  auto requestBytes = Message(zp::RequestAccountData, &action, 4, 4); auto requestPacket = Wrap(requestBytes);
  SendHook(&connection, nullptr, &requestPacket);
  Check(sent == 2, "server cache downloads not requested in isolation");
  uint8_t toggle = 9;
  auto toggleBytes = Message(zp::ActionBarToggles, &toggle, 1, 4); auto togglePacket = Wrap(toggleBytes);
  SendHook(&connection, nullptr, &togglePacket);
  Check(sent == 2, "bar-layout server writes blocked");
  uint32_t times[8]{1,2,3,4,5,6,7,8};
  auto timeBytes = Message(zp::AccountDataTimes, times, sizeof(times), 2); auto timePacket = Wrap(timeBytes);
  DispatchHook(&connection, nullptr, 7, &timePacket);
  bool zero = true; for (size_t i = 2; i < timeBytes.size(); ++i) zero = zero && !timeBytes[i];
  Check(zero && received == 2, "account timestamps cannot override the local clone");
  auto downloadBytes = Message(zp::ServerAccountData, &action, 4, 2); auto downloadPacket = Wrap(downloadBytes);
  DispatchHook(&connection, nullptr, 7, &downloadPacket);
  Check(received == 2, "server cache payloads do not overwrite isolated settings");
  auto newer = official; newer[2] = 999;
  barsBytes = Message(zp::ActionButtons, newer.data(), sizeof(newer), 2); barsPacket = Wrap(barsBytes);
  DispatchHook(&connection, nullptr, 7, &barsPacket);
  Check(delivered[0] == 116 && delivered[2] == 0, "future logins preserve isolated slots");
  Check(store->LoadBars(L"TESTACCOUNT", guid, true, saved) && saved[2] == 999, "latest server baseline retained for Revert");
  auto badBytes = Message(zp::SetActionButton, edit, 4, 4); auto badPacket = Wrap(badBytes);
  SendHook(&connection, nullptr, &badPacket);
  Check(sent == 2 && !store->problem.empty(), "malformed settings fail closed");
  {
    zp::Bypass guard;
    SetFileAttributesW(store->Journal(L"TESTACCOUNT", guid, false).c_str(), FILE_ATTRIBUTE_READONLY);
  }
  SendHook(&connection, nullptr, &editPacket);
  Check(sent == 2, "checkpoint failures do not leak settings to server");
  {
    zp::Bypass guard;
    SetFileAttributesW(store->Journal(L"TESTACCOUNT", guid, false).c_str(), FILE_ATTRIBUTE_NORMAL);
  }
  session.realm = L"Realm"; session.character = L"Testchar";
  store->Identify(session.account, session.realm, session.character, guid);
  Write(original / L"Realm/Otherchar/layout-cache.txt", "isolated other layout");
  store->Revert(L"TESTACCOUNT", L"Realm", L"Testchar", true);
  Check(Replay(session), "current-character Revert replays official bars through client receive adapter");
  Check(delivered == newer, "Revert restores latest official action bars");
  SendHook(&connection, nullptr, &editPacket);
  Check(store->LoadBars(L"TESTACCOUNT", guid, false, saved) && saved == newer, "old UI action saves cannot overwrite reverted bar checkpoint");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "official character layout", "character Revert restores original files");
  Check(Contents(profile / L"macros-cache.txt") == "isolated macro", "character Revert preserves shared account macros");
  Check(Contents(profile / L"Realm/Otherchar/layout-cache.txt") == "isolated other layout", "character Revert preserves other characters");
  Write(character / L"layout-cache.txt", "stale Lua logout write");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "official character layout", "old UI saves cannot overwrite Revert during reload");
  store->muted.clear();
  Write(character / L"layout-cache.txt", "fresh UI layout");
  Check(Contents(profile / L"Realm/Testchar/layout-cache.txt") == "fresh UI layout", "new UI can save after resume");
  store->Revert(L"TESTACCOUNT", L"", L"", false);
  Check(Contents(profile / L"macros-cache.txt") == "official account macro", "account Revert restores shared macros");
  Check(Contents(profile / L"Realm/Otherchar/layout-cache.txt") == "official other layout", "account Revert covers all character files");
  Check(zp::fs::exists(store->data / L"backups"), "Reverts retain backups");
  session.account = L"NORMALACCOUNT";
  SendHook(&connection, nullptr, &editPacket);
  DispatchHook(&connection, nullptr, 7, &downloadPacket);
  Check(sent == 3 && received == 5, "Default account network writes and cache reads pass through");
  store->Mode(L"TESTACCOUNT", false);
  Check(store->Isolated(L"TESTACCOUNT"), "disabling isolation does not upload an in-memory isolated session");
  store->Mode(L"TESTACCOUNT", true); // retain isolation for abrupt-exit child
  Check(Contents(original / L"macros-cache.txt") == "official account macro", "official account macros unchanged after full suite");
  Check(Contents(character / L"layout-cache.txt") == "official character layout", "official layout unchanged after full suite");
}
int CrashChild(const zp::fs::path &root) {
  store = std::make_unique<zp::Store>(root);
  HookScope hooks;
  Write(store->original / L"TESTACCOUNT/macros-cache.txt", "crash-session macro");
  zp::Bars bars{}; bars[3] = 12345;
  store->SaveBars(L"TESTACCOUNT", 42, false, bars);
  // Deliberately bypass destructors and every logout/save event.
  TerminateProcess(GetCurrentProcess(), 123);
  return 1;
}
void AbruptExit(const zp::fs::path &root, const std::wstring &executable) {
  std::wstring command = L"\"" + executable + L"\" --crash \"" + root.wstring() + L"\"";
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  Check(CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0, "abrupt-exit child starts");
  Check(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0, "abrupt-exit child finishes");
  DWORD status = 0; GetExitCodeProcess(process.hProcess, &status);
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  Check(status == 123, "child was killed without clean-up");
  zp::Store restarted(root);
  restarted.EnsureProfile(L"TESTACCOUNT");
  Check(Contents(restarted.original / L"TESTACCOUNT/macros-cache.txt") == "official account macro", "forced termination leaves official macros untouched");
  Check(Contents(restarted.Profile(L"TESTACCOUNT") / L"macros-cache.txt") == "crash-session macro", "isolated file survives forced termination");
  zp::Bars bars{};
  Check(restarted.LoadBars(L"TESTACCOUNT", 42, false, bars) && bars[3] == 12345, "action checkpoint survives forced termination and restart");
  zp::AtomicWrite(restarted.data / L"modes.txt", "corrupt registry");
  bool refused = false;
  try { zp::Store invalid(root); } catch (...) { refused = true; }
  Check(refused, "corrupt registry cannot silently revert to Default");
}
}

int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 3 && std::wstring(argv[1]) == L"--crash") return CrashChild(argv[2]);
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
    auto root = zp::fs::absolute(zp::fs::path(executable).parent_path() / L"../../.test-data" / std::to_wstring(GetTickCount64()));
    Fixture(root);
    Tests(root);
    store.reset(); sessions.clear();
    AbruptExit(root, executable);
    std::cout << "PASS: " << checks << " native checks, including real Win32 file detours and forced process termination.\n";
    return 0;
  } catch (const std::exception &error) {
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); zp::files::store = nullptr;
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
