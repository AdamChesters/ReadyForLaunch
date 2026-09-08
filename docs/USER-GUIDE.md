# ReadyForLaunch 0.1.0 preview

By Adam Chesters. Windows x64, portable build.

## First launch

Extract the ZIP into a folder you can keep, then run `ReadyForLaunch.exe`. Windows may show a reputation prompt because this preview is not code-signed. No installation, service, or administrator rights are normally required.

Choose a simulator profile. The five preset buttons start empty so nothing launches unexpectedly. Add your apps, arrange them, then press **GO**.

## Add apps

**Running apps** is the default picker. It shows applications with visible top-level windows, with one entry per app. It excludes services, hidden windows, tray-only apps, and tool windows. Open an app's main window first if it is missing. Background helpers can still be added intentionally using another source.

The picker saves the app's executable or Windows identity, and recognises Steam apps when their executable is inside a discovered Steam library. A window title may become the initial display name; edit that name as desired. Review arguments and working folder if an app needs special launch options. Running-process command lines are not captured automatically.

Other tabs:

- **Browse EXE:** choose an executable. Arguments and working directory are optional.
- **Steam:** choose an installed game/tool, or enter an App ID / Steam store URL. Set game launch arguments in Steam's own Properties dialog. Steam handles login, updates, and launch-option prompts.
- **Installed apps:** choose a Windows packaged app or a supported Start Menu entry. Some shortcuts with special arguments or shell commands are omitted; use Running apps or Browse EXE instead.

Steam and packaged Windows launches are **launch-only in this preview**. Their existing launcher/broker cannot reliably establish which processes belong exclusively to this session, so Stop and Emergency stop leave them open. Direct EXE launches are tracked in a Windows job object, including their child processes. An app which cannot be tracked safely is reported as a failed launch.

## Groups and timing

Add groups with **+ Add group**. Drag a row's `::` handle to another row or group, or use the row's menu to move it. Each app's checkbox enables it for the profile.

The **first enabled task** exposes the group's start rule:

- **On GO:** start alongside all other independent groups.
- **After group…:** select a named group and wait for all of its enabled apps to reach their configured startup/completion conditions.

The dependency belongs to the group, so it survives changes to the first task. You can make a linear chain such as **VR → Flight tools → Simulator**, or start two groups after the same predecessor. Circular references and empty predecessor groups are rejected.

Later rows choose:

- **With previous:** launch alongside the previous row, using the same gate.
- **After previous starts:** wait for the preceding app's readiness condition.
- **Delay after previous:** wait the selected time from the preceding launch dispatch. Change the seconds inside the timing dropdown or App details.
- **After previous finishes:** wait for a helper to finish successfully. Set the helper's readiness to **Successful completion** first.

If you disable a middle row, ordinary timing follows the nearest preceding enabled row. A failed app blocks dependent apps and groups; independent groups continue. Retry is available after the failed process has exited.

## Readiness

In App details choose **Process detected**, **Window responding**, **Successful completion**, or **Confirm ready manually**. Add a settle delay and startup timeout if needed.

Steam entries selected from a running app usually have a detection EXE filled in. Other Steam entries default to manual confirmation; optionally choose a detection EXE to automate readiness. Detection allows observation, not permission to terminate brokered processes.

Process/window detection does not prove that a simulator has loaded a flight or that a headset is connected. For SteamVR, use manual confirmation after verifying its headset indicator. For VR2JB, verify the helper's exit-code behaviour before relying on Successful completion (this preview recognises code 0). Follow the helper's prerequisites, including closing conflicting VR apps first. No automatic VR2JB preset, firmware changes, or headset configuration are included.

## Stopping a session

**Stop** cancels pending launches and requests app closure in reverse dependency order. Apps can refuse, prompt for unsaved work, or hide in the tray. After ten seconds, a remaining app is shown as Needs attention. Close it manually or choose Emergency stop.

Existing instances are reused and marked **Already running**. Both stop actions leave those instances open. Launch-only apps also remain open. A directly launched prerequisite is retained while an observed or unresolved dependent app remains open; Emergency stop can explicitly force the directly launched apps to end.

**Emergency stop** cancels pending work and forcibly terminates this session's directly launched process trees. Unsaved state in those apps can be lost. It does not kill Steam, pre-existing apps, or unrelated processes.

PSVR2SimShaker hides its window on close. Use its tray/settings Exit action if you need it to shut down gracefully; ReadyForLaunch does not yet provide a dedicated SimShaker exit integration.

## Profiles, settings, and tray

Use the small **+** beside Custom profiles to create an empty profile or duplicate the current profile. Settings lets you rename profiles and delete custom profiles. Each profile stores its own app configuration. Changes autosave to `%LOCALAPPDATA%\ReadyForLaunch\settings.json`, with a last-good backup.

While a session is active, profile editing is locked. Closing the ReadyForLaunch window hides it to the tray; use its tray icon to reopen it or stop the session. With no active session, closing the window exits.

## Next version

V2 will add **Update state**, capturing window positions, sizes, monitor assignments, and window states per profile for restoration on later launches. It is deliberately absent from this first preview.

## Development options

- `--data-dir <folder>` uses an isolated settings directory.
- `--preview` displays the example stack and prevents GO from launching it.
- `--capture <absolute-path.png>` renders the actual interface to a PNG and exits without showing a window.
- `--capture-view picker|editor|settings|group` opens that actual dialog for visual verification; use with `--capture`.

Capture files remain local. There is no telemetry upload, cloud account, or background network service.
