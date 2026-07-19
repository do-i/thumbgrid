# 006 — AUR Release Workflow Fixes Action Plan

Source: review (2026-07-19) of the AUR release pipeline — tag push →
`.github/workflows/arch-package.yml` (build → GitHub release asset +
gh-pages pacman repo) → manual `scripts/publish-aur.sh` (promote a trusted
release to the `thumbgrid-bin` AUR package). Related: `docs/005-aur-package-action-plan.md`.

Model tiers: Haiku 4.5 (`claude-haiku-4-5-20251001`) mechanical edits ·
Sonnet 5 (`claude-sonnet-5`) standard coding · Opus 4.8 (`claude-opus-4-8`)
design/judgment-heavy work · Fable 5 (`claude-fable-5`) cross-cutting
multi-file features.

Suggested order: B1 (one fix covering both pkgrel gaps) → B2 → B3+B4 as a
pair → robustness items (B5–B8) in any order → polish (B9–B13) as time
allows.

---

## Correctness

### B1. Derive pkgrel from the AUR clone, not the local checkout

`scripts/publish-aur.sh:117-124` computes the pkgrel bump from the
working-tree PKGBUILD. Two failure modes:

- **Dry-run poisoning:** `--dry-run` edits the PKGBUILD and exits without
  committing; the next real run reads the modified file, concludes
  "republishing the same pkgver", and publishes pkgrel=2 for a version
  never published at 1.
- **Local ≠ AUR:** publishing from another machine (or after losing the
  local publish commit) can collide with or regress the AUR-side pkgrel.

Fix: clone the AUR repo *first* and compute `NEW_PKGREL` against its
PKGBUILD (the authoritative state). Bonus: `--dry-run` can then show a
diff against real AUR state instead of stopping before the clone.

*Model:* **Sonnet 5** — reordering an existing script's phases and moving
the comparison source; clear spec, moderate care around the retry/trap
structure.

- [ ] B1 open

### B2. ABI-only rebuild must build the tag, not the dispatched branch HEAD

`.github/workflows/arch-package.yml:49-63`: on `workflow_dispatch` the
version comes from `git describe --tags --abbrev=0`, but the build uses
the dispatched branch's HEAD — there is no `git checkout "$ref"`. If the
branch has moved past the tag, the "rebuild" silently ships newer code
labeled with the old version.

Fix: check out `$ref` before building on dispatch runs.

*Model:* **Haiku 4.5** — one-step workflow edit with an unambiguous spec.

- [x] B2 done — dispatch path checks out the tag (+ `git lfs pull`), then
  re-chowns the workspace for the builder user

### B3. Upload ABI rebuilds to the GitHub release

`arch-package.yml:77` gates the release-upload step on
`startsWith(github.ref, 'refs/tags/')`, so a dispatch-built pkgrel=2
package only reaches gh-pages — but `thumbgrid-bin`'s `_srcrel`
parameterization and publish-aur's `-[0-9]+-` asset regex exist precisely
to consume such assets. The pipeline cannot currently produce what its
consumer is designed for.

Fix: on dispatch runs, upload to the existing tag's release via the
action's `tag_name:` input.

*Model:* **Sonnet 5** — small workflow change but needs the
`softprops/action-gh-release` semantics (tag_name on non-tag refs) done
right; pairs with B4.

- [x] B3 done — upload step also runs on dispatch, targeting the tag's
  release via `tag_name: ${{ steps.ver.outputs.tag }}`

### B4. Pick the highest srcrel asset, not the oldest

`scripts/publish-aur.sh:95-97`: GitHub lists release assets in upload
order and the script takes `head -1`, so once a release carries srcrel 1
and 2 (after B3), the script would promote the stale one.

Fix: extract the srcrel from each matching asset name, sort numerically,
take the max.

*Model:* **Haiku 4.5** — mechanical jq/sort change with an obvious test
(two fake asset names).

- [ ] B4 open

## Robustness

### B5. Pathspec the local publish commit

`publish-aur.sh:144-146`: `git add <paths> && git commit` commits the
entire index, so anything the user had staged is swept into the "Publish
thumbgrid-bin" commit.

Fix: `git commit -m ... -- packaging/arch-bin/` (or bail if the index is
dirty).

*Model:* **Haiku 4.5** — one-line change, well-understood git behavior.

- [ ] B5 open

### B6. Make the AUR clone retryable after a partial failure

`publish-aur.sh:155`: if `git clone` dies mid-transfer leaving a
non-empty directory, every retry fails with "destination path already
exists" — the retry loop can never succeed.

Fix: remove the target directory inside the retried unit (wrap clone in a
small function that `rm -rf`s first).

*Model:* **Haiku 4.5** — mechanical; the retry() helper already exists.

- [ ] B6 open

### B7. Do not retry permanent HTTP errors

