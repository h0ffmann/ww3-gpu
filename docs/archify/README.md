# Archify pages

Interactive HTML twins of Mermaid figures, drawn with the `archify` skill (tt-a1i/archify v3.0.1,
MIT). The skill is the submodule `vendor/archify`, pinned and sparse, with
`.claude/skills/archify` a symlink into it; nothing of it is copied here. Each page is one
standalone file with inline SVG, light and dark themes, and image export.

A page is never a figure of the book: the figure is the Mermaid fence with its *How to read this figure* card, which carries the evidence and the `⚠` marks; every node and label traces to a file read, a WW3 fact cites `model/src/<file>.F90:<line>` at the pin, and a speed figure says "regtest pending" until WW3's regtests have run on its commit.

Only the typed source (`<slug>.json`) is committed. The pages are build output, about 830 kB each
because every page inlines Archify's template, and git ignores them:

```bash
just archify init     # once per clone: the pinned commit, shallow, only archify/ on disk
just archify render   # every docs/archify/<slug>.json -> <slug>.html, validated and browser-checked
```

`render` runs Archify's `finalize --quality showcase`, which validates the source, renders the
page and checks it in a real browser. At `2ab3cae` it reproduces, byte for byte, the five pages
first drawn for [`WW3_GPU_VS_NOAA_202610.md`](../WW3_GPU_VS_NOAA_202610.md) (v, `cmp`, 2026-10-17).

| Source | Type | Question | Figure |
|---|---|---|---|
| [ww3-gpu-vs-noaa-architecture.json](ww3-gpu-vs-noaa-architecture.json) | architecture | What is the same and what differs between NOAA's WW3 and ww3-gpu? | [the two builds](../WW3_GPU_VS_NOAA_202610.md#the-two-builds) |
| [ww3-gpu-parity-workflow.json](ww3-gpu-parity-workflow.json) | workflow | How is a ww3-gpu result judged against NOAA's WW3? | [the verdict](../WW3_GPU_VS_NOAA_202610.md#the-verdict) |
| [ww3-gpu-step-sequence.json](ww3-gpu-step-sequence.json) | sequence | Where does a time step leave the Fortran in ww3-gpu, and what does NOAA's WW3 do at the same place? | [one time step](../WW3_GPU_VS_NOAA_202610.md#one-time-step) |
| [ww3-gpu-port-lifecycle.json](ww3-gpu-port-lifecycle.json) | lifecycle | Which states does a WW3 routine pass through on its way to Kokkos? | [a routine's states](../WW3_GPU_VS_NOAA_202610.md#a-routines-states) |
| [ww3-gpu-device-math-dataflow.json](ww3-gpu-device-math-dataflow.json) | dataflow | Why can a GPU result carry the bits of NOAA's CPU run? | [device math](../WW3_GPU_VS_NOAA_202610.md#device-math) |
