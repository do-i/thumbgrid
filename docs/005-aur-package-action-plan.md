# 005 — AUR Package Action Plan

Goal: add an AUR listing for thumbgrid as a third install option, alongside
the existing GitHub Release asset and the self-hosted pacman repo
(`docs/arch-pacman-repo-plan.md`, which explicitly deferred this — "no AUR
account, revisit later"). Not a replacement for either existing path.

Status update (2026-07-19): the registration blocker below is resolved —
`thumbgrid-bin` is published on AUR and maintained via
`scripts/publish-aur.sh` (its header notes pushes "confirmed in practice"), so
an AUR account evidently exists. A1–A4 are marked done accordingly.

Status update (2026-07-20): A5 is done — README corrected and the AUR
publishing workflow documented in `CONTRIBUTING.md#releasing`. All items in
this plan are now complete; it can be deleted once the workflow-hardening
follow-ups in `docs/006-aur-release-workflow-fixes-action-plan.md` land.

Original blocker (checked 2026-07-19, since resolved): AUR disabled new
account registration on 2026-06-15 during cleanup of a large
malicious-package campaign ("Atomic Arch", 1,500+ compromised packages).

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work · Fable 5 (`claude-fable-5`) cross-cutting
multi-file features.

---

## A1. Watch for AUR registration reopening

Periodically check `https://aur.archlinux.org/register` (currently returns
Access Denied) or Arch news for an announcement. No other item in this plan
that needs an account can start before this.

*Model:* n/a — manual/periodic check, not implementation work.

- [x] A1 done — account exists; publishes have gone through in practice

## A2. Decide which AUR package variant(s) to publish

Pick one to start (can add others later):

- `thumbgrid` — builds from a released source tarball/tag. Standard,
  expected default.
- `thumbgrid-bin` — installs the prebuilt binary (matches what the gh-pages
  pacman repo already ships; fastest install, least AUR-side build risk).
- `thumbgrid-git` — always builds latest `develop`/`main`.

Recommendation: start with `thumbgrid-bin`, since the binary artifact and
its build pipeline already exist and are exercised by the gh-pages repo;
add source-build `thumbgrid` later if there's demand from users who don't
trust prebuilt binaries.

*Model:* **Opus 4.8** — judgment call weighing maintenance burden vs. user
trust/expectations, not mechanical.

- [x] A2 done — went with the recommendation: `thumbgrid-bin` first

## A3. Write an AUR-shaped PKGBUILD

The existing `packaging/arch/PKGBUILD` builds in-place from a checked-out
repo (`_srcroot` trick, CI-only) — this doesn't work on AUR's build
infrastructure, which has no access to your local checkout. Needs a
variant with:

- `source=()` pointing at a downloadable artifact for the chosen variant
  from A2 (GitHub release tarball for `thumbgrid`, release binary package
  for `thumbgrid-bin`, or a `git+https://...` source for `thumbgrid-git`).
- Real `sha256sums=()` (or `b2sums`) instead of the empty `source=()` the
  current file uses.
- Keep dependency list in sync with `packaging/arch/PKGBUILD`'s `depends=`
  — don't let the two drift.
- Generate `.SRCINFO` from the new PKGBUILD (`makepkg --printsrcinfo`) —
  required by AUR alongside the PKGBUILD itself.

*Model:* **Sonnet 5** — mechanical adaptation of an existing, working
PKGBUILD to a different source-fetch model; no new design decisions beyond
A2's outcome.

- [x] A3 done — the version-neutral `packaging/arch-bin/PKGBUILD.template`
  is tracked here; `scripts/publish-aur.sh` renders the concrete `PKGBUILD`
  and `.SRCINFO` directly in the temporary AUR clone on each publish

## A4. Submit to AUR

Once an account exists (A1) and A3 is ready:

- Push PKGBUILD + `.SRCINFO` to the AUR git repo for the chosen package
  name.
- Verify a clean `pacman -S` (via an AUR helper) installs correctly on a
  throwaway Arch environment.

*Model:* **Sonnet 5** — following AUR's documented submission steps, low
judgment once A3 is validated.

- [x] A4 done — published; per-release promotion is the manual
  `./run.sh` → "AUR publish" flow (`scripts/publish-aur.sh`)

## A5. Add AUR to README install options + ongoing maintenance

- Document the AUR package in `README.md` alongside the existing GitHub
  Release and pacman-repo instructions.
- Maintenance loop per release: bump `pkgver`/reset `pkgrel=1` (or bump
  `pkgrel` only for ABI-only rebuilds, mirroring the policy already in
  `docs/arch-pacman-repo-plan.md`), regenerate `.SRCINFO`, push. Watch for
  AUR comment/flag notifications (out-of-date flags, build reports).

*Model:* **Haiku 4.5** for the README edit (mechanical) · manual process
(no AI model) for the ongoing per-release maintenance loop itself.

- [x] A5 done (2026-07-20) — corrected the Installation intro (it claimed the
  fork "does not yet ship to ... AUR") and added a `thumbgrid-bin` install
  snippet to README's "Arch Linux package" section, noting it can lag the
  pacman repo. The maintenance loop is covered by `scripts/publish-aur.sh`,
  now documented under "Publishing to AUR" in `CONTRIBUTING.md#releasing`
  along with the tag-push fan-out, ABI-only rebuilds, and packaging checks

---

Suggested order: A1 gates everything. A2 and A3 (drafting) can happen while
still waiting on A1. A4 → A5 once unblocked.
