# ReadyForLaunch — implementation plan

**Owner:** Adam Chesters / [AdamChesters](https://github.com/AdamChesters)

**Date:** 8 September 2026

**Stage:** proposed MVP; no application implementation yet

## 1. Product

A small Windows utility that starts a simulator and its companion apps in the right order. Pick a profile, check the apps you want, press **GO**, and see whether each app started. Finish with **Stop**; use **Emergency stop** when a launched app hangs.

Keep the everyday workflow in one compact window. App paths and detailed detection settings belong in an expandable editor. The MVP excludes accounts, cloud sync, a plugin marketplace, scripting, simulator configuration editing, telemetry dashboards, and automatic software installation.

## 2. Main window

Use the [JPG concept](mockups/readyforlaunch-ui.jpg) as the visual starting point. Target approximately **980 × 790 logical pixels**, resizable and DPI-aware; allow the app list to scroll while profile selection and session controls remain visible. Collapse groups for smaller windows.

The visual language comes from PSVR2SimShaker's source: near-black background, dark bordered groups, muted teal controls, cyan selection, Segoe UI body text, and a Georgia Bold cyan title. Retain the **by Adam Chesters** attribution, linked to this repository. See the [source references](RESEARCH.md#ui-reference).

Layout, top to bottom:

1. Title, attribution, and a small Settings button.
2. Profile buttons: **DCS World**, **MSFS 2024**, **MSFS 2020**, **X-Plane 12**, **IL-2**. A **Custom profiles** dropdown and a small add button complete the selector.
3. Stacked groups containing compact rows: drag handle, enabled checkbox, app name, source badge, timing selector, status, and an ellipsis menu.
4. A fixed footer: overall status, **Emergency stop**, **Stop**, and a prominent **GO** button.

Rows support drag-and-drop within or between groups, with Move up/down in the menu for keyboard use. Status uses text as well as colour: Idle, Waiting, Launching, Running, Completed, Already running, Failed, and Needs attention. An optional collapsed activity list explains failures without occupying the main screen.

## 3. Adding an app

**+ Add app** opens one small dialog with three tabs. The global button asks which group to use; a group-specific button preselects that group.

| Method | Normal interaction | Implementation approach |
| --- | --- | --- |
| Browse EXE | Pick an executable; name fills automatically | Native Windows file picker; store the path. Expand Details for arguments and working directory. |
| Steam | Search installed Steam games and tools by name | Discover Steam libraries and local app manifests; save the App ID. Ask Steam to launch it, without needing the game's EXE path. Offer manual App ID or pasted Steam store URL as a fallback. |
| Installed apps | Search the current user's Windows app list | Enumerate AppsFolder and Start Menu launch entries; resolve ordinary shortcuts and packaged-app identities. |

Steam launch is feasible through its URL protocol, for example `steam://run/250820` for SteamVR. ReadyForLaunch opens that URI through Windows' shell; it does not require a Steam Web API key. Steam login, updates, licence checks, or a game's own launcher may still require attention. Local manifest discovery is an implementation convenience, not a stable public API; it must fail cleanly and retain the manual App ID fallback. Native Steam applications are in scope; non-Steam shortcuts added to Steam can use Browse EXE initially.

The Installed apps picker should mirror **launchable apps**, not the Settings uninstall list. Some installed products have no launch entry. Display this limitation with an unobtrusive Browse EXE fallback. Packaged apps use their AUMID through the appropriate Windows activation API; desktop entries use their actual shell launch information. Do not attempt to execute every App ID as a file path. [Microsoft's app identity documentation](https://learn.microsoft.com/en-us/windows/configuration/store/find-aumid) explains the Start Menu coverage limitation.

## 4. Order, delays, and parallel groups

Use a **single timing selector per row** rather than three independent timing checkboxes, so contradictory rules cannot be selected. The checkbox at the left enables or disables the app.

| Timing | Meaning |
| --- | --- |
| On GO | The first app starts when its group's gate opens. |
| With previous | Share the previous app's launch gate, so both launch without waiting for either to start. |
| After previous starts | Wait until the preceding app satisfies its configured started condition. |
| X seconds after previous | Start the timer when the preceding launch request is successfully dispatched, or an existing instance is accepted; dispatch this app after X seconds. This alone does not prove readiness. |
| After previous finishes | For one-shot helpers: wait for exit and a validated success result. Keep this option under the same selector, with helper details in the editor. |

Details also permit a short **settle delay after detection**, useful when an app needs a little more time after its process appears. This is distinct from the fixed delay measured from launch dispatch.

Every group starts on GO by default. A small group setting can instead wait for another named app or group; a group becomes ready when every enabled member has reached its required startup/completion condition. These references use stable IDs. Reject cycles and unresolved dependencies before starting. The UI never exposes a graph editor.

**Example stack:**

```text
GO ─┬─ VR & haptics: VR2JB → SteamVR → PSVR2SimShaker
    │                    success      started/ready
    │
    └─ Flight tools: VoiceAttack → wait 5 s → OpenKneeboard + SRS

VR & haptics ready ── Simulator: DCS World
```

The VR and tools groups execute independently. The simulator waits only for the VR group in this example. Three or more independent groups work the same way.

Ordinary adjacent-row rules follow the nearest preceding enabled row. If none exists, use the group's gate and show **On GO**. Explicit dependencies on a disabled app or empty group are configuration errors that require fixing; they must not silently disappear. Show the resulting rule immediately when rows move or are disabled. A failed app blocks its dependants while unrelated groups continue; present **Retry** on the failed row. No automatic restart loops in the MVP.

## 5. What “started” means

The default is **the target process has appeared and remained alive for a short stability interval**. This is not a claim that a flight simulator has loaded a mission or that a VR headset is connected.

The expandable app editor can select Process detected, Window responding, or Successful completion for helpers. For a dependency requiring device readiness, use a verified application-specific readiness check, or pause at a small **Confirm ready** action. Window responsiveness and elapsed time must not be presented as proof of device readiness.

Proposed defaults: a 60-second startup timeout, adjustable per app, and a short stability interval of one second. A timeout displays the failing condition and pauses its dependants. Long simulator loading and Steam updates may require a longer timeout; the UI remains responsive and Stop stays available throughout.

### The PSVR2 example needs special handling

[PSVR2SimShaker's setup guide](https://github.com/AdamChesters/PSVR2SimShaker/blob/3ca53054bc1c32135f270f43a282f02087005ea4/docs/SETUP.md) requires the headset awake, the VR apps closed before VR2JB, VR2JB success, and SteamVR's connected headset indicator before SimShaker starts.

- Configure VR2JB as a one-shot helper. Verify its real success/exit behaviour before shipping a preset; do not assume that opening the console or exiting always means success.
- If SteamVR, DCS, the PlayStation VR2 App, or SimShaker is already running, block this specific unlock preset with a clear explanation. The ordinary reuse-existing default does not override these prerequisites.
- Validate SteamVR readiness detection. Until that check is reliable, this preset needs a manual **Headset connected — continue** gate; a fixed delay remains a user-chosen approximation.
- Run the user's installed helper only. Firmware setup and headset configuration remain outside this launcher.

The JPG illustrates the compact layout; it does not display the expanded readiness options or imply this integration has been tested.

## 6. GO, Stop, and Emergency stop

**GO** validates the current profile, captures an immutable run configuration, checks existing instances, and schedules the groups. Disable repeat GO while a session is active. Editing profiles must not rewrite the active run; keep launch configuration read-only until the session ends.

**Already running:** reuse a positively identified instance by default and mark it **Already running**. It can satisfy a dependency after its condition is checked, but does not become owned by ReadyForLaunch and will be left running by both stop actions. Never infer identity from a common process name alone.

**Stop** first cancels all queued launches and timers, then requests a graceful exit in reverse dependency order: simulator before its required VR tools, consumers before providers. Independent branches can close in parallel. Allow each app time to exit (proposed default 10 seconds). If an app refuses or asks about unsaved work, mark Needs attention and retain the dependencies it still needs. Stop does not silently turn into a force kill.

Prefer a documented app-specific exit command when available; otherwise request window close. Tray apps may hide instead of exiting. PSVR2SimShaker currently does this, so a fully graceful preset requires a supported exit command/IPC added in a separately scoped change, or a verified existing mechanism. Until then, report that it remains running and allow manual exit.

**Emergency stop** cancels pending work immediately and forcibly terminates verified session-owned processes and tracked descendants, rechecking handles/identity to prevent PID reuse mistakes. Keep it visibly separate from GO. It must leave pre-existing apps, unrelated processes, Steam itself, and shared system brokers untouched. Report any access-denied or remaining process; never claim everything stopped when it did not. Force termination can lose an app's unsaved state.

Steam and packaged-app launches can be brokered through an existing process, so their launcher PID is not necessarily the game PID. Adapters must resolve the actual target using launch results, executable/package identity, creation time, and baseline state. A launch-time coincidence is insufficient ownership evidence. If attribution remains ambiguous, label the row **Launch only — stop unavailable** and require explicit target configuration where possible. This is an MVP compatibility limitation, not grounds to kill all similarly named processes.

Closing ReadyForLaunch's window during a session hides it to the tray, where Stop remains available. With no active session, window close exits. Do not reserve PSVR2SimShaker's existing Ctrl+Alt+Space shortcut; a global shortcut can wait for a later version.

## 7. Profiles and small conveniences

The five simulator buttons are starter profile containers, not an assertion that those simulators are installed or a popularity ranking. Each starts unconfigured until the user chooses its launch target. Custom profiles support create, rename, duplicate, and delete. Autosave configuration locally under `%LOCALAPPDATA%\ReadyForLaunch`, with an atomic write and last-good backup.

Keep launchable app definitions separate from profile ordering so the same Steam app or EXE can be reused across profiles. Per-profile arguments or timing are overrides. A schema version allows migration later; runtime PIDs and machine logs never enter shared profile definitions.

Include only three modest conveniences alongside the requested features: duplicate a profile, skip already-running apps, and a collapsed recent activity log. Profile export/import is a later candidate; it should support remapping local paths and never auto-run on import.

## 8. Implementation approach

Recommended stack: **C++20, Win32, Dear ImGui, and Direct3D 11**, using CMake and pinned dependencies. This follows PSVR2SimShaker's existing platform and makes its UI theme straightforward to reproduce. Use native Windows file dialogs and Shell APIs for app discovery/activation. Measure idle CPU/GPU use and suspend rendering when hidden.

Keep five small components: profile storage, app discovery/launch adapters, a dependency scheduler, process/session tracking, and UI. The scheduler owns cancellation and operates independently of the render loop. Every provider reports launch-dispatched, target-identified, started/completed, failed, and shutdown outcomes separately. Record retained process handles and start times for owned targets. Use job objects only where compatible; do not assume they contain processes spawned by an existing Steam client.

Deliver Windows x64 first as a portable ZIP; add a per-user installer after behaviour is stable. Target Windows 11 initially and test Windows 10 separately before advertising support. Decide the project's licence before importing source; any reuse from the GPL-3.0 PSVR2SimShaker project must preserve applicable notices and licence obligations. This planning repo imports no application source code.

## 9. Build sequence and acceptance

| Milestone | Deliverable | Evidence needed before proceeding |
| --- | --- | --- |
| 1. Prove launch and ownership | Minimal adapters for EXE, Steam, and one packaged/Start Menu app; session tracking spike | Launch succeeds without knowing a Steam game's EXE; identify actual targets, already-running cases, and ownership limits. Validate VR2JB success and SteamVR readiness options. |
| 2. Compact editor and profiles | Native window, source picker, rows, groups, reorder, timing, starter/custom profiles | Save and reopen settings; handle missing paths, duplicates, disabled dependencies, and cycles; inspect keyboard navigation and 100/150/200% scaling. |
| 3. Session engine | Parallel groups, gates, delays, cancellation, retry, Stop and Emergency stop | Deterministic tests with controllable helper processes verify ordering, simultaneous release, timeouts, reverse shutdown, cancellation races, and protection of pre-existing apps. |
| 4. Real-stack preview | DCS/VR walkthrough, one non-VR profile, packaged-app coverage, portable build | Test real launches with Steam open and closed, an update/login interruption, a refusing-to-close tray app, an exited/reused PID, and permission failures. Document observed compatibility before release. |

The first version is ready when a user can assemble a profile using all three sources, run at least three independent chains, see exactly which dependency is waiting, and stop the owned apps without touching a pre-existing app. Any brokered app lacking reliable ownership must be visibly identified as launch-only. Runtime validation remains future work; none of these application tests has been run for this planning deliverable.

## 10. V2 — remember and restore window state

**Explicit follow-up requirement from Adam, 8 September 2026:** test and iterate after the first build; add remembered application window positions in v2.

Add a compact **Update state** button for the active profile. Once the user has arranged their apps, it captures each app's window positions and sizes, target monitor, and normal/maximised/minimised state. Subsequent launches restore that saved arrangement after the real application windows appear. A small success message reports how many windows were saved. Repeated use replaces the saved layout for that profile; ordinary app shutdown must not overwrite it automatically.

Capture all eligible top-level app windows, including multiple windows from one app, and use durable app/window identity rather than runtime HWNDs or changing document titles alone. Exclude splash screens, transient dialogs, tooltips, and unrelated windows. A profile option enables or disables restoration without discarding the saved arrangement.

Account for delayed window creation, DPI changes, monitor reordering/disconnection, negative desktop coordinates, and apps that override placement. Restore to the correct monitor when it exists; otherwise fit the window into a connected monitor's working area. Keep bounded retries and report unsupported/elevated/exclusive-fullscreen windows instead of repeatedly moving them. Reposition an already-running app only when it is explicitly included in the saved restoration scope.

Validate after the first-build feedback cycle with a two-monitor layout, mixed DPI, disconnected monitor, a maximised app, a minimised app, and an app with multiple windows. Exact restoration is best-effort for windows the application or Windows will not allow ReadyForLaunch to control. This feature is intentionally outside the first build and is not shown in the v1 JPG.
