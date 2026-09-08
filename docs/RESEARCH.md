# Name check and implementation research

Checked **8 September 2026**. These findings support the proposed plan; they are not results from running the launcher.

## Name decision

**Use ReadyForLaunch.** Searches did not identify a recognisable active flight sim launcher or popular flight sim utility with this name. Search visibility is incomplete, so this is a practical community-name check, not a guarantee of uniqueness.

The GitHub API search `ReadyForLaunch in:name` returned one repository before this project was created:

| Project | Description | Last push | Stars at check | Assessment |
| --- | --- | --- | --- | --- |
| [itomblack/readyforlaunch](https://github.com/itomblack/readyforlaunch) | Blog | 27 October 2021 | 0 | Unrelated; appears inactive. |

A second GitHub search for the hyphenated form found:

| Project | Description | Last push | Stars at check | Assessment |
| --- | --- | --- | --- | --- |
| [SiobhanCoady/ready-for-launch](https://github.com/SiobhanCoady/ready-for-launch) | Node app for viewing upcoming space launches | 9 August 2017 | 0 | Unrelated spaceflight project; appears inactive. |
| [tthrelk93/QuikFixReadyForLaunch](https://github.com/tthrelk93/QuikFixReadyForLaunch) | No description | 16 November 2022 | 0 | Longer name; no apparent flight sim connection from the search metadata. |

None was marked archived in GitHub. Old activity is evidence of inactivity, not proof a project has been formally abandoned. Adam's requested rule permits using the name where an old project exists.

Additional web searches covered `"ReadyForLaunch" simulator`, `"ReadyForLaunch" "flight sim"`, `"Ready For Launch" "launcher" flight simulator`, and site-specific searches of flightsim.to and the DCS forums. Results mainly contained ordinary uses of the phrase or unrelated spaceflight material, not a competing product. The exact `AdamChesters/ReadyForLaunch` endpoint returned 404 before creation, confirming it was available to the authenticated account at that time.

## UI reference

Reference project: [AdamChesters/PSVR2SimShaker](https://github.com/AdamChesters/PSVR2SimShaker), inspected at commit `3ca53054bc1c32135f270f43a282f02087005ea4`.

- [Theme and fonts in src/main.cpp](https://github.com/AdamChesters/PSVR2SimShaker/blob/3ca53054bc1c32135f270f43a282f02087005ea4/src/main.cpp#L56): Win32/Direct3D 11 and Dear ImGui; dark surfaces, teal controls, cyan highlights, Segoe UI body, Georgia Bold title.
- [Header, compact rows, and expandable controls in src/app.cpp](https://github.com/AdamChesters/PSVR2SimShaker/blob/3ca53054bc1c32135f270f43a282f02087005ea4/src/app.cpp#L178): title and attribution, compact action buttons, bordered groups, status lights, expandable detail controls.

Approximate 8-bit theme equivalents used to guide the mockup:

| Role | Colour |
| --- | --- |
| Window background | `#060709` |
| Group fill | `#0B0D0F` |
| Input background | `#141A1F` |
| Border | `#263338` |
| Normal button | `#17262E` |
| Accent | `#59BAD6` |
| Main text | `#EBEDF0` |
| Muted text | `#8C969E` |

The JPG is an AI-generated concept based on these source-derived values, not a captured or pixel-identical PSVR2SimShaker screen. Its exact prompt is saved in [MOCKUP-PROMPT.md](MOCKUP-PROMPT.md).

## Technical feasibility

### EXE launch

Windows' [process creation documentation](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes) describes creating a process and retaining its handle and ID. Use a separate executable path, correct Windows argument quoting, explicit working directory, and minimal handle inheritance. This is the most straightforward launch method to track.

### Steam launch

Valve's [Steamworks API overview — SteamAPI_RestartAppIfNecessary](https://partner.steamgames.com/doc/sdk/api#SteamAPI_RestartAppIfNecessary) explicitly describes launching through `steam://run/<AppID>` and starting the Steam client if needed. This supports the proposed app-ID-based launch method. ReadyForLaunch can request the URI through Windows' shell; it does not need to embed the Steamworks SDK just to open a Steam link.

[SteamVR's Steam store page](https://store.steampowered.com/app/250820/SteamVR/) identifies App ID **250820**, giving the concrete example `steam://run/250820`.

The separate Valve Developer Community Steam browser protocol page returned HTTP 403 during this check, so the primary evidence used here is the accessible Valve Steamworks documentation. No Steam app was launched during research. Discovery across multiple local libraries, target-process identification, launch-option prompts, updates, and graceful shutdown remain validation tasks for the first implementation milestone.

### Windows app list and packaged launch

[Microsoft's AUMID guide](https://learn.microsoft.com/en-us/windows/configuration/store/find-aumid) documents Get-StartApps and shell app enumeration, and notes that apps absent from the Start Menu will not appear in that command's results. This supports a launchable-app picker with a browse fallback, not a promise to list every installed component.

[IApplicationActivationManager::ActivateApplication](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iapplicationactivationmanager-activateapplication) activates a Windows Store app using its app identity and returns the process ID fulfilling the request. An existing instance may fulfil that request; a returned PID alone does not establish session ownership. Desktop shortcut entries need their own shell launch path.

### Readiness and graceful shutdown

The [PSVR2SimShaker setup guide](https://github.com/AdamChesters/PSVR2SimShaker/blob/3ca53054bc1c32135f270f43a282f02087005ea4/docs/SETUP.md) specifies the unlock/start sequence and says closing its window hides it to the tray. Therefore the VR2JB step needs a success/completion gate, SteamVR needs a readiness check, and SimShaker needs a genuine exit mechanism for fully graceful shutdown. These are concrete integration requirements, not generic delay assumptions.

Microsoft's [CloseMainWindow documentation](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.process.closemainwindow?view=net-10.0) explains that a close request does not force an app to exit; it may refuse or request user input. The native implementation should make this same distinction between graceful Stop and forced Emergency stop. Tracking actual session-owned processes is the core release requirement.

## Scope of this delivery

- Name searches completed and UI source inspected.
- JPG mockup generated and converted to a real JPEG; layout and text visually reviewed.
- Plan records the MVP and Adam's v2 window-state requirement.
- Repository contains documentation and the concept image only. Launch methods, timing behaviour, and process shutdown have not yet been implemented or runtime-tested.
