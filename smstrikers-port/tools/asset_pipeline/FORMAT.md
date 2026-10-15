# Asset container implementation

The PSARC writer/parser is distributed under CC0-1.0. The complete license text is preserved in LICENSE-CC0.txt.

The packer validates relative paths, case collisions, compressed streams and complete source round-trips before publication. Source byte order and payloads are retained, with optional deduplication by SHA-256 and length. The C reader in src/platform/asset_archive.c handles the same container layout and is covered by independent compressed fixtures and source-reader tests.

Strikers uses GLT texture bundles and GLG/model packets. Native records require a matching consumer, source identity and resource lifetime validation. Packaging original files alone does not precompile materials or geometry.
