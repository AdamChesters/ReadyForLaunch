# Changelog

## 0.2.0-alpha.1 — 9 September 2026

- Add Help with the approved mockup, a scrollable Status window, and live app status lights.
- Skip detected running apps and show a warning instead of launching another instance.
- Add On GO to every row and preserve group dependencies when rows move or are disabled.
- Add Rename, Copy from, Copy to, and Delete to profile context menus; fix duplicate-name hover conflicts.
- Keep Running apps first, followed by Steam, Installed apps, and Browse EXE.
- Use stable launchers for Discord/Squirrel versioned installations and track their actual app process.
- Add Open target folder to app menus.
- Remove Emergency stop. Stop requests graceful closure of identified session apps, including reused instances.
- Add the PSVR2SimShaker-derived GitHub updater, per-user installer, and version display at the top.
- Add the ReadyForLaunch icon, subtitle, alpha notices, and improved display scaling.
- Migrate first-preview settings to schema 2; preserve profiles during installation and updates.
- Adopt GPL-3.0 with attribution for reused updater code.

## 0.1.0 — 8 September 2026

Initial private Windows preview: app discovery, profiles, groups, dependencies, scheduler, and native UI.
