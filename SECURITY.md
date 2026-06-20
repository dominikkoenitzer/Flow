# Security Policy

## Supported versions

FLOW ships as a single rolling line of named releases. Only the **latest published release** receives fixes. If you're on an older build, please update before reporting an issue.

| Version | Supported |
|---|---|
| Latest release | ✅ |
| Older releases | ❌ |

## Reporting a vulnerability

**Please do not open a public issue for security vulnerabilities.**

Report privately using one of:

1. **GitHub Security Advisories** (preferred) — open a private report at
   <https://github.com/dominikkoenitzer/Flow/security/advisories/new>.
2. **Email** — dominik.koenitzer@gmail.com with a clear description and reproduction steps.

Please include:

- The FLOW release name (e.g. *Firefly*) and your Windows version.
- A description of the issue and its impact.
- Step-by-step reproduction, and a proof-of-concept if you have one.

You can expect an acknowledgement within a few days. Once a fix is ready, it will go out in the next release and you'll be credited if you'd like.

## Scope & what is *not* a vulnerability

FLOW is, by design, a privileged input-automation tool. The following are inherent to that design and are **not** considered vulnerabilities:

- **It requires Administrator rights.** Global low-level input hooks (`WH_MOUSE_LL` / `WH_KEYBOARD_LL`) are only permitted for elevated processes.
- **Antivirus / SmartScreen warnings.** The binary is unsigned and uses global input hooks, so some engines flag it. This is a false positive inherent to this category of software — verify the published SHA-256 if you want assurance the download is intact.
- **It can synthesize input.** Recording and replaying keystrokes/clicks is the entire purpose of the tool.

Genuine security issues — for example memory-safety bugs reachable from a crafted `.rec` macro file, privilege-escalation paths beyond the intended elevation, or unsafe handling of the settings/macro files — are in scope and very welcome.
