# Formatted copy and stream-header host regression

Run after changing the formatted text copies or `AudioStream::_HdrReadCB`:

```sh
python3 smstrikers-port/tools/test_text_stream_bounds.py --output .tmp/text-stream-host
```

The output directory must be new. The script requires Clang with ASan/UBSan and a 64-bit little-endian host. It discovers the macOS SDK with `xcrun` on macOS and supports an explicit `--compiler` path. It does not invoke CMake, build a Vita executable or contact a device.

The runner extracts the current 23 copy statements from nine frontend/source files, their actual fixed-array declarations, `sDSPADPCM`, the stream-header conversion block, `nlVector3`, and the real `TLTextInstance::SetString` body. It compiles the current BasicString, Vector, Format, nlStrNCpy and allocation-header providers. The host allocator requests the real byte count with malloc and tracks frees; OS and printing includes are minimal stubs. Scene classes are represented only by their real destination members with surrounding sentinels, so the fixture does not claim to reproduce a scene-class ABI.

The text group contains 200 cases covering default empty, allocated empty, short Unicode text, exact fit, full capacity, long input, reused buffers and actual Format output at all 23 sites. Multidimensional destinations exercise first and last rows. It verifies neighboring bytes, termination, source preservation, ownership and continuing destination access after source destruction.

The header group checks six full 96-byte inputs, fixed layout offsets, an independent byte-position endian oracle and outside canaries. Omitting sample-rate conversion must fail its oracle. A deliberately unbounded copy of a short real BasicString must fail with an ASan heap-buffer-overflow; this is a mutation check of the oracle. The historical captured prepatch source witness is retained separately from the reusable tests.

The default empty BasicString points to a two-byte static object, but the host's ASan did not diagnose its oversized read in the historical witness. No negative proof is claimed from that global case. Empty input remains covered positively by all corrected copy sites.

This fixture does not run localization files, complete scene methods, renderer output, DSP playback or Vita code. Long input is truncated according to the existing nlStrNCpy contract; atomic preservation of formatting escapes or surrogate pairs at the truncation boundary is outside this change. The header tests verify byte equivalence and bounds; a passing old conversion does not make cross-subobject array accesses valid C++.
