# Native protection contract

The Lua addon is a controller and account/character browser. `zProfilesNative(command, ...)` exposes versioned operations returning primitive values. It never executes Lua supplied from disk or by another client.

## Mount lifecycle

`Load` runs synchronously through VanillaFixes before game initialization. It verifies the client, loads `modes.txt`, installs the whole hook set, and then permits startup. Failure terminates startup rather than allowing configured accounts to upload settings unprotected.

The active account modes are immutable for the process lifetime. UI mode changes atomically save the desired modes for the next process. They cannot turn off protection while isolated state remains in memory.

The first isolated access obtains an exclusive per-account file lock and clones the original directory into a staging directory. A readiness marker is written before the directory is renamed into place. An incomplete clone is not used. Originals are copied by value, never hardlinked.

Win32 ANSI and wide file APIs route paths under `WTF/Account/<isolated account>` to `WTF/zProfiles/profiles/<account>`. Open/create, metadata, enumeration, copy, move, and delete paths are covered. Changing the process working directory is tracked so bare relative paths under an account still route correctly. Internal storage operations use a thread-local bypass to avoid redirecting their own reads of official files. Unconfigured account paths pass through unchanged.

## Protocol boundary

The audited 1.12.1 client exposes:

| Entry | Address / ABI |
| --- | --- |
| NetClient send funnel | `0x5379A0`, thiscall `(connection, CDataStore*)`, void |
| NetClient message dispatch | `0x537AA0`, thiscall `(connection, uint32, CDataStore*)`, void |
| Lua function registration | `0x704120`, fastcall `(name, CFunction)` |

`CDataStore` is a 24-byte polymorphic x86 structure: vtable, buffer, base, capacity, size, cursor. Outgoing opcodes occupy four bytes; incoming opcodes occupy two. Adapter reads do not advance the cursor.

The authenticated account is taken from `CMSG_AUTH_SESSION`; the effective account-directory global is a fallback. `CMSG_PLAYER_LOGIN` starts a new GUID-specific session. Logout completion retires that character's live replay context.

For isolated accounts:

- `0x128 CMSG_SET_ACTION_BUTTON`: checkpoint the edited 120-slot local array, drop the message.
- `0x2BF CMSG_SET_ACTIONBAR_TOGGLES`: checkpoint the flags, update the local field, drop the message.
- `0x20B CMSG_UPDATE_ACCOUNT_DATA`: drop the message.
- `0x20A CMSG_REQUEST_ACCOUNT_DATA`: drop the request.
- `0x209 SMSG_ACCOUNT_DATA_TIMES`: mask the eight timestamps to zero so the clone stays authoritative.
- `0x20C SMSG_UPDATE_ACCOUNT_DATA`: drop the cache payload.
- `0x129 SMSG_ACTION_BUTTONS`: retain the official 480-byte baseline, then substitute the local array before dispatch.

Other messages pass through. A settings decoding or storage error stays blocked; there is no error fallback that uploads local settings. The hook chain's original functions remain the normal path for forwarded messages.

The four multibar flags occupy the low nibble read by the audited `Script_GetActionBarToggles` through `player+0xE68`, then `fields+0x102A`. The stock setter sends only a packet, so suppression must also update the local field. The getter reapplies the cached local nibble in case a later server field update includes the official flags.

## Revert lifecycle

Revert stages a fresh copy from original files and archives the working directory before activation. It also archives/reset local bar checkpoints. For a live character it mutes old settings writes, replays the retained official bar message through the normal client dispatcher, and requests `ReloadUI`.

The audited ReloadUI teardown saves and destroys native macro state and destroys binding state. UI initialization reads their account-data files again. Old file writes are routed to a discard directory, and old settings packets cannot overwrite the fresh bar checkpoint. After the new UI reaches `PLAYER_ENTERING_WORLD`, the addon resumes profile writes.

The latest original directory supplies file baselines. Official action-slot baselines are the server arrays received on login. These are intentionally distinct sources: arbitrary addon files from a different computer are not available to a client addon.

## Scope of validation

The native executable tests include the production adapter and install the production Win32 hooks into a disposable test process. They exercise actual redirected file operations, capture forwarded messages with mock downstream functions, and force-terminate a child process. They do not simulate a live server's login sequence or prove coexistence with every client mod. Those checks are tracked separately in VALIDATION.md.

Protocol evidence: [CMaNGOS Classic settings handlers](https://github.com/cmangos/mangos-classic/blob/master/src/game/Entities/MiscHandler.cpp). Loader evidence: [VanillaFixes additional DLL initialization](https://github.com/hannesmann/vanillafixes/blob/main/src/dll/dll.c). Client addresses and reload call sites were audited directly against the pinned executable; client code is not redistributed.
