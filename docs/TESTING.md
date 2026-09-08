# Alpha verification

Local verification: **9 September 2026**, Windows x64, Visual Studio 2026 C++ tools, Release configuration with the static runtime. No flight simulator, Steam game, Discord, VR2JB, or headset operation was launched or stopped during these checks.

## Automated checks

`ctest -C Release --output-on-failure` runs four suites:

- **Scheduler and profiles:** 52 assertions covering linear group chains, first-row dependency retention, cycles/empty predecessors, parallel members/shared predecessors, settle time, dispatch-relative delay, completion/manual readiness, timeouts, failures/retry, cancellation, reverse graceful shutdown, reused-app closure, refusal without escalation, later-row On GO, profile copy identity/dependency remapping, JSON round trips, and schema migration.
- **Windows process lifecycle:** isolated visible/hidden/tool-window fixtures, process ownership, graceful close/refusal, child tracking, helper completion, missing targets, settings backup, packaged-app provider availability, and filtering the Running apps list. A Squirrel-style fixture launches its actual app then exits; removing the old version folder and installing a new one verifies stable target handling. Pre-existing fixture instances are detected and reused. Forced fixture cleanup is restricted to test-owned processes.
- **Update verification:** alpha semantic-version precedence, release/asset selection, permitted download hosts, missing checksums/assets, size and SHA-256 verification, corrupted/truncated installers, and cancellation.
- **Profile UI interactions:** a real ImGui/WARP render context hovers duplicate-name profile buttons and custom entries, opens the right-click Rename action, saves a rename, and selects the second duplicate entry independently. Conflicting-ID diagnostics remain enabled.

Read-only discovery enumerated actual running windows, Steam manifests across local libraries, and Windows/Start Menu entries. Discovery is not proof that every app launches successfully.

## Visual verification

The application's native capture path was used to inspect the main interface at **100%, 150%, and 200%** scaling, plus the picker, app editor, settings, groups, Help, Status, and Updates views.

The README screenshot is the actual app rendering an illustrative preview stack. No personal window titles are included. Preview mode prevents GO from launching that stack. Help was checked with the embedded approved JPG; Status was checked with enough sample alerts to scroll.

## Build and packaging

GitHub Actions builds Release, runs all four suites, and creates a portable ZIP plus a per-user Inno Setup installer. Both include the executable, documentation, icon assets, GPL license, and dependency notices. SHA256SUMS accompanies the packages.

A package smoke check passed for both the locally staged executable and the downloaded CI-built executable using isolated data directories, including the embedded Help image. The published alpha also passed `update_tests.exe --live <download-folder>`: public release lookup, HTTPS installer download, and SHA-256 verification. That download check did not execute the installer.

Release evidence: [successful Actions run](https://github.com/AdamChesters/ReadyForLaunch/actions/runs/34255399890), source commit `d1d7957b5b5a8a4fdfbfa8a477530fc39a3e3295`, and [v0.2.0-alpha.1 prerelease](https://github.com/AdamChesters/ReadyForLaunch/releases/tag/v0.2.0-alpha.1). Both downloaded package checksums were verified. Later social artwork and README edits are on main; they do not change the published binary.

At the user's request, the verified installer was subsequently run silently over the existing portable 0.1.0 folder. It exited successfully, the installed executable reports 0.2.0-alpha.1, and the settings file's SHA-256 was unchanged across installation. The previous app and profile folder were backed up, and the Start Menu shortcut was verified. ReadyForLaunch was not running during this installation.

Interactive installation, the in-app download-to-install handoff, uninstall, Windows reputation prompts, and the real app stack remain unverified. The successful silent installation must not be described as verification of those separate flows.

## First live iteration

- Configure the real VR2JB → SteamVR → PSVR2SimShaker sequence. Verify helper completion and headset readiness manually before relying on automatic gates.
- Exercise Steam launch options, Steam open/closed, updates/login prompts, Windows packaged-app activation, and representative EXEs from Running apps.
- Verify Discord after a real update; the automated test covers the version-folder pattern, not Discord's full updater.
- Check elevated apps, single-instance handoffs, tray apps, unsaved-work prompts, and child-process windows. These can need manual readiness or an app-specific exit action.
- Confirm that already-running session apps warn and receive normal close requests on Stop.
- Test mixed-DPI monitor movement, resizing, keyboard navigation, and long profile/app names on the user's desktop.
- Test the first interactive installer update and confirm the existing profiles remain available.

V2 window-position capture/restoration and **Update state** remain planned.
