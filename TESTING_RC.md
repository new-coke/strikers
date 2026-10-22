# Strikers Vita: local release candidate for testers

Distribute `strikers_vita.vpk`, `strikers.ini`, this document and
`release-candidate.json` privately from `smstrikers-port/build-vita-gxm-latest`.
**Keep the VPK local. Do not upload it to GitHub, attach it to a release, or
include it in a commit.** The ISO and PSARC are not part of the delivery.
Testers must prepare their own archive following the repository's
[asset setup guide](https://github.com/robin994/strikersVita/blob/main/ASSET_SETUP.md).

Install the complete VPK with VitaShell and copy the supplied INI to
`ux0:data/strikersVita/strikers.ini`, backing up any personal settings first.
The new pipeline requires `ux0:data/strikersVita/sms.psarc`.
Before updating, also back up `cardA/` and `cardB/` from that directory.

This tester profile uses the normal game flow, PlayStation button glyphs,
444 MHz CPU / 222 MHz GPU, synchronous frame completion and no render skipping.
Diagnostics, FPS overlay, profiler, captures, watchdog and probes are disabled.
To restore GameCube glyphs, set `vita_playstation_glyphs = 0` and restart.

Test startup and save loading, menus and controls, one full match, stadium
entrance, goals/replays, trophy presentations and movies. Include movie
skipping and returning to the menu. For issues, report the scene and exact
reproduction steps, video/audio quality and synchronization, Vita model and
firmware. Include the manifest, and preserve the core dump if the game crashes.
Do not enable profiling during normal-performance tests.

This is a functional test candidate. **Stable 30 FPS and 60 FPS are not yet
certified.** Previous tests on the C9 baseline showed a stadium entrance
below 30 FPS and a movie that reached its correct duration but skipped some
source frames. Those results do **not** measure the new release candidate.

To rebuild from a clean checkout with VitaSDK configured:

```sh
smstrikers-port/build-vita-vitagl.sh --release-candidate
```

The script reuses the shared Vita build directory, resets experimental
CMake options and writes a manifest containing the source commit, build
settings and SHA-256 hashes.
