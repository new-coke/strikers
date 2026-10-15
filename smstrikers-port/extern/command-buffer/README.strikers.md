# Render command storage

Bounded command storage for one producer and one consumer. Commands and their variable payloads remain owned by a slot until consumption completes. Storage and payload alignment is 16 bytes. The implementation is distributed under CC0-1.0; the complete license text is preserved in LICENSE.

The renderer uses eight 2 MiB slots. The frontend publishes after sixteen draws or when a slot fills; result-bearing calls wait for the owner. GPU resource lifetimes are handled separately by the rendering library.
