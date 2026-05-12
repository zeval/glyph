# Questions

This file tracks decisions from the initial spec questions. Answered items are recorded here so implementation can proceed. Open items remain because they need implementation spikes, not product preference answers.

## Answered Decisions

1. PSP-2000/3000/E1000 support should follow after PSP Go v1.
   - Keep storage/path/platform abstractions generic.
   - PSP Go remains primary v1 target.

2. Custom firmware is assumed for v1.
   - Development and release can target the user's PSP Go on 6.61 PRO-C.
   - Unmodded/official firmware support is not a project goal.

3. V1 book focus is mostly novels/text.
   - Long-term project ambition remains broad EPUB support.
   - V1 can ship with a narrower compatibility subset.

4. V1 language/script scope is Latin.
   - Complex shaping, CJK, RTL, and large font fallback remain post-MVP.

6. Reader should support both page-based and scroll-based reading.

7. Collapsed PSP Go reading is a core UX goal.
   - `L` click: scroll up within the current page; jump to previous page at the page edge.
   - `R` click: scroll down within the current page; jump to next page at the page edge.
   - No L/R hold behavior for v1.
   - D-pad can also page turn.

8. Cross means accept/select. Circle means back.

9. Search is post-MVP.

11. SDL2 remains the target backend unless proven too slow.

12. Embedded EPUB fonts are ignored by default for MVP.

14. Hardware available: PSP Go running 6.61 PRO-C.
    - Firmware can change if a concrete toolchain/debug requirement appears.
    - No change is needed for early prototyping based on current research.

15. Project license is MIT.
    - Dependency policy: MIT-compatible permissive dependencies are acceptable.
    - Allowed after audit: MIT, BSD-2-Clause, BSD-3-Clause, Zlib, ISC, Apache-2.0.
    - Bundled fonts may use SIL OFL after audit.
    - GPL/AGPL dependencies are out of scope for v1 unless explicitly approved later.

16. Host build/test harness is approved.
    - Make host parser/layout tests part of the first implementation milestone.

17. PPSSPP emulator harness direction is approved.
    - Next step is implementation spike after PPSSPP is installed or built in the agent environment.

18. Development dependencies must work on Linux/Ubuntu Server and macOS.
    - The current agent machine is Linux/Ubuntu Server.
    - macOS should be a supported development platform, not an afterthought.
    - Native PSPDEV/toolchains are first-class on both Linux and macOS.
    - Docker remains optional for CI/reproducible builds, not the canonical local path.

19. Default PSP Go book folder is `ef0:/PSP/GAME/glyph/books/`.
    - File browser can still access `ef0:/` and `ms0:/`.

20. Saves, cache, and logs live inside the app folder.
    - `ef0:/PSP/GAME/glyph/saves/`
    - `ef0:/PSP/GAME/glyph/cache/`
    - `ef0:/PSP/GAME/glyph/logs/`

21. First implementation slice is Toolchain+UI.
    - PSPDEV build skeleton.
    - SDL2 screen/input/font demo.
    - PPSSPP harness stub.
    - Goal: prove platform/build/render/input risk before EPUB depth.

22. Host build should run the same SDL2 UI app.
    - Linux/macOS build uses same app shell with keyboard controls.
    - Host screenshots and fast UI iteration are first-class.
    - Parser/layout tests share code with host and PSP builds.

23. Kandev task delegation is allowed for implementation.
    - Parallel tasks may use their own workspaces.
    - Private repo permits merge-to-main workflow if needed.
    - Subtasks can message back with findings or completed patches.

24. M0 should build a usable shell before EPUB parsing depth.
    - File browser UI.
    - Reader placeholder page.
    - Settings shell.
    - L/R and keyboard input mapping.
    - Builds/runs on PSP and host.

25. UI style target is quiet utility.
    - Dense but readable.
    - Restrained colors.
    - Minimal chrome.
    - Optimized for reading on 480x272.

26. Default reader theme is dark.
    - Dark gray/black background.
    - Off-white text.
    - Other themes can follow after core UI works.

27. M0 bundled font is Atkinson Hyperlegible or Atkinson Hyperlegible Next.
    - SIL OFL allowed as a font-specific license exception after audit.
    - DejaVu remains fallback if package size/rendering fit is poor.

28. Settings/progress/bookmarks format is JSON.
    - Human-readable.
    - Easy to debug/copy from PSP storage.
    - Version fields required for future migration.

29. JSON library is `nlohmann/json`.
    - MIT.
    - Header-only C++.
    - Used only for small settings/progress/bookmark files.
    - Monitor PSP binary size.

