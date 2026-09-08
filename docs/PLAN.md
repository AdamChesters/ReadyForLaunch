# ReadyForLaunch implementation plan

**Intended for flight sims, usable for anything** — by Adam Chesters.

Status: **0.2.0-alpha.1**, incorporating the completed first-build feedback. Windows x64 first, native C++20 / Win32 / Dear ImGui / Direct3D 11. Keep the main workflow compact: choose a profile, arrange apps, press GO, inspect Status, and Stop when finished.

## 1. Minimal app selection

Running apps is the default picker and shows visible app windows while filtering services and hidden/tool windows. Alternate tabs are Steam, Installed apps, then Browse EXE. Store stable executable, Steam, or Windows identities rather than runtime PIDs. Recognise Discord/Squirrel launchers to avoid version-specific paths.

## 2. Profiles

Starter buttons: DCS World, MSFS 2024, MSFS 2020, X-Plane 12, and IL-2. Custom profiles appear in a dropdown. All start empty. Every profile supports right-click Rename, Copy from, Copy to, and Delete. Copies retain independent IDs and remap group dependencies; duplicate display names remain valid.

## 3. Groups and row timing

Independent groups run concurrently. A group's first enabled row can select On GO or After group, allowing linear chains and multiple groups following one predecessor. The dependency stays with the group when the first row changes.

Every later row can run On GO, with the previous enabled row, after it starts, after a dispatch-relative delay, or after a successful helper finishes. On GO bypasses that later row's normal group/predecessor gate. A group's dependants wait for every enabled member, including any On GO member.

Validate missing/empty predecessors, cycles, invalid launch targets, and completion gates before starting. Failed branches block their dependants without stopping independent groups. Retry requires the old process to have exited.

## 4. Readiness

Process detection, responding window, successful completion (exit code 0), or manual confirmation. Default timeout: 60 seconds; default settle interval: one second. These are adjustable and do not prove simulator/headset readiness.

No automatic VR2JB preset ships in the alpha. Its prerequisites and exit behavior, SteamVR headset readiness, and SimShaker's graceful exit need real-stack validation. The original JPG illustrates layout rather than a verified preset.

## 5. GO and Stop

GO snapshots and validates the profile, then schedules enabled apps. Identify existing instances before launching; skip duplicates and warn in Status. Reused apps must satisfy their readiness conditions.

Stop cancels pending launches and sends normal close requests to identified session apps, including reused instances, in reverse dependency order. A detected dependent remaining open holds its prerequisites. After ten seconds a non-exiting app needs attention. Unidentified brokered targets require manual closure.

Emergency stop was removed during review. No UI or tray action forcibly terminates apps. Test code retains a restricted fixture-cleanup facility; it is not a product control.

Close the launcher during an active session to hide it in the tray. Stop and Open remain available there. With no active session, closing exits.

## 6. Status and help

A small glass-style light sits immediately left of each row's existing right-side status text: off, green for running, yellow for queued. Read-only detection also updates idle rows.

Status is a scrollable timestamped log of warnings, launches, failures, and stop outcomes. Help includes short explanations and the approved JPG mockup, clearly identified as the original concept.

The subtitle appears under the app title and at the top of the README. The version appears in the top update control; the footer carries a short alpha notice.

## 7. Storage and updates

Autosave profiles locally with atomic replacement and a last-good backup. Schema 2 adds On GO and stable app launchers while reading schema 1 from the initial preview. Runtime process handles and logs are not profile data.

Reuse PSVR2SimShaker's GPL-3.0 GitHub updater with release-version comparison, HTTPS host restrictions, bounded downloads, SHA-256/size verification, progress, and cancellation. Include prereleases during alpha testing. No account credentials are embedded.

Distribute a portable ZIP and per-user Inno Setup installer. The updater opens the installer only after user selection and while no session is active. Application-file replacement must not remove profiles. Preserve source attribution and publish corresponding tagged source.

## 8. Completed review feedback

- [x] Duplicate profile-name hover conflict fixed with stable ImGui IDs and actual hover/selection regression tests.
- [x] Help with mockup and explanations; scrollable Status with explicit existing-instance warnings.
- [x] On GO on every item and preserved group dependencies.
- [x] Running apps, Steam, Installed apps, Browse EXE order.
- [x] Stable Discord/Squirrel targets and actual-process tracking across a simulated version-folder update.
- [x] Rename, Copy from, Copy to, Delete on all profile types.
- [x] Open target folder in app menus.
- [x] Glass-style status lights beside status text.
- [x] Emergency stop removed; requested Stop tooltip added.
- [x] Subtitle in UI/README, version at top, prominent alpha labeling.
- [x] PSVR2SimShaker-derived updater, installer packaging, and original ReadyForLaunch icon.
- [x] Public repository and published v0.2.0-alpha.1 prerelease with installer, portable ZIP, and checksums.
- [x] Silent installation over the first preview, with backup and unchanged-profile hash verification.
- [x] Revised social graphic with capitalised tagline, under-1-MB JPG, and centered README banner.

The social graphic is committed and displayed in the README. This session did not apply it in GitHub's separate Social preview setting because the browser was not signed in; that setting remains unverified. See [asset notes](../assets/README.md). Actual app-stack testing and v2 window restoration remain outstanding.

## 9. Acceptance and first live iteration

Automated checks cover scheduler gates, profile persistence/migration, independent copy IDs, actual UI duplicate-name interactions, Windows fixture lifecycle, app-window filtering, stable versioned launchers, and update validation. Inspect the native main UI at 100%, 150%, and 200% scaling and its key dialogs.

The next user test cycle should exercise the real VR stack, Steam open/closed and update/login interruptions, Windows packaged apps, tray apps, unsaved-work prompts, permission failures, and live mixed-DPI monitor movement. Do not describe these as verified based on fixtures or discovery alone. See [TESTING.md](TESTING.md).

## 10. V2 — remember and restore window state

**Explicit follow-up requirement from Adam, 8 September 2026:** test and iterate after the first build; add remembered application window positions in v2.

Add a compact **Update state** button for the active profile. Once the user has arranged their apps, it captures each app's window positions and sizes, target monitor, and normal/maximised/minimised state. Subsequent launches restore that saved arrangement after the real application windows appear. A small success message reports how many windows were saved. Repeated use replaces the saved layout for that profile; ordinary app shutdown must not overwrite it automatically.

Capture all eligible top-level app windows, including multiple windows from one app, and use durable app/window identity rather than runtime HWNDs or changing document titles alone. Exclude splash screens, transient dialogs, tooltips, and unrelated windows. A profile option enables or disables restoration without discarding the saved arrangement.

Account for delayed window creation, DPI changes, monitor reordering/disconnection, negative desktop coordinates, and apps that override placement. Restore to the correct monitor when it exists; otherwise fit the window into a connected monitor's working area. Keep bounded retries and report unsupported/elevated/exclusive-fullscreen windows instead of repeatedly moving them. Reposition an already-running app only when it is explicitly included in the saved restoration scope.

Validate after the first-build feedback cycle with a two-monitor layout, mixed DPI, disconnected monitor, a maximised app, a minimised app, and an app with multiple windows. Exact restoration is best-effort for windows the application or Windows will not allow ReadyForLaunch to control. This feature is intentionally outside the alpha.
