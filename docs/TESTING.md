# First preview verification

Local verification: **9 September 2026**, Windows x64, Visual Studio 2026 C++ tools, Release configuration with the static C++ runtime. No flight simulator, Steam game, VR2JB, or headset operation was launched as part of these checks.

## Automated checks

`ctest -C Release --output-on-failure` runs two suites:

- **Scheduler and profiles:** linear group chains, first enabled task retaining the group gate, circular/empty dependency rejection, parallel members and shared predecessors, readiness settle time, dispatch-relative delay, successful helper completion, manual confirmation, timeout, failed launch/retry, stop cancelling pending work, reverse shutdown order, protection of reused apps, protection of prerequisites whose reused dependents remain open, exited prerequisites, launch exceptions, profile duplication, and JSON round trips.
- **Windows process lifecycle:** directly launched fixture ownership, graceful closure, refusal to close, force termination of an owned child process tree, refusing to terminate a pre-existing instance, successful helper exit codes, missing targets, settings backup, and the packaged-app activation provider being available. The running-app picker includes a visible fixture once and excludes hidden and tool-window fixtures. Test fixtures clean up only their own process jobs.

Read-only discovery also enumerated real running application windows, Steam manifests across local libraries, and Windows/Start Menu entries. Enumeration is not proof that every discovered app will launch successfully.

## Visual and packaging checks

The executable's native render capture was used to inspect the main window and the picker, app details, settings, and group dialogs. The README screenshot is the real application rendering an illustrative preview stack. It contains no captured personal window titles. `--preview` prevents that stack from launching.

The portable package contains the executable, user guide, test notes, and dependency licence notices. A package smoke check renders the executable from its staged location with an isolated data directory. The ZIP has a SHA-256 checksum.

## First live iteration

- Configure the real VR2JB → SteamVR → PSVR2SimShaker sequence. Verify helper completion and SteamVR headset readiness manually before enabling automatic gates. Use SimShaker's own Exit command when needed.
- Check actual Steam launch options, Steam updates/login prompts, packaged-app activation, and representative EXEs selected from Running apps. Steam and packaged Windows apps remain launch-only in this preview.
- Check apps requiring elevation, single-instance launchers that hand off to another process, tray apps, apps with unsaved-work prompts, and apps whose main window belongs to a child process. These can require manual readiness or app-specific integration.
- Check mixed-DPI monitors, resizing, keyboard navigation, and very long application/profile names with the user's normal desktop layout.

V2 window-position capture/restoration and the **Update state** button are requirements in the plan, not implemented features of 0.1.0.
