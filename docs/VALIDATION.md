The native fixtures and client audit have moved to [zAPI/tests](https://github.com/zabuu/zAPI/tree/main/tests). The merged DLL also has a shared registration/ReloadUI/render-hook chaining fixture.

# Validation status

## Automated checks

Version 0.3.2 / zAPI 3.5: **146 native checks and 26 Lua tests pass**, including the forced-termination fixture. The pinned local client also passes the static audit. Compiler warnings are confined to the vendored MinHook C sources.

The live-slot regression reproduces the previous no-op Apply: the old mocked dispatcher saw a restored login seed while the actual live table remained unchanged. It now verifies the local setter, spell/item/account-macro/character-macro restoration, all 120 slots, true-live Save/Undo capture, disabled uploads, one final refresh and rollback after a failed live write. The live setter, cache refresh flags, full-bar event and separate seed/live table addresses are included in the static ABI audit.

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
- Named setup files preserve all 120 raw action slots, secondary bindings and visibility flags; corrupt payloads and unsafe names are rejected.
- Setup storage is scoped to account/GUID; Default accounts and zero GUIDs cannot create snapshots. Rename collisions preserve both files; Delete archives snapshots; Undo is kept outside the named list.
- Named setup application exercises the production local-setter adapter and player-field read/write code against generated x86 memory. It updates the live action table, local checkpoint and visibility nibble, preserves the official baseline and unrelated player bits, and sends no server edit. A missing live player leaves the setup unchanged.
- A saved setup survives forced termination with no logout or destructor, alongside the existing isolated files and action checkpoints.

Lua tests cover absent/incompatible backends, Default behavior, mode selection, pending-restart status, account/character pagination, current/offline Revert routing, partial-Revert reload, event setup, backend errors, and Lua 5.0 syntax restrictions.

Setup Lua tests cover binding swaps without key collisions, both assigned keys, clearing extra keys, direct-action commands, empty binding sets, pre-switch Undo, rollback after native/binding/save errors, Default/combat/cursor/old-DLL/fault guards, malformed payload rejection, two-click Update/Delete, rename/paging, lazy talent attachment and opening the standalone panel after the talent parent is hidden.

`audit_client.py` verifies the exact executable hash, PE metadata, hook-entry signatures, Lua bridge entry points, multibar field access, reload/save/destroy/load call sites for macros and bindings, and the active binding-set update in `SaveBindings(2)`. Static inspection is separate from runtime validation.

## Remaining live checks

Version 0.4.1 passes 48 Lua tests, including selection-before-Apply, inline save
and rename, duplicate-name recovery, confirmation, empty-state creation,
pagination, edge-aware docking, attached/standalone dragging, long names and
fade cleanup. Offline renders of list, menu, editor, empty and unavailable
states were visually checked. Live-game inspection was not performed.

The 0.4.0 update passes 38 Lua tests, including the installed zPie runtime, and
157 native profile checks. New coverage includes sparse/empty ring slots,
extra rings, macro index changes and missing macros, preserved appearance and
profile tables, Undo, native/binding/refresh rollback, disabled or busy zPie,
legacy setups, corrupt payloads, version-2 length bounds and ring snapshots
surviving forced termination. Gameplay verification remains separate.

Use a disposable test account before relying on this release:

1. Start with the configured VanillaFixes loader; confirm `/zp` reports the native layer and Default accounts behave normally.
2. Select Isolate, fully restart, and confirm the panel reports **Active: isolated**.
3. Edit bindings, account/character macros, stock action buttons, chat layout, and an addon layout. Log out/restart and confirm the local choices return.
4. From another clean installation, verify the official server action bars and server-backed account caches did not change.
5. Repeat edits and force-close WoW; verify original local files and remote official settings still match the baseline. Isolated changes that were never flushed may be lost.
6. Revert the current character. Verify macros/bindings/layouts after the automatic UI reload, and check other characters/shared settings remain isolated.
7. Revert the account. Verify shared macros/bindings and all local character layouts reset; previous isolated copies remain in backups.
8. Exercise the installed SuperWoW, ClassicAPI, Nampower, VanillaHelpers, and zAPI stack. Verify the shared zAPI.dll loads first and no later mod bypasses its settings/file boundary.
9. Switch accounts, realms, reload UI, zone, and return to character select. Default accounts must retain normal behavior, and isolated accounts must remain isolated.
10. Open talents before and after `/zp bars`. Check the Setups sidebar with stock and pfUI talent skins, parent hiding, standalone dragging, left docking near the screen edge, small resolutions and UI scaling.
11. Save two setups with different primary/secondary bindings, empty slots, macros, items, spell ranks, stance pages and stock bar visibility. Change specs through OctoWoW and manually Apply each setup. Verify the correct spells/icons/bindings and third-party bar refresh without a reload. Recheck that bindings persist after a character relog and other characters retain shared account bindings.
12. Exercise Update, Rename, Delete and Undo. Make an unsaved edit before switching, then Undo and verify it returns. Revert the character/account and confirm named setups still exist separately. Force-close after Save, restart, and apply the saved snapshot. Confirm Default, combat and occupied-cursor guards.
13. With zPie enabled, save two setups with different spell/macro/item assignments, empty slots, raid markers and extra rings. Apply each and Undo without reloading. Check the actual ring actions and zPie options preview, preserved ring appearance, and recovery after a restart. Update an old setup to add ring slots; applying an old setup before Update must leave the rings alone.

These live checks have not been marked as completed by the offline suites. This release does not claim universal crash-proof coverage for untested client variants, native mods, arbitrary server state, or launching without the DLL.
