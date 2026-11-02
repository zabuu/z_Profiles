#pragma once
#include "Store.h"
#include "MinHook.h"

namespace zp::files {
inline Store *store = nullptr;
inline void (*reportFailure)(const char *) = nullptr;
inline volatile LONG accountCwd = 0;
inline void UpdateCwd() {
  wchar_t cwd[32768]{};
  if (store && GetCurrentDirectoryW(32768, cwd))
    InterlockedExchange(&accountCwd, Within(cwd, store->original.wstring()) ? 1 : 0);
}
inline bool Relative(const std::wstring &path) {
  return !path.empty() && path[0] != L'\\' && (path.size() < 2 || path[1] != L':');
}
inline void Failure() noexcept {
  try {
    const char *message = "Profile file access failed. Settings uploads remain blocked; see zProfilesNative.log.";
    if (reportFailure) reportFailure(message);
    else if (store) { std::lock_guard<std::recursive_mutex> lock(store->mutex); store->problem = message; }
  } catch (...) {}
  SetLastError(ERROR_ACCESS_DENIED);
}
inline std::wstring Map(const wchar_t *path, bool write) {
  if (!path || !store || bypass || store->active.empty()) return path ? path : L"";
  std::wstring input(path);
  if (Upper(input).find(L"ACCOUNT") == std::wstring::npos && !(accountCwd && Relative(input))) return input;
  return store->Route(input, write);
}
inline std::string Map(const char *path, bool write) {
  if (!path || !store || bypass || store->active.empty()) return path ? path : "";
  // Keep unrelated asset opens on the ordinary path without a conversion.
  std::string hint(path);
  for (auto &c : hint) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (hint.find("ACCOUNT") == std::string::npos && !(accountCwd && hint[0] != '\\' && (hint.size() < 2 || hint[1] != ':'))) return path;
  return Ansi(store->Route(Wide(path, CP_ACP), write));
}
inline bool Writes(DWORD access, DWORD disposition) {
  return disposition != OPEN_EXISTING || (access & (GENERIC_WRITE | GENERIC_ALL | DELETE | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)) != 0;
}

#define ZP_CREATE(S, T) \
inline decltype(&::CreateFile##S) create##S = nullptr; \
inline HANDLE WINAPI Create##S(const T *p, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t) { \
  try { auto m = Map(p, Writes(a,d)); return create##S(p ? m.c_str() : nullptr,a,s,sa,d,f,t); } catch (...) { Failure(); return INVALID_HANDLE_VALUE; } }
ZP_CREATE(A, char)
ZP_CREATE(W, wchar_t)
#undef ZP_CREATE

#define ZP_ATTR(S, T) \
inline decltype(&::GetFileAttributes##S) attr##S = nullptr; \
inline DWORD WINAPI Attr##S(const T *p) { try { auto m=Map(p,false); return attr##S(p ? m.c_str() : nullptr); } catch (...) { Failure(); return INVALID_FILE_ATTRIBUTES; } } \
inline decltype(&::GetFileAttributesEx##S) attrEx##S = nullptr; \
inline BOOL WINAPI AttrEx##S(const T *p, GET_FILEEX_INFO_LEVELS level, LPVOID info) { try { auto m=Map(p,false); return attrEx##S(p ? m.c_str() : nullptr,level,info); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::SetFileAttributes##S) setAttr##S = nullptr; \
inline BOOL WINAPI SetAttr##S(const T *p, DWORD attrs) { try { auto m=Map(p,true); return setAttr##S(p ? m.c_str() : nullptr,attrs); } catch (...) { Failure(); return FALSE; } }
ZP_ATTR(A, char)
ZP_ATTR(W, wchar_t)
#undef ZP_ATTR

#define ZP_DIR(S, T) \
inline decltype(&::CreateDirectory##S) dir##S = nullptr; \
inline BOOL WINAPI Dir##S(const T *p, LPSECURITY_ATTRIBUTES sa) { try { auto m=Map(p,true); return dir##S(p ? m.c_str() : nullptr,sa); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::RemoveDirectory##S) removeDir##S = nullptr; \
inline BOOL WINAPI RemoveDir##S(const T *p) { try { auto m=Map(p,true); return removeDir##S(p ? m.c_str() : nullptr); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::DeleteFile##S) del##S = nullptr; \
inline BOOL WINAPI Del##S(const T *p) { try { auto m=Map(p,true); return del##S(p ? m.c_str() : nullptr); } catch (...) { Failure(); return FALSE; } }
ZP_DIR(A, char)
ZP_DIR(W, wchar_t)
#undef ZP_DIR

#define ZP_CWD(S, T) \
inline decltype(&::SetCurrentDirectory##S) cwd##S = nullptr; \
inline BOOL WINAPI Cwd##S(const T *p) { auto ok=cwd##S(p); if (ok) { try { UpdateCwd(); } catch (...) { Failure(); } } return ok; }
ZP_CWD(A, char)
ZP_CWD(W, wchar_t)
#undef ZP_CWD

#define ZP_FIND(S, T, D) \
inline decltype(&::FindFirstFile##S) find##S = nullptr; \
inline HANDLE WINAPI Find##S(const T *p, D *data) { try { auto m=Map(p,false); return find##S(p ? m.c_str() : nullptr,data); } catch (...) { Failure(); return INVALID_HANDLE_VALUE; } } \
inline decltype(&::FindFirstFileEx##S) findEx##S = nullptr; \
inline HANDLE WINAPI FindEx##S(const T *p, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS search, LPVOID filter, DWORD flags) { try { auto m=Map(p,false); return findEx##S(p ? m.c_str() : nullptr,level,data,search,filter,flags); } catch (...) { Failure(); return INVALID_HANDLE_VALUE; } }
ZP_FIND(A, char, WIN32_FIND_DATAA)
ZP_FIND(W, wchar_t, WIN32_FIND_DATAW)
#undef ZP_FIND

#define ZP_MOVE(S, T) \
inline decltype(&::MoveFile##S) move##S = nullptr; \
inline BOOL WINAPI Move##S(const T *a, const T *b) { try { auto x=Map(a,true), y=Map(b,true); return move##S(a ? x.c_str() : nullptr,b ? y.c_str() : nullptr); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::MoveFileEx##S) moveEx##S = nullptr; \
inline BOOL WINAPI MoveEx##S(const T *a, const T *b, DWORD flags) { try { auto x=Map(a,true), y=Map(b,true); return moveEx##S(a ? x.c_str() : nullptr,b ? y.c_str() : nullptr,flags); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::CopyFile##S) copy##S = nullptr; \
inline BOOL WINAPI Copy##S(const T *a, const T *b, BOOL fail) { try { auto x=Map(a,false), y=Map(b,true); return copy##S(a ? x.c_str() : nullptr,b ? y.c_str() : nullptr,fail); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::CopyFileEx##S) copyEx##S = nullptr; \
inline BOOL WINAPI CopyEx##S(const T *a, const T *b, LPPROGRESS_ROUTINE progress, LPVOID data, LPBOOL cancel, DWORD flags) { try { auto x=Map(a,false), y=Map(b,true); return copyEx##S(a ? x.c_str() : nullptr,b ? y.c_str() : nullptr,progress,data,cancel,flags); } catch (...) { Failure(); return FALSE; } }
ZP_MOVE(A, char)
ZP_MOVE(W, wchar_t)
#undef ZP_MOVE

#define ZP_REPLACE(S, T) \
inline decltype(&::ReplaceFile##S) replace##S = nullptr; \
inline BOOL WINAPI Replace##S(const T *a, const T *b, const T *backup, DWORD flags, LPVOID exclude, LPVOID reserved) { \
  try { auto x=Map(a,true), y=Map(b,true), z=Map(backup,true); return replace##S(a ? x.c_str() : nullptr,b ? y.c_str() : nullptr,backup ? z.c_str() : nullptr,flags,exclude,reserved); } catch (...) { Failure(); return FALSE; } } \
inline decltype(&::CreateDirectoryEx##S) dirEx##S = nullptr; \
inline BOOL WINAPI DirEx##S(const T *source, const T *target, LPSECURITY_ATTRIBUTES sa) { \
  try { auto x=Map(source,false), y=Map(target,true); return dirEx##S(source ? x.c_str() : nullptr,target ? y.c_str() : nullptr,sa); } catch (...) { Failure(); return FALSE; } }
ZP_REPLACE(A, char)
ZP_REPLACE(W, wchar_t)
#undef ZP_REPLACE

inline decltype(&::CopyFile2) copy2 = nullptr;
inline HRESULT WINAPI Copy2(PCWSTR source, PCWSTR target, COPYFILE2_EXTENDED_PARAMETERS *parameters) {
  try { auto a=Map(source,false), b=Map(target,true); return copy2(source ? a.c_str() : nullptr,target ? b.c_str() : nullptr,parameters); }
  catch (...) { Failure(); return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED); }
}

struct Hook { const char *name; void *detour; void **original; };
#define ZP_HOOK(Name, Detour, Original) {#Name, reinterpret_cast<void *>(&Detour), reinterpret_cast<void **>(&Original)}
inline bool Install(Store *target) {
  Hook hooks[] = {
    ZP_HOOK(CreateFileA,CreateA,createA), ZP_HOOK(CreateFileW,CreateW,createW),
    ZP_HOOK(GetFileAttributesA,AttrA,attrA), ZP_HOOK(GetFileAttributesW,AttrW,attrW),
    ZP_HOOK(GetFileAttributesExA,AttrExA,attrExA), ZP_HOOK(GetFileAttributesExW,AttrExW,attrExW),
    ZP_HOOK(SetFileAttributesA,SetAttrA,setAttrA), ZP_HOOK(SetFileAttributesW,SetAttrW,setAttrW),
    ZP_HOOK(CreateDirectoryA,DirA,dirA), ZP_HOOK(CreateDirectoryW,DirW,dirW),
    ZP_HOOK(SetCurrentDirectoryA,CwdA,cwdA), ZP_HOOK(SetCurrentDirectoryW,CwdW,cwdW),
    ZP_HOOK(RemoveDirectoryA,RemoveDirA,removeDirA), ZP_HOOK(RemoveDirectoryW,RemoveDirW,removeDirW),
    ZP_HOOK(DeleteFileA,DelA,delA), ZP_HOOK(DeleteFileW,DelW,delW),
    ZP_HOOK(FindFirstFileA,FindA,findA), ZP_HOOK(FindFirstFileW,FindW,findW),
    ZP_HOOK(FindFirstFileExA,FindExA,findExA), ZP_HOOK(FindFirstFileExW,FindExW,findExW),
    ZP_HOOK(MoveFileA,MoveA,moveA), ZP_HOOK(MoveFileW,MoveW,moveW),
    ZP_HOOK(MoveFileExA,MoveExA,moveExA), ZP_HOOK(MoveFileExW,MoveExW,moveExW),
    ZP_HOOK(CopyFileA,CopyA,copyA), ZP_HOOK(CopyFileW,CopyW,copyW),
    ZP_HOOK(CopyFileExA,CopyExA,copyExA), ZP_HOOK(CopyFileExW,CopyExW,copyExW)
    ,ZP_HOOK(ReplaceFileA,ReplaceA,replaceA), ZP_HOOK(ReplaceFileW,ReplaceW,replaceW)
    ,ZP_HOOK(CreateDirectoryExA,DirExA,dirExA), ZP_HOOK(CreateDirectoryExW,DirExW,dirExW)
    ,ZP_HOOK(CopyFile2,Copy2,copy2)
  };
  for (auto &hook : hooks) if (MH_CreateHookApi(L"kernel32.dll", hook.name, hook.detour, hook.original) != MH_OK) return false;
  store = target;
  UpdateCwd();
  return true; // caller enables its complete hook set atomically
}
#undef ZP_HOOK
} // namespace zp::files
