---
name: dsp-check
description: Rebuild the headless DSP tests and run the self-test and unit tests. Use when changing dsp/, or when a pop, click, crack, gate, or level bug needs a measurement before a plugin deploy.
---

# DSP check

After a DSP change, and before claiming a click or level fix, run this from the repo root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dsp_check.ps1
```

Add `-Audit` only when investigating a click or tail. Read the failing lines. Do not paste a passing run.

`build-audit` stays `DOOMGAZER_BUILD_PLUGIN=OFF` with the Visual Studio 18 2026 generator. Do not turn the plugin on in that tree, and do not deploy from it.

Plugin, UI, and parameter edits build through the deploy skill (`build-msvc`), not this one.
