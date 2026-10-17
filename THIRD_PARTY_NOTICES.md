# Third-party notices

ReadyForLaunch is by Adam Chesters and is licensed under **GNU GPL version 3**. See [LICENSE](LICENSE). Source corresponding to each release is available at its tagged commit in [AdamChesters/ReadyForLaunch](https://github.com/AdamChesters/ReadyForLaunch).

## dcs2psvr2-shaker updater

`src/updates.cpp`, `src/updates.hpp`, and `tests/update_tests.cpp` adapt the auto-updater from Adam Chesters' [dcs2psvr2-shaker](https://github.com/AdamChesters/dcs2psvr2-shaker) (formerly PSVR2SimShaker) under GPL-3.0. The Windows HTTPS release client, semantic-version handling, download validation, and tests originate from that implementation. ReadyForLaunch changes the repository, asset names, version integration, UI, and installer destination, and supports offline UI tests.

Adapted on 9 September 2026. SHA-256 hashes of the original source files:

- `src/updates.cpp`: `b524a5cf041fc0997c9d2609c60d9f13031c1c391e607b253b1c8c87c07512ee`
- `src/updates.hpp`: `fa2be8f3ba6ad2a277ec4001f304bf2a8f14e8bb6ee40967356ddda22649e3be`

The colour palette and visual direction also follow dcs2psvr2-shaker. See [artwork notes](assets/README.md) for the ReadyForLaunch icon.

## Other dependencies

- **Dear ImGui**, copyright Omar Cornut and contributors, MIT. See `licenses/DearImGui.txt`. Uses the Win32 and Direct3D 11 backends.
- **JSON for Modern C++**, copyright Niels Lohmann and contributors, MIT. See `licenses/nlohmann-json.txt`.
- **Inno Setup**, copyright Jordan Russell and Martijn Laan. The installer is built with Inno Setup; its license permits distributing generated installers. See the [Inno Setup license](https://github.com/jrsoftware/issrc/blob/main/license.txt).

Segoe UI and Georgia are loaded from Windows; font files are not redistributed. Steam, simulator, and application names identify user-installed software. No third-party flight sim or companion applications are bundled.