30. Host dependencies use system packages.
    - Ubuntu: apt packages.
    - macOS: Homebrew packages.
    - Avoid vcpkg/vendor complexity unless needed later.

31. Build system is CMake underneath with a top-level Makefile wrapper.
    - User commands: `make host`, `make psp`, `make test`, `make run-host`, `make run-ppsspp`.
    - CMake handles host/PSP targets, SDL2/pkg-config, and tests.

32. Host test framework is Catch2.
    - Use for host-only parser/layout/storage tests.
    - Integrate through CMake/CTest.

33. C++ runtime policy: C++17 with no exceptions and no RTTI.
    - RAII allowed and encouraged.
    - Use explicit `Result`/error values.
    - Avoid hidden heap-heavy abstractions in PSP hot paths.

34. Unsupported/malformed EPUB content should degrade gracefully in v1.
    - Open what can be opened.
    - Skip unsupported pieces.
    - Show clear warnings/errors.
    - Never crash or hang on unsupported books.

35. Early CI/merge checks target Linux and macOS host builds/tests.
    - PSP cross-build CI can come later.
    - PSP local build remains required before PSP-facing work is done.

36. First implementation work stays in one workspace initially.
    - Kandev subtasks remain available after architecture stabilizes or work splits cleanly.

37. M0 completion gate: host app works, PSP `EBOOT.PBP` builds, PPSSPP harness stub exists.
    - Real PSP Go smoke test is follow-up, not required to call M0 complete.

38. PPSSPP automation depth in M0: stub and detect.
    - Script detects PPSSPP if installed.
    - Script documents/install hooks.
    - It runs when PPSSPP is available.
    - Do not build PPSSPP from source in M0.

39. Formatting policy: clang-format.
    - Add `.clang-format`.
    - Add `make format`.
    - Add `make check-format`.
    - No heavy linting in M0.

40. Post-M0 slice: EPUB metadata.
    - Open EPUB.
    - Parse container/OPF/TOC.
    - Extract title/author/spine.
    - Cover with host tests.

## Resolved Explanations

### 5. Is GPL Acceptable For Dependencies?

This asks whether `glyph` is allowed to use libraries with copyleft licenses, especially GPL or AGPL.

Why it matters:

- Some useful ebook/PSP libraries are GPL-family licensed.
- GPL dependencies can require distributing the whole combined program under GPL-compatible terms.
- AGPL dependencies are even more restrictive for many projects.
- Permissive dependencies like MIT/BSD/Zlib/Apache usually let the app choose its own license.

Examples from research:

- OSLib: GPL-2.0, useful PSP 2D helper but license-constraining.
- CoolReader/crengine: GPLv2, possible ebook renderer reference/port but license-constraining.
- MuPDF: AGPL/commercial, not a good fit unless accepting AGPL or buying commercial license.
- SDL2/FreeType/miniz/stb-style stack: easier for permissive licensing.

Decision:

- Treat GPL/AGPL dependencies as disallowed for v1 unless explicitly approved.
- Prefer MIT or MIT-compatible permissive dependencies.

Possible later exception:

- Is GPL acceptable if it unlocks a major implementation shortcut?

### 10. What Is A Host Build/Test Harness?

This means compiling the non-PSP parts of `glyph` on the normal development machine, separate from the PSP build.

Host-testable parts:

- EPUB ZIP/container parsing.
- OPF/NCX/nav parsing.
- XHTML-to-document conversion.
- CSS subset parsing.
- pagination/layout.
- bookmark/progress serialization.

Why it matters:

- Faster than building/copying/running on PSP for every parser/layout change.
- Easier to run unit tests.
- Easier for agent automation.
- Lets the PPSSPP harness focus on real app behavior instead of basic parser correctness.

Decision:

- Yes. Make host tests part of the first implementation milestone.

### 13. Preferred License For `glyph`

This asks how you want other people to be allowed to use, modify, and redistribute the `glyph` source.

Common choices:

- MIT: simple permissive license. People can use/modify/redistribute with attribution.
- BSD-2/BSD-3: permissive, similar to MIT.
- Apache-2.0: permissive plus explicit patent terms, more text.
- GPL-3.0: copyleft. Modified/distributed versions must remain GPL.
- Proprietary/closed: source not freely reusable unless you grant terms.

Why it matters:

- It affects which dependencies are compatible.
- It affects whether other PSP projects can reuse the code.
- It affects how releases and forks work.

Decision:

- Use MIT.
- This keeps dependency choices broad and distribution simple.
