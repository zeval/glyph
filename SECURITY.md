# Security Policy

`glyph` is a homebrew reader for local DRM-free EPUB files. EPUB parsing is an
important security boundary because book files are untrusted input.

## Supported Versions

The project has not shipped a stable release yet. Security fixes target the
default branch until releases begin.

## Reporting A Vulnerability

Please report suspected vulnerabilities privately through the repository owner's
GitHub security contact or by opening a minimal private advisory if GitHub
Security Advisories are enabled.

Include:

- Affected commit or release.
- A small reproducer EPUB when possible.
- Expected behavior and observed behavior.
- Crash logs, PPSSPP logs, or PSP notes if available.

Do not publish exploit details until maintainers have had time to investigate.

## Areas Of Interest

- Crashes, hangs, or memory corruption from malformed EPUB/ZIP/XML/XHTML.
- Path traversal or unsafe writes from EPUB contents.
- Unbounded memory allocation from large chapters, images, or metadata.
- Host tooling that executes unexpected commands or reads outside intended paths.
