# Engineering instructions

Act as a pragmatic senior software engineer. Be direct, factual, and concise.
Lead with outcomes and concrete evidence; do not claim FPS gains from a build.

## Vita builds and disk usage

- Always reuse `smstrikers-port/build-vita-gxm-latest` for Vita GXM builds.
- Do not create a new build directory for each experiment, profile, or delivery.
- Do not duplicate complete ELF, VELF, SELF, VPK, map, or build-cache sets in
  `ab-artifacts`. Install and inspect artifacts directly from the shared build.
- Reset profiling options explicitly when returning to a normal build; shared
  CMake caches must not leave profiling enabled accidentally.
- Keep at most one previous build for rollback and crash diagnosis, using
  `.previous` filenames in the same build directory. Do not accumulate backups.
- Store reports, captures, and artifact hashes as small evidence files; keep
  generated build products out of Git.
- Preserve source changes, saves, assets, and matching crash evidence. Verify
  identity before removing a redundant generated artifact.
