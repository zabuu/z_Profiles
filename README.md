# zProfiles

Local account profiles for the audited Windows 10/11 x86 OctoWoW / WoW 1.12.1.5875 client.

**0.2.0 is experimental. Native file/network tests and Lua controller tests are provided. Live character login, addon interoperability, and cross-computer server verification are still required before relying on it with an important account.**

## Installation

1. Fully exit WoW.
2. Put the addon folder in `Interface/AddOns/z_Profiles`.
3. Copy **[zProfilesNative.dll](bin/zProfilesNative.dll)** from the addon's `bin` folder into your **main WoW folder**, beside `WoW.exe` and `VanillaFixes.exe`. The destination is `<WoW folder>/zProfilesNative.dll`.
4. Open `dlls.txt` in that same main WoW folder, or create it if it is missing. Add this as the **first line**, keeping your other DLL entries below it:

   ```text
   zProfilesNative.dll
   ```

5. Start the game through **VanillaFixes.exe** so it loads `zProfilesNative.dll`. Starting `WoW.exe` directly does not load this protection.

## Use

Enable the zProfiles addon, log in to a character, and open `/zp` or `/zprofiles`. Select an account and choose **Isolate**, then fully exit and restart the client. Until that restart, the panel continues to show **Active: default**.

Every unconfigured account uses **Default**. Default accounts keep normal file and server synchronization behavior. Choosing **Use Default** also takes effect only after a full restart: an isolated session is never uploaded by opting out in the middle of play.

The panel lists account directories visible on this installation, then their local realm/character directories. It does not query passwords, remotely enumerate accounts, or send profile contents anywhere.

## Talent / bar setups

Use **Bar setups** above the talent window, the **Bar setups** button in `/zp`, or `/zp bars`. The compact panel follows the talent window; opening it with the slash command gives you a movable standalone panel.

1. On an **isolated** character, arrange your bars and bindings for one spec.
2. Enter a name such as `Frost`, `Fire`, `Tank`, or `Healing`, then choose **Save new**.
3. Change talents through OctoWoW, arrange the second setup, and save it under another name.
4. After changing specs, click **Apply** beside the matching saved setup. No UI reload is needed.

Each character has its own named setups. They contain **all 120 action slots**, including hidden/stance pages, the keys assigned to each command, and the four stock multibar visibility flags. Applying writes character-specific bindings inside the isolated clone, leaving other characters' shared binding cache alone. Talent points, addon frame positions, pfUI bar styling, pet-bar arrangements, and macro text are outside these snapshots. Macro buttons keep their slot references to the current macros; keep those macros available when using a saved setup.

Select a name to **Update selected**, **Rename**, or **Delete selected**. Update and Delete require a second click in the panel. Existing/deleted snapshots are retained in local backups. **Undo switch** restores the unsaved bars and bindings captured just before the most recent Apply attempt; applying Undo does not replace that backup.

Switching is manual and does not detect or change OctoWoW specs. Controls are unavailable during combat, while holding an item/spell on the cursor, or for Default accounts. Saved snapshots are flushed immediately by the DLL, so saving does not depend on logging out. Later edits are not written back into a named snapshot until you choose Update. The normal isolated action checkpoint still tracks bar edits.

**Updating from 0.1.0:** fully exit WoW and replace `<WoW folder>/zProfilesNative.dll` with the updated file from `bin/zProfilesNative.dll`, as well as updating the addon folder. Keep `zProfilesNative.dll` first in `dlls.txt`. The panel explains this if it finds an older DLL. Existing isolated profiles remain usable. Character/account **Revert** restores the official baseline while keeping named setups and Undo available separately.

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

## Revert

**Revert beside a character** replaces that character's cloned directory with its original local directory and resets its action bars to the most recent official slots received from the server. Other characters and shared account files retain their isolated settings.

**Revert account** restores the entire original local account directory, including shared macros, shared bindings, and every character's local files. Existing local action-bar checkpoints are reset to their official baselines.

Reverting the current character also replays official action bars locally and reloads the UI so native macro/binding state and addon layouts load from the restored files. Old UI file saves and settings packets are discarded during this reload. Previous isolated files and bar checkpoints are retained in `WTF/zProfiles/backups`.

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

Keep the native loader enabled for isolated sessions. The addon alone cannot protect a client launched without the DLL. To remove zProfiles, exit WoW, remove its line from `dlls.txt`, and remove its DLL/addon. Original account files remain intact; retained clones and backups can be archived separately.

## Build and validate

Requires Visual Studio C++ x86 build tools. The addon itself has no Lua library dependency. The Python UI tests use `lupa.lua51` as a compatibility harness; production code uses Lua 5.0 constructs.

```powershell
.\native\build.ps1 -Tests
python -m unittest discover -s tests -p 'test_*.py' -v
python .\tests\audit_client.py 'C:\Path\To\WoW.exe'
.\Package.ps1
```

The supported executable SHA-256 is `d8fc8b43006118b9e51572581dad00db498acef100b45a75d01d0e3791eb63e7`. `zProfilesNative.dll` verifies PE metadata and unmodified hook entries in memory. Its entry must be first in `dlls.txt` so it initializes before other DLLs hook those entries.

See [native design](docs/DESIGN.md) and [validation / remaining live checks](docs/VALIDATION.md). MinHook's BSD license is included with its source. No Blizzard binary or account data is included.
