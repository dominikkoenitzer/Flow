<!-- Thanks for contributing to FLOW! Please fill this in. -->

## Summary

<!-- What does this PR change, and why? -->

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] UI / rendering change
- [ ] Refactor / cleanup
- [ ] Docs / build / CI

## How I tested it

<!-- There is no automated test suite — manual verification is required. -->

- [ ] Built cleanly with `.\scripts\build.ps1` (no new warnings; CI uses `-Wall -Wextra -Werror`).
- [ ] Ran the exe **as Administrator** and confirmed the affected feature works.
- Steps I ran:

## Checklist

- [ ] The build stays **static** — no new non-system DLL imports (CI verifies this).
- [ ] Any new pixel/layout literal is wrapped in `Sc()` / `Scf()` (DPI-safe).
- [ ] If I moved a UI control, I updated **both** `CreateControls` and `PaintUI` (they share the layout constants).
- [ ] I did **not** change the `.rec` macro format / `InputEvent` (or I have a migration plan).
- [ ] I restored `flow::protection::EnforceProtection()` if I disabled it for debugging.
- [ ] Updated `README.md` / `CLAUDE.md` if behavior changed.
