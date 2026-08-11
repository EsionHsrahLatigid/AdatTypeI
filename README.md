# AdatTypeI

AdatTypeI is a JUCE audio effect that round-trips audio through an ADAT Type I inspired frame model, then applies controlled bit flips, burst inversions, and frame holds.

## Identity

- Company: EsionHsrahLatigid
- Manufacturer code: EHL_
- Plug-in code: AdtI
- Bundle ID: jp.ehl.adattypei
- Formats: VST3, Standalone, and AU on macOS

## Build

Use a local JUCE checkout when available:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DADATTYPEI_JUCE_PATH=/path/to/JUCE
cmake --build build/release --target AdatTypeI_Artifacts --parallel
ctest --test-dir build/release --output-on-failure
```

If `ADATTYPEI_JUCE_PATH` is empty, CMake fetches JUCE 8.0.15.

Staged products are written to:

- `artifacts/Release/VST3/`
- `artifacts/Release/AU/` on macOS
- `artifacts/Release/Standalone/`
