# zProfiles

Local account profiles for the audited Windows 10/11 x86 OctoWoW / WoW 1.12.1.5875 client.

**0.4.1 is experimental. Native file/network tests and Lua controller tests are provided. Live character login, addon interoperability, and cross-computer server verification are still required before relying on it with an important account.**

## Installation

1. Put the addon folder in `Interface/AddOns/z_Profiles`, or clone this repository there. Enable **zProfiles** in the AddOns menu.
2. Install **[zAPI.dll 3.6 or later](https://github.com/zabuu/zAPI#installation)** separately. Download **`zAPI.dll`** from that repository's **`bin`** folder and copy it into your main WoW folder, beside `WoW.exe` and `VanillaFixes.exe`.
3. Open **`dlls.txt`** in that main WoW folder. Put **`zAPI.dll`** first, exactly once, and preserve your other DLL entries below it. Remove the old **`zProfilesNative.dll`** line if upgrading. The shared zAPI includes profile protection; do not load both DLLs.
4. Fully restart through **VanillaFixes.exe**. `/reload` cannot load a new DLL. Installing an addon through Git or an addon manager does not install this DLL or edit `dlls.txt`.

The addon repository and its ZIP contain no native DLL or native implementation. The complete manual guide, shared source, binary and checksum are maintained in **[zabuu/zAPI](https://github.com/zabuu/zAPI)**. Keep your existing `WTF/zProfiles` data when upgrading.

## Use

Enable the zProfiles addon, log in to a character, and open `/zp` or `/zprofiles`. Select an account and choose **Isolate**, then fully exit and restart the client. Until that restart, the panel continues to show **Active: default**.

Every unconfigured account uses **Default**. Default accounts keep normal file and server synchronization behavior. Choosing **Use Default** also takes effect only after a full restart: an isolated session is never uploaded by opting out in the middle of play.

The panel lists account directories visible on this installation, then their local realm/character directories. It does not query passwords, remotely enumerate accounts, or send profile contents anywhere.

## Talent / bar setups

Use **Setups** in the talent window's footer, the **Bar setups** button in `/zp`, or `/zp bars`. The talent button expands a slim, 168-unit sidebar attached to the visible talent-window edge. It matches the window's height with stock and pfUI skins, follows it when moved, and can open on the left when the right edge has insufficient room. `/zp bars` opens the same sidebar as a movable standalone panel.

Select a setup, then use the single **Apply** button. The selection stripe shows which setup will be applied; the small dot marks the last applied setup. **+** opens the name field for a new save. **...** contains **Update saved setup**, **Rename...** and **Delete...**, with a second click required for Update/Delete. **Undo last switch** stays below Apply. Pages appear only when needed; the mouse wheel also changes pages. Long names and full status messages are available in tooltips. The sidebar uses a short opening fade and no idle animation loop.

1. On an **isolated** character, arrange your bars, bindings and zPie ring slots for one spec.
2. Choose **+**, enter a name such as `Frost`, `Fire`, `Tank`, or `Healing`, then choose **Save setup**. The empty panel also provides a **Save setup** button.
3. Change talents through OctoWoW, arrange the second setup, and save it under another name.
4. After changing specs, select the matching setup and click **Apply**. No UI reload is needed.

**0.3.2 / zAPI 3.5 fixes Apply doing nothing:** earlier DLLs replayed a login staging message without updating live action slots. The fixed DLL restores and verifies the live slots, including spells and macro buttons. Existing saved setups can be applied without saving them again. Fully restart after updating the DLL; `/reload` cannot replace it.

Each character has its own named setups. They contain **all 120 action slots**, including hidden/stance pages, the keys assigned to each command, and the four stock multibar visibility flags. Applying writes character-specific bindings inside the isolated clone, leaving other characters' shared binding cache alone. Talent points, addon frame positions, pfUI bar styling, pet-bar arrangements, and macro text are outside these snapshots. Macro buttons keep their slot references to the current macros; keep those macros available when using a saved setup.

**zPie ring slots (0.4.0 / zAPI 3.6):** when zPie is enabled, Save and Update also capture the assignments in all eight slots of every ring, including extra rings and empty slots. Apply restores spells, items, normal macros, SuperMacros and raid markers immediately. Macro assignments resolve by name; keep the referenced macro definitions available. Ring names, size, position, appearance and zPie's own saved profiles keep their current settings. Rings created after saving keep their layout but have their assignments cleared when applying that snapshot.

Older setups leave zPie alone. Arrange the desired ring slots and choose **... > Update saved setup** to add them to an existing setup. Saving with zPie disabled also leaves rings unspecified. Applying a setup containing ring slots requires zPie to be enabled and initialized; close any open ring or pending ring action first.

Select a name and open **...** to update, rename or delete it. Update and Delete require a second click in the menu. Existing/deleted snapshots are retained in local backups. **Undo last switch** restores the unsaved bars, bindings and zPie ring assignments captured just before the most recent Apply attempt; applying Undo does not replace that backup. Saved ring assignments are flushed with the snapshot and survive a crash without waiting for addon SavedVariables or logout.

**Updating from 0.4.0:** this sidebar update uses the same zAPI 3.6 DLL. Run `/reload` after updating the addon files.

Switching is manual and does not detect or change OctoWoW specs. Controls are unavailable during combat, while holding an item/spell on the cursor, or for Default accounts. Saved snapshots are flushed immediately by the DLL, so saving does not depend on logging out. Later edits are not written back into a named snapshot until you choose Update. The normal isolated action checkpoint still tracks bar edits.

**Updating:** update this addon and install the shared **zAPI.dll 3.6+** as described above. Replace any old `zProfilesNative.dll` loader entry with `zAPI.dll` first in `dlls.txt`; retain `WTF/zProfiles`. Existing isolated profiles, named setups and Undo remain compatible. Character/account **Revert** keeps named setups and Undo separately.

## What is isolated

| Settings | Behavior while isolation is active |
| --- | --- |
| Account and character keybinding caches | Reads and writes use the local clone. |
| Account and character macro caches | Reads and writes use the local clone; account-data uploads are blocked. |
| Character layout/chat/config caches | The entire account directory is cloned, including these files. |
| Addon SavedVariables under the account directory | Use the same clone, including addon-managed layouts and extended macros. |
| The 120 server action-button slots | Incoming official slots are retained as the Revert baseline; local slots are substituted for play. Edits are checkpointed locally and never forwarded. |
| The four stock multibar visibility flags | Server writes are blocked; the local player field and checkpoint retain the local choice. |
| Account-data cache synchronization | Server timestamps are masked and settings uploads/download requests are blocked for isolated accounts. |

Protection is performed by the DLL independently of addon logout events. A crash, Alt-F4, or disabled Lua addon does not cause a compensating "restore" upload: the covered server edits were suppressed, and the original account files were never opened for those edits.

This is a profile boundary, not a rollback of the character. Inventory, equipment, talents, quests, chat messages, social lists, and other gameplay changes retain normal behavior. Client-wide `WTF/Config.wtf`, graphics/audio settings, and native-mod files outside `WTF/Account/<account>` are outside the local file boundary. A mod that sends packets outside the client's send funnel or writes files through undocumented direct OS paths needs a separate compatibility review.

WoW may keep unsaved macro, binding, or addon changes only in memory. Those *isolated* changes can be lost in a crash; preserving the official copy does not make every addon crash-durable. Action-button checkpoints use flushed temporary files and atomic replacement.

## Addon settings and addon profiles

Addons keep control of their own profiles, profile selectors, layouts, and
settings. On an isolated account, ordinary account-wide `SavedVariables` and
`SavedVariablesPerCharacter` files load and save in the matching working clone,
with their account/realm/character paths and contents preserved. Choosing or
editing a profile inside an addon continues to use that addon's normal controls.
Default accounts keep their normal files and behavior.

The clone is seeded from that account's existing local files once, then retains
its own changes across reloads and logins. It does not re-import or overwrite
addon settings on every login. Changes made in the original files while using
Default mode do not automatically merge into an existing isolated clone.

Named zProfiles setups contain action slots, keybindings and optional zPie ring
assignments. Applying one updates the captured ring slots without switching
zPie's own saved profile or restoring its appearance settings. Other addons
continue to use their own profile selectors. Account/character **Revert** restores
the corresponding cloned files, including addon settings in that scope.

## Revert

**Revert beside a character** replaces that character's cloned directory with its original local directory and resets its action bars to the most recent official slots received from the server. Other characters and shared account files retain their isolated settings.

**Revert account** restores the entire original local account directory, including shared macros, shared bindings, and every character's local files. Existing local action-bar checkpoints are reset to their official baselines.

Reverting the current character also restores the official live action slots and reloads the UI so native macro/binding state and addon layouts load from the restored files. Old UI file saves and settings packets are discarded during this reload. Previous isolated files and bar checkpoints are retained in `WTF/zProfiles/backups`.

"Official" files mean the unmodified local account files on this installation. zProfiles cannot retrieve another computer's addon files or macros that were never cached here. Official action bars are refreshed at each character login. For an unvisited character, the next login seeds its bars from the server.

## Files

All private profile data stays outside this repository:

```text
WTF/Account/<ACCOUNT>/...                 original local account files
WTF/zProfiles/modes.txt                   opted-in account names
WTF/zProfiles/profiles/<ACCOUNT>/...       working account clones
WTF/zProfiles/bars/<ACCOUNT>/...           official/local action checkpoints
WTF/zProfiles/identities/<ACCOUNT>/...     local character/GUID mapping
WTF/zProfiles/setups/<ACCOUNT>/<GUID>/...  named bars/bindings and last Undo
WTF/zProfiles/backups/...                 pre-Revert copies
WTF/zProfiles/staging/...                 incomplete/new clones
WTF/zProfiles/discard/...                 old UI saves during a Revert
```

The DLL rejects linked/junction-backed profile trees, simultaneous access to the same isolated account from multiple client processes, corrupt registries, and unsupported packet shapes. Settings failures do not fall back to uploading an isolated profile. Incompatible or incomplete native installation stops client startup before login; details are written to `zProfilesNative.log` beside the game.

Keep the native loader enabled for isolated sessions. The addon alone cannot protect a client launched without the DLL. To stop isolation, choose **Use Default** for each isolated account and fully restart. You may then remove the Lua addon. Keep the shared zAPI DLL and loader entry for other addons that use it. Removing only the Lua addon does not disable native profile protection. Original account files remain intact; retained clones and backups can be archived separately.

## Build and validate

Requires Visual Studio C++ x86 build tools. The addon itself has no Lua library dependency. The Python UI tests use `lupa.lua51` as a compatibility harness; production code uses Lua 5.0 constructs.

```powershell
.\..\zAPI\build.ps1 -Tests -Package
python -m unittest discover -s tests -p 'test_*.py' -v
python ..\zAPI\tests\audit_profiles_client.py 'C:\Path\To\WoW.exe'
.\Package.ps1
```

The audited executable SHA-256 values are `d8fc8b43006118b9e51572581dad00db498acef100b45a75d01d0e3791eb63e7` and `3d019e9ee7f220b5fd90ba33e538130a19774159e4cda0cbfd65563618e388d5`. Both pass the same PE metadata, hook signatures, layout offsets and binding/reload call-site audit. `zAPI.dll` verifies PE metadata and unmodified hook entries in memory. Its entry must be first in `dlls.txt` so it initializes before other DLLs hook those entries.

See [native design](docs/DESIGN.md) and [validation / remaining live checks](docs/VALIDATION.md). Native source, tests and MinHook's BSD license are maintained in the zAPI repository. No Blizzard binary or account data is included.
