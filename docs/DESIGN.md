The native implementation now lives in [zabuu/zAPI](https://github.com/zabuu/zAPI). `zAPI.dll` provides the existing `zProfilesNative()` Lua interface; the interface name does not imply a separate DLL. One shared initialization path owns file/network hooks and Lua registration.

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
- `0x209 SMSG_ACCOUNT_DATA_TIMES`: this client reads eight 16-byte MD5 digests. zAPI 3.4 supplies the local cache digests so binding/settings initialization proceeds using the isolated files. The old 32-byte timestamp assumption was incorrect.
- `0x20C SMSG_UPDATE_ACCOUNT_DATA`: drop the cache payload.
- `0x129 SMSG_ACTION_BUTTONS`: retain the official 480-byte baseline, then substitute the local array before dispatch.

Other messages pass through. A settings decoding or storage error stays blocked; there is no error fallback that uploads local settings. The hook chain's original functions remain the normal path for forwarded messages.

The four multibar flags occupy the low nibble read by the audited `Script_GetActionBarToggles` through `player+0xE68`, then `fields+0x102A`. The stock setter sends only a packet, so suppression must also update the local field. The getter reapplies the cached local nibble in case a later server field update includes the official flags.

## Revert lifecycle

Revert stages a fresh copy from original files and archives the working directory before activation. It also archives/reset local bar checkpoints. For a live character it mutes old settings writes, applies retained official slots through the local action setter with uploads disabled, emits a full bar refresh, and requests `ReloadUI`.

The audited ReloadUI teardown saves and destroys native macro state and destroys binding state. UI initialization reads their account-data files again. Old file writes are routed to a discard directory, and old settings packets cannot overwrite the fresh bar checkpoint. After the new UI reaches `PLAYER_ENTERING_WORLD`, the addon resumes profile writes.

The latest original directory supplies file baselines. Official action-slot baselines are the server arrays received on login. These are intentionally distinct sources: arbitrary addon files from a different computer are not available to a client addon.

## Named setup lifecycle (0.2.0)

The original bridge version remains 1; `setupVersion` advertises the setup contract independently, and `liveBarsVersion` identifies verified live-slot capture/restoration. An old DLL can still protect accounts while the updated Lua panel directs the user to replace it before using setups.

Setups are scoped to the authenticated account and current character GUID at `WTF/zProfiles/setups/<ACCOUNT>/<GUID>/<name>.zps`. Names are restricted to 1-32 letters, numbers, spaces, hyphens and underscores; device names, path separators and leading/trailing spaces are rejected. Files contain a versioned header, all 120 raw action slots, a visibility nibble, hex-encoded key/command pairs and a corruption checksum. The checksum detects accidental damage; it is not an authentication mechanism. Reads never execute Lua. Writes flush and atomically replace the snapshot, archiving prior copies. Rename cannot overwrite another setup; Delete archives the file.

Save captures the live action table and Lua's binding enumeration, including secondary keys and available direct-action bindings. Apply reads and validates bindings, captures the current unsaved live setup in the separate `.undo.zps` snapshot, clears the old key list, sets the target keys, and saves character binding set 2. The audited stock `SaveBindings(2)` sets the active binding set to 2 as well as saving it. The native bridge rereads the snapshot and verifies the binding payload still matches before applying its bars through the local slot setter with uploads disabled, updating flags/checkpoints and sending one full bar refresh. It verifies the resulting live slots before reporting success. There is no outgoing action edit or UI reload. A failed binding/native apply restores previous bindings and their previous binding-set scope; native failures attempt to restore the prior live bars/flags. An unrecoverable native rollback records a profile fault.

Undo loads the pre-switch snapshot without overwriting it. Named snapshots and Undo live outside the cloned account directory, so character/account Revert does not delete them. Default accounts, uninitialized characters, muted reloads and faulted profiles cannot use native setup operations. Lua also blocks combat and occupied-cursor operations. Snapshots contain action references; macro contents and talent state are not copied. Server-driven spec changes continue through the existing isolated-action baseline handling, and the user manually applies the desired saved setup afterward.

Explicit Save/Update is durable across termination; a complete binding/bar switch spans Lua and native cache writes and is not a single cross-file transaction. Failure recovery and a retained Undo reduce inconsistent states, while the original account/server isolation boundary remains in force. Cross-spec runtime behavior still needs the live checks in VALIDATION.md.

## Scope of validation

The native executable tests include the production adapter and install the production Win32 hooks into a disposable test process. They exercise actual redirected file operations, capture forwarded messages with mock downstream functions, and force-terminate a child process. They do not simulate a live server's login sequence or prove coexistence with every client mod. Those checks are tracked separately in VALIDATION.md.

Protocol evidence: [CMaNGOS Classic settings handlers](https://github.com/cmangos/mangos-classic/blob/master/src/game/Entities/MiscHandler.cpp). Loader evidence: [VanillaFixes additional DLL initialization](https://github.com/hannesmann/vanillafixes/blob/main/src/dll/dll.c). Client addresses and reload call sites were audited directly against the pinned executable; client code is not redistributed.
