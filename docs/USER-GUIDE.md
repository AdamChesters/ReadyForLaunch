# ReadyForLaunch 0.2.0-alpha.1

**Intended for flight sims, usable for anything** — by Adam Chesters.

Alpha software: verify your own app stack and keep a backup of your profiles. Windows x64; unsigned executable and installer.

## First launch

Run Setup for a per-user installation, or extract the portable ZIP into a folder you can keep and run `ReadyForLaunch.exe`. No service or administrator rights are normally required. Windows may show a reputation prompt for this unsigned alpha.

Choose a simulator profile. The five starter buttons begin empty. Add your apps, arrange them, then press **GO**.

## Add apps

The source tabs are **Running apps → Steam → Installed apps → Browse EXE**. Running apps is selected each time you open the picker.

**Running apps** shows applications with visible top-level windows, with one entry per app. It excludes services, hidden windows, tray-only apps, and tool windows. Open an app's main window first if it is missing. Add a hidden helper intentionally using another source.

The picker saves the executable or Windows identity and recognises Steam apps inside discovered Steam libraries. A window title may become the display name; edit it as desired. Review arguments and working folder if the app needs special options. Running-process command lines are not captured automatically.

- **Steam:** choose an installed game/tool, or enter its App ID or Steam store URL. Steam handles login, updates, and launch prompts. Set game arguments in Steam's own Properties dialog.
- **Installed apps:** choose a Windows packaged app or a supported Start Menu shortcut. Shortcuts with arbitrary shell commands or unsupported arguments are omitted.
- **Browse EXE:** choose an executable; optionally set arguments and a working folder.

Discord and compatible Squirrel apps are stored using their stable `Update.exe --processStart App.exe` launcher, with a stable working folder. ReadyForLaunch tracks the app it starts, including when the launcher exits immediately. Old saved version-folder EXEs are converted when the stable launcher exists. Actual Discord updates still belong to Discord.

Use an app row's **… → Open target folder** to open its containing or resolved install folder in Explorer. An unresolved or missing folder is reported in Status.

## Groups and timing

Add groups with **+ Add group**. Drag the `::` row handle or use the row menu to reorder/move apps. Each checkbox enables that app for the profile.

The **first enabled task** shows the group's start rule:

- **On GO:** start with other independent groups.
- **After group…:** wait for all enabled apps in the selected group to reach their configured readiness/completion conditions.

This dependency belongs to the group and survives moving or disabling its first row. Build a linear chain such as **VR → Flight tools → Simulator**, or start two groups after the same predecessor. Circular references and empty predecessor groups are rejected.

Every later row offers:

- **On GO:** start immediately, regardless of the row's position or its group's normal wait.
- **With previous:** share the preceding enabled row's launch gate.
- **After previous starts:** wait for the preceding app's readiness condition and any group gate.
- **Delay after previous:** wait the chosen interval from the previous launch dispatch, plus any group gate.
- **After previous finishes:** wait for a helper configured with **Successful completion** to finish successfully.

Changing the first row to On GO removes that group's dependency. If a later On GO row becomes first through moving/disabling rows, the existing group rule takes precedence and is shown in the UI.

A failed app blocks its dependants while independent branches continue. Retry is available after the failed app has exited.

## Readiness and status

App details offers **Process detected**, **Window responding**, **Successful completion**, or **Confirm ready manually**, with a settle delay and startup timeout.

Steam entries chosen from Running apps usually include a detection EXE. Other Steam entries default to manual confirmation; select a detection EXE for automatic readiness. Steam updates or long loading times may need a longer timeout.

Lights sit to the left of each row's status: **off** when not running, **green** when running, **yellow** while queued. Idle observation refreshes periodically. Detection can be unavailable for protected processes or brokered apps.

Before launch, ReadyForLaunch checks for an identified running instance. If found, it skips another launch and warns in **Status**. The existing app still has to satisfy the configured readiness condition. Status holds a scrollable, timestamped log of recent alerts and outcomes.

A detected process or responding window does not prove a flight has loaded or a headset is connected. For SteamVR, confirm readiness after checking the headset indicator. For VR2JB, verify its real exit-code behaviour before using Successful completion, which requires code 0. Follow that helper's own prerequisites. No automatic unlock preset or headset setup is included.

## Stop

Hover text: **gracefully commands shutdown of apps in the list. Some may ignore it**

Stop cancels queued launches and asks identified apps participating in the session to close in reverse dependency order. **This includes apps that were already running and reused by GO.** It does not close disabled rows or apps that have not participated in the session.

Apps can refuse, prompt for unsaved work, or hide in the tray. After ten seconds, remaining apps show **Needs attention**. Prerequisites remain open while a detected dependent is still running. If a brokered target cannot be identified, Status tells you to close it manually.

There is no Emergency stop or force-kill action. Use the app's own Exit command or Windows Task Manager when required. In particular, apps such as PSVR2SimShaker can hide instead of exiting on a normal window-close request.

## Profiles and Help

Right-click any profile button or custom entry for **Rename**, **Copy from…**, **Copy to…**, and **Delete**. Copy replaces the destination's app/group configuration while retaining its name and tab identity. Replacement and deletion ask for confirmation. At least one profile must remain.

Duplicate names are allowed and have distinct internal identities. Use **+** beside Custom profiles to create an empty profile or duplicate the current one. Changes autosave to `%LOCALAPPDATA%\ReadyForLaunch\settings.json`, with a last-good `.bak` backup.

**Help** displays brief instructions and the original approved mockup. The concept image includes some controls removed during review; the current app and this guide describe the implemented behavior.

Editing is locked during a session. Closing ReadyForLaunch then hides it to the tray, whose menu can reopen it or Stop the session. With no active session, window close exits.

## Updates

The top-right version indicator checks the public GitHub release feed on startup, including alpha releases. Click it to check again, read the current/latest version, open Releases, or download an available update.

Downloads are checked against the release asset's size and SHA-256 digest before the installer opens. Installation is unavailable during an active session. The per-user installer replaces application files and preserves the profile folder; it requires any running ReadyForLaunch instance to close. Uninstall also leaves your profiles in place.

A portable copy uses the same installer update path, targeting the current app folder. To remain entirely portable, download and extract the ZIP yourself instead. The app does not upload profiles or status logs; its network requests are update checks and downloads. Normal Steam launches use Steam's own protocol.

This alpha reads settings from 0.1.0 and saves schema 2. Older builds cannot read the new options; back up settings before downgrading.

## Next version

V2 will add **Update state** to save and restore window positions, sizes, monitors, and states per profile. It is not included in this alpha.

## Development options

- `--data-dir <folder>` uses an isolated settings directory.
- `--preview` displays an illustrative stack and prevents GO from launching it.
- `--capture <absolute-path.png>` renders the interface to a PNG without showing a window, then exits.
- `--capture-view picker|editor|settings|group|help|status|updates` opens that view for a capture.
- `--capture-scale 1|1.5|2` selects display scaling for a capture.

Preview/capture mode disables network update checks and uses sample data where appropriate.
