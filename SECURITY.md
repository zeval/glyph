# Security Policy

`glyph` is an offline PSP homebrew application that opens local DRM-free EPUB
files. Security-sensitive issues are still in scope because EPUBs are untrusted
input and the app writes local settings/progress files.

## Supported Versions

There is no stable release line yet. Security fixes currently target `main`.
Release branch support will be documented here when stable releases exist.

## Reporting A Vulnerability

Please do not open a public issue for a vulnerability.

Use GitHub private vulnerability reporting if it is enabled for the repository.
If that is not available, contact the repository owner or maintainers privately
with:

- affected commit or release,
- host/PSP/PPSSPP environment,
- steps to reproduce,
- a minimal test EPUB or fixture generator when possible,
- expected and actual behavior,
- any crash logs, sanitizer output, or PPSSPP details.

Do not send copyrighted books. Reduce the EPUB to a minimal fixture or use a
public-domain sample whenever possible.

## Scope

Examples of useful reports:

- crashes, hangs, or memory corruption while opening malformed EPUB files,
- path traversal or unsafe file handling,
- unsafe archive parsing or decompression behavior,
- local data loss in `saves/`, `cache/`, or `logs/`,
- vulnerable dependencies or license/security concerns in bundled assets.

DRM bypasses, network issues, and ebook store integrations are out of scope
because glyph does not implement those features.
