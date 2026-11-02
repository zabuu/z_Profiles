# Validation status

## Automated checks

Version 0.1.0: **87 native checks and 12 Lua tests pass**, including the forced-termination fixture. The pinned local client also passes the static audit. Compiler warnings are confined to the vendored MinHook C sources.

The native tests use only generated `TESTACCOUNT` / `NORMALACCOUNT` fixtures under ignored `.test-data` directories. They do not read the installation's real account directories.

Covered behavior includes:

- Unknown accounts and unconfigured installations use Default.
- Mode changes remain pending until a new process.
- Exclusive profile locks prevent two clients editing the same isolated account.
- Real ANSI/wide opens, writes, directory creation, queries, enumeration, copy, move, delete, and bare relative paths select the clone.
- Original macros and character files retain their contents.
- Production send/receive adapters block the specified settings messages and pass ordinary casting/default-account messages.
- Official action slots seed local slots; later server baselines update Revert without overwriting the local profile.
- Macro action-slot values and untouched slots are preserved in journals.
- Malformed settings and failed checkpoint replacement never reach the mock server.
- Per-character Revert preserves other characters and shared macros.
- Account Revert restores shared files and all character files.
- Old UI writes and action saves cannot undo a Revert during reload.
- Forced process termination skips destructors/logout handlers; originals and flushed local checkpoints survive the next process.
- Corrupt registry data cannot silently turn configured protection off.

Lua tests cover absent/incompatible backends, Default behavior, mode selection, pending-restart status, account/character pagination, current/offline Revert routing, partial-Revert reload, event setup, backend errors, and Lua 5.0 syntax restrictions.

`audit_client.py` verifies the exact executable hash, PE metadata, hook-entry signatures, Lua bridge entry points, multibar field access, and reload/save/destroy/load call sites for macros and bindings. Static inspection is separate from runtime validation.

## Remaining live checks

Use a disposable test account before relying on this release:

1. Start with the configured VanillaFixes loader; confirm `/zp` reports the native layer and Default accounts behave normally.
2. Select Isolate, fully restart, and confirm the panel reports **Active: isolated**.
3. Edit bindings, account/character macros, stock action buttons, chat layout, and an addon layout. Log out/restart and confirm the local choices return.
4. From another clean installation, verify the official server action bars and server-backed account caches did not change.
5. Repeat edits and force-close WoW; verify original local files and remote official settings still match the baseline. Isolated changes that were never flushed may be lost.
6. Revert the current character. Verify macros/bindings/layouts after the automatic UI reload, and check other characters/shared settings remain isolated.
7. Revert the account. Verify shared macros/bindings and all local character layouts reset; previous isolated copies remain in backups.
8. Exercise the installed SuperWoW, ClassicAPI, Nampower, VanillaHelpers, and zAPI stack. Verify the native layer loads first and no later mod bypasses its settings/file boundary.
9. Switch accounts, realms, reload UI, zone, and return to character select. Default accounts must retain normal behavior, and isolated accounts must remain isolated.

These live checks have not been marked as completed by the offline suites. This release does not claim universal crash-proof coverage for untested client variants, native mods, arbitrary server state, or launching without the DLL.
