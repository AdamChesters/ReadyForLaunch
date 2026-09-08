# Third-party notices

ReadyForLaunch is by Adam Chesters and is licensed under **GNU GPL version 3**. See [LICENSE](LICENSE). Source corresponding to each release is available at its tagged commit in [AdamChesters/ReadyForLaunch](https://github.com/AdamChesters/ReadyForLaunch).

## PSVR2SimShaker updater

`src/updates.cpp`, `src/updates.hpp`, and `tests/update_tests.cpp` adapt Adam Chesters' [PSVR2SimShaker](https://github.com/AdamChesters/PSVR2SimShaker) auto-updater under GPL-3.0. The Windows HTTPS release client, semantic-version handling, download validation, and tests originate from that implementation. ReadyForLaunch changes the repository, asset names, version integration, UI, and installer destination, and supports offline UI tests.

Imported from the updater development working copy on 9 September 2026. SHA-256 hashes of the original source snapshot:

- `src/updates.cpp`: `b524a5cf041fc0997c9d2609c60d9f13031c1c391e607b253b1c8c87c07512ee`
- `src/updates.hpp`: `fa2be8f3ba6ad2a277ec4001f304bf2a8f14e8bb6ee40967356ddda22649e3be`

The colour palette and visual direction also follow PSVR2SimShaker at Adam's request. The new ReadyForLaunch icon has its own [generation provenance](assets/README.md).

## Other dependencies

- **Dear ImGui**, copyright Omar Cornut and contributors, MIT. See `licenses/DearImGui.txt`. Uses the Win32 and Direct3D 11 backends.
- **JSON for Modern C++**, copyright Niels Lohmann and contributors, MIT. See `licenses/nlohmann-json.txt`.
- **Inno Setup**, copyright Jordan Russell and Martijn Laan. The installer is built with Inno Setup; its license permits distributing generated installers. See the [Inno Setup license](https://github.com/jrsoftware/issrc/blob/main/license.txt).

Segoe UI and Georgia are loaded from Windows; font files are not redistributed. Steam, simulator, and application names identify user-installed software. No third-party flight sim or companion applications are bundled.