The most common failure — running publish before CI finishes, or a local
tag with no release yet — is a 404, which `curl -sSfL` reports the same
as a transient failure, so it is retried 10× (~50 s) before the helpful
"has the workflow finished?" message.

Fix: capture the HTTP status (`curl -w '%{http_code}'` or
`--fail-with-body`) and only retry on 5xx / network errors; fail fast
with the existing hint on 4xx.

*Model:* **Sonnet 5** — needs care to keep retry() generic while adding
status-aware behavior for the curl call sites only.

- [ ] B7 open

### B8. Normalize versions the same way CI does

CI maps `-` → `_` for pacman versions (`arch-package.yml:60`); the
publish script does not, so a tag like `v2026.7.12-rc1` could never match
its release asset. The version is also interpolated unescaped into a jq
regex (dots match any char; a `+` would break it).

Fix: apply the same `s/^v//; s/-/_/g` normalization in publish-aur.sh and
escape the version when building the asset regex. Low priority while tags
stay plain-numeric.

*Model:* **Sonnet 5** — small, but regex-escaping in jq via shell has
enough foot-guns to be above mechanical.

- [ ] B8 open

## Polish

### B9. Include `_srcrel` in the local source filename

`packaging/arch-bin/PKGBUILD:35` renames the download to
`thumbgrid-$pkgver-x86_64.pkg.tar.zst`, so two srcrels of the same pkgver
collide in makepkg's download cache and produce a checksum mismatch until
the cached file is deleted.

Fix: include `$_srcrel` in the local (left-of-`::`) filename.

*Model:* **Haiku 4.5** — one-line PKGBUILD edit + `.SRCINFO` regen.

- [x] B9 done — local filename now matches the remote asset name exactly;
  `.SRCINFO` regenerated

### B10. CI guard against depends drift and stale .SRCINFO

No check keeps `packaging/arch/PKGBUILD` and
`packaging/arch-bin/PKGBUILD` `depends=` arrays in sync (a risk
`docs/005-aur-package-action-plan.md` A3 already flags), nor that
`.SRCINFO` matches its PKGBUILD — manual edits (like the maintainer-email
fix, cee8783d) are exactly how drift sneaks in.

Fix: small CI job (or step in an existing workflow) that diffs the two
depends arrays and asserts `makepkg --printsrcinfo` output equals the
committed `.SRCINFO`.

*Model:* **Sonnet 5** — new CI step with a couple of design micro-choices
(where it runs, how to parse depends robustly).

- [ ] B10 open

### B11. Consider GPG-signing packages and the pacman repo db

The gh-pages repo currently requires `SigLevel Optional` on users'
machines. AUR-side integrity is fine (sha256 pins the asset), but signing
the repo db/packages is the standard next hardening step. Needs a key
management decision (where the private key lives, CI secret handling) —
may be deliberately skipped; decide, then implement or record the
decision.

*Model:* **Opus 4.8** — the decision (key custody, secret rotation,
trust model) is judgment-heavy; the implementation afterward is Sonnet 5
territory.

**Decision (2026-07-19): deferred.** Rationale: signing in CI requires
storing the private key as a GitHub Actions secret, and for a
single-maintainer personal repo that key is exfiltratable by anything
that can run in CI — the added trust is marginal over what already
exists (assets served over HTTPS from GitHub; the AUR path pins an exact
sha256 in the PKGBUILD; README already discloses `SigLevel Optional
TrustAll` as personal/low-stakes). Revisit if the user base grows or a
second maintainer joins; the implementation sketch then is: offline
master key + CI signing subkey as a secret, `gpg --detach-sign` each
package, `repo-add --sign`, publish the pubkey in the README, drop
`SigLevel` to `Optional TrustedOnly`. Provisioning the key/secret is a
human step regardless.

- [x] B11 decided — defer signing; decision + revisit criteria recorded
  above

### B12. Guard against promoting prereleases/drafts

publish-aur defaults to the latest local git tag; that tag's release may
be a prerelease (or not vetted). Minor since the script is explicitly the
manual "I trust it" step, but a `.prerelease == true` check with a
confirmation prompt would prevent slips.

*Model:* **Haiku 4.5** — one jq field check + prompt, mirroring existing
script style.

- [ ] B12 open

### B13. Un-stale doc 005 vs. reality

`docs/005-aur-package-action-plan.md` says everything is blocked on AUR
account registration reopening, while `publish-aur.sh`'s header describes
AUR pushes "confirmed in practice" (up to ~8 retry attempts). One of the
two is stale — update doc 005 to reflect the actual account/publishing
status, and mark completed items (A2–A4 appear done given thumbgrid-bin
exists).

*Model:* **Haiku 4.5** — doc-only edit once the human confirms the actual
AUR account status.

- [x] B13 done — doc 005 updated: A1–A4 marked done (AUR account/publishes
  confirmed by publish-aur.sh in practice), A5 kept open for the README
  edit, stale registration blocker demoted to historical note
