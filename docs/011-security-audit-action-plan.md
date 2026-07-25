# 011 — Security Audit Action Plan

Security review of thumbgrid at commit `35fc0ff0` (2026-07-24). This audit
covered application code, local file operations, platform integrations,
scripts, GitHub Actions, and release packaging.

T-shirt sizes rank **security severity**, not implementation effort:

- **XL** — critical: credible code execution, root-level compromise, or
  destructive data loss
- **L** — high: significant supply-chain or release-integrity exposure
- **M** — medium: bounded integrity, availability, or trust-boundary failure
- **S** — low: defense-in-depth weakness with limited direct impact

The work below is intentionally not implemented by this document.

## Implementation status (2026-07-24)

| Item | Status |
|---|---|
| XL1 symlink deletion | **Done** — fixed and regression-tested |
| XL2 macOS AppleScript injection | **Open** — code fix is small, but unbuildable/untestable on this Linux host |
| XL3 pacman repository signing | **Blocked on maintainer** — needs a signing key, secrets, and published fingerprint |
| L1 Windows dependency pinning | **Open** — large, and unverifiable without a Windows runner |
| L2 action SHA pinning / permissions | **Blocked on maintainer** — changes release workflow permissions |
| M1 copy/move containment | **Done** |
| M2 rename path traversal | **Done** |
| M3 artifact signing/notarization | **Blocked on maintainer** — needs Developer ID and Authenticode certificates |
| S1 video thumbnail temp paths | **Done** |

Items marked *blocked on maintainer* need credentials or repository settings that
only the project owner holds; they are deliberately not attempted here.

## Immediate — XL

### XL1. Permanent deletion follows directory symlinks into their targets

**Evidence**

`FileOperationsController::removePaths()` in
`src/components/fileoperationscontroller.cpp` tests `QFileInfo::isDir()`
without first testing `isSymLink()`. A directory symlink is therefore sent to
`DirectoryModel::removeDir()`, which reaches
`FileOperations::removeDir(..., recursive=true)` and calls
`QDir::removeRecursively()` on the symlink path.

A focused Qt 6 probe reproduced the destructive behavior: recursive removal
on a path that was itself a directory symlink removed the files inside the
external target, left the target directory present but empty, left the
symlink present, and returned failure. Recursive removal of a normal
directory containing a symlink did not follow that contained symlink; the
vulnerable case is when the selected deletion target is itself a directory
symlink.

**Impact**

A user who permanently deletes what appears to be one directory entry can
instead erase the contents of an unrelated directory referenced by that
symlink. A malicious or misleading symlink in a browsed folder can therefore
turn an ordinary delete action into data loss outside that folder.

**Action** — DONE (2026-07-24)

- [x] Classify a selected entry as a symlink before testing whether it is a
      directory. `FileOperationsController::removePaths()` now tests
      `!fi.isSymLink() && fi.isDir()`.
- [x] For permanent deletion, unlink a symlink with `QFile::remove()` and
      never construct a recursive `QDir` operation for it. New
      `FileOperations::removeSymLink()`.
- [x] For trash operations, move the symlink itself to trash without
      dereferencing its target — a link now routes to the file handler, whose
      trash path moves the link.
- [x] Centralize this rule in the file-operation layer so callers cannot
      accidentally reintroduce target-following behavior.
      `FileOperations::removeDir()` returns early to `removeSymLink()` for any
      link, so the rule holds even if a caller misclassifies a path.
- [ ] Preserve the existing confirmation flow, but identify the entry as a
      symbolic link when practical. Confirmation flow is preserved; *labelling*
      the entry as a link in the dialog text is still open (cosmetic).

Additionally fixed while here: `checkCanRemove()` treated a **dangling** symlink
as absent (`exists()` resolves the link), so broken links could not be removed
at all. `DirectoryModel::removeFile()` now drops both the file and dir entry,
since a link resolving to a directory is listed as a dir entry.

**Reproduction, before the fix** — a Qt probe on a directory symlink:
`removeRecursively()` returned `false`, both files inside the *target* were
gone, the target directory was left empty, and the link itself remained. The
user saw an error while data outside the browsed folder was already destroyed.

**Verified by** `src/tests/behavior/test_delete_does_not_follow_symlinks.cpp`
(6 cases: absolute-target, relative-target, dangling, ordinary directory,
directory containing a link, link to a file). Mutation-tested: reverting the
`removeDir()` guard makes it fail on target-file survival, not merely on a
return code.

Worth recording: `QDir::removeRecursively()` has its own guard for symlinks it
encounters *as entries* during a walk. The bug was only ever reachable when the
selected deletion target was itself a link — which is exactly the case the old
`isDir()` classification produced.

**Acceptance**

- Deleting a directory symlink removes only the link.
- Moving a directory symlink to trash moves only the link.
- The external target directory and every file beneath it remain unchanged.
- Deleting an ordinary directory still recursively removes its own contents.
- Tests cover valid, dangling, relative-target, and absolute-target symlinks.

### XL2. macOS “Show in Finder” permits AppleScript injection through a path

**Evidence**

`PlatformDesktop::showInDirectory()` in
`src/platform/platformdesktop_macos.cpp` concatenates `selectedPath` directly
into this AppleScript source fragment:

```text
select POSIX file "<selectedPath>"
```

The result is passed to `osascript` as executable source. macOS filenames can
contain quotes and newlines, so a crafted path can terminate the string and
insert additional AppleScript statements.

**Impact**

Using “Open containing directory” on an attacker-named file can execute
AppleScript, including shell commands, with the thumbgrid user’s privileges.

**Action**

- [ ] Remove the AppleScript construction entirely.
- [ ] Reveal the file with `QProcess::startDetached("/usr/bin/open",
      {"-R", selectedPath})`, keeping the executable and every argument
      separate.
- [ ] Do not add a shell fallback or rebuild AppleScript source by escaping
      user-controlled text.
- [ ] Report launch failure to the user and retain the existing directory-open
      fallback where appropriate.

**Acceptance**

- Paths containing quotes, newlines, spaces, Unicode, backslashes, and leading
  dashes remain one inert process argument.
- No part of a filename is interpreted as shell or AppleScript source.
- A normal file is selected in Finder and an empty selection opens the
  fallback directory.

### XL3. The recommended pacman repository trusts unsigned root packages

**Evidence**

`README.md` recommends:

```ini
[thumbgrid]
SigLevel = Optional TrustAll
Server = https://do-i.github.io/thumbgrid/arch/$arch
```

`.github/workflows/arch-package.yml` publishes the package and repository
database without package or database signatures.

**Impact**

Pacman installs package contents as root. Compromise of the publishing
workflow, GitHub account or Pages content, or another failure in the HTTPS
delivery path can therefore become persistent root-level code execution on
machines configured for automatic updates. The README warns that the channel
is low-stakes, but it still labels the configuration as recommended.

**Action**

- [ ] First complete the workflow action-pinning and permission isolation in
      L2 before introducing signing credentials.
- [ ] Create a dedicated package-signing subkey used only for thumbgrid
      releases; do not reuse a personal primary key.
- [ ] Store the signing subkey and passphrase as protected release-workflow
      secrets and import them only in the Arch publishing job.
- [ ] Sign every package and publish its detached `.sig`.
- [ ] Sign the repository database with the same release key.
- [ ] Publish the public key through a stable repository-controlled location
      and document its full fingerprint through a second prominent channel.
- [ ] Replace `Optional TrustAll` with signature-required pacman configuration
      after signatures are live.
- [ ] Document key import, fingerprint verification, rotation, revocation, and
      recovery procedures before calling the repository recommended.

**Acceptance**

- A clean pacman keyring can install from the repository only after trusting
  the documented fingerprint.
- An altered package, signature, or repository database fails verification.
- Normal `pacman -Syu` updates continue to work.
- Release logs do not expose private key material or its passphrase.

## High — L

### L1. Windows release builds consume unverified third-party code

**Evidence**

`scripts/build-thumbgrid.sh`, called by
`.github/workflows/build-package.yml`, currently:

- downloads dependency lists from a mutable `main` branch and expands their
  contents into a package-manager command;
- downloads prebuilt Qt, OpenCV, and mpv archives without checking a pinned
  digest;
- clones several image-format plugins from unpinned default branches; and
- compiles and packages those results into the published Windows artifact.

HTTPS protects transport but does not make mutable upstream content immutable
or detect a compromised upstream release/repository.

**Impact**

Compromise or unexpected movement of any upstream branch, dependency list, or
binary release can silently inject code into a thumbgrid Windows artifact.

**Action**

- [ ] Add a reviewed dependency lock manifest containing the immutable version
      or commit and SHA-256 digest for every downloaded or cloned input.
- [ ] Check archive hashes before extraction and abort on any mismatch.
- [ ] Check out every source dependency by full commit SHA, then verify `HEAD`
      before building.
- [ ] Replace the mutable remote package-list inputs with a versioned list
      tracked in this repository.
- [ ] Upgrade or remove the legacy Qt 5-era dependency path so the Windows
      artifact matches thumbgrid’s current Qt 6 build contract.
- [ ] Record the lock-manifest revision and resolved inputs in the artifact’s
      build metadata.

**Acceptance**

- Altering one archive byte causes the build to fail before extraction.
- Moving an upstream default branch does not change a rebuild.
- A missing or unexpected dependency commit causes a hard failure.
- Two builds from the same source and lock manifest resolve identical inputs.

### L2. Release workflows use mutable action tags with write-capable tokens

**Evidence**

Workflows reference actions through tags such as `actions/checkout@v7`,
`softprops/action-gh-release@v2`, and `msys2/setup-msys2@v2`. The Arch and
macOS workflows grant `contents: write` at workflow scope, including steps
that only need read access.

**Impact**

A compromised or unexpectedly moved action tag can execute inside a release
job and inherit repository write access. Future signing credentials would
raise that impact further.

**Action**

- [ ] Pin every first- and third-party action to a reviewed full commit SHA,
      retaining a comment with the human-readable release version.
- [ ] Set the default workflow permission to `contents: read`.
- [ ] Split build and publishing responsibilities where needed, granting
      `contents: write` only to the job that uploads or pushes release data.
- [ ] Keep signing secrets out of pull-request and ordinary build jobs.
- [ ] Configure automated dependency updates to propose action-SHA upgrades
      for review.
- [ ] Consider pinning release container images by digest and updating those
      digests through the same review process.

**Acceptance**

- Every `uses:` entry is immutable.
- Test and build-only jobs have read-only repository permissions.
- Only tag/manual release jobs can access release credentials or write
  repository content.

## Medium — M

### M1. Recursive copy or move accepts a destination inside its source

**Evidence**

`FileOperationsController::doInteractiveCopyMove()` creates the destination
directory and recursively enumerates the source. The general
`copyPathsTo()`/`movePathsTo()` entry points do not reject a destination that
is the source itself or one of its descendants. `Core::moveSelection()` has a
route-specific string check, but drag-and-drop, bookmarks, paste, and copy
routes can bypass it.

**Impact**

Copying or moving a folder into its own descendant can repeatedly reproduce
the source tree until path-length, filesystem, or disk-space limits stop it.
This is a local availability and data-integrity failure.

**Action** — DONE (2026-07-24)

- [x] Put one containment guard in `FileOperationsController` before
      confirmation or mutation so every route shares it.
      `destinationIsInsideSource()` runs at the top of both `copyPathsTo()` and
      `movePathsTo()`, ahead of the confirmation dialog.
- [x] Resolve the existing source and destination with canonical filesystem
      paths before comparison, including symlink aliases
      (`QFileInfo::canonicalFilePath()`).
- [x] For each directory source, reject a destination that is the same path or
      lies beneath it. Sources that are files, or are themselves symlinks, are
      skipped — a link is recreated as a link and never recurses.
- [x] Return a specific result/message without creating any destination entry:
      new `FileOpResult::DESTINATION_INSIDE_SOURCE`.
- [x] Remove the weaker UI-only check — the string comparison in
      `Core::moveSelection()` is gone, superseded by the central guard.

**Acceptance**

- Copy and move reject self, direct descendant, deep descendant, and
  symlink-resolved descendant destinations.
- Sibling and unrelated destinations continue to work.
- Rejection leaves the filesystem unchanged.
- File-only copy and move behavior is unaffected.

### M2. Rename accepts path traversal components

**Evidence**

`FileOperations::rename()` constructs the destination as:

```text
source-parent + "/" + newName
```

Both rename editors pass free-form text without enforcing that `newName` is
one filename component. A value containing `../` can therefore move an entry
outside the displayed folder and can reach the existing overwrite flow.

**Impact**

A nominal rename can become an out-of-folder move or overwrite. It does not
cross the current user’s operating-system permissions, but it violates the
operation’s trust boundary and creates a surprising destructive path.

**Action** — DONE (2026-07-24)

- [x] Validate names in the file-operation layer, not only in UI widgets —
      `FileOperations::isValidFileName()`, enforced inside
      `FileOperations::rename()` so no UI route can skip it.
- [x] Reject absolute paths, `.`, `..`, `/` and `\` on every platform. Note:
      an **empty** name keeps returning `NOTHING_TO_DO` rather than becoming an
      error, preserving today's behaviour when the user clears the field and
      presses Enter. It is still rejected without touching the filesystem,
      which is what the acceptance criterion requires.
- [x] Require the accepted value to be exactly one leaf filename component
      (explicit separator checks plus a `QFileInfo(name).fileName() == name`
      identity check).
- [x] Add a distinct invalid-name result (`FileOpResult::INVALID_NAME`) and
      keep the rename editor open with a clear error —
      `Core::renameCurrentSelection()` now reopens the rename overlay with the
      rejected text, alongside the existing error message.

**Acceptance**

- Traversal, absolute paths, both separator styles, `.` and `..` are rejected
  without filesystem changes.
- Valid names containing spaces, Unicode, multiple dots, and leading dots
  still work where the platform permits them.
- Existing overwrite confirmation still applies to a valid same-directory
  destination.

### M3. macOS and Windows artifacts lack verifiable publisher identity

**Evidence**

The macOS workflow uses ad-hoc signing (`codesign --sign -`) and does not
notarize or staple the app/DMG. The Windows development-release workflow does
not Authenticode-sign its executable or archive. Release artifacts also lack
a signed checksum/provenance manifest.

**Impact**

Users cannot cryptographically verify that these artifacts were published by
the expected maintainer, and platform trust systems cannot provide normal
publisher-identity guarantees.

**Action**

- [ ] Sign macOS release builds with a Developer ID certificate, submit them
      for notarization, staple the result, and verify before upload.
- [ ] Authenticode-sign Windows executables and verify the signature before
      packaging and publishing.
- [ ] Publish a signed checksum or provenance manifest for every release
      artifact.
- [ ] Limit credentials to protected release jobs and document certificate
      rotation/revocation.
- [ ] Until signing exists, label the affected downloads as unsigned rather
      than implying verified publisher identity.

**Acceptance**

- Platform-native verification reports the expected publisher identity.
- macOS notarization validation succeeds offline after stapling.
- Altered artifacts fail signature or provenance verification.

## Low — S

### S1. Video thumbnails use predictable shared output names

**Evidence**

`ThumbnailerRunnable::createVideoThumbnail()` writes mpv output to
`settings->tmpDir() + source-basename + ".png"`. Different source directories
with the same basename share the output path, and multiple thumbnail workers
can use it concurrently. Linux also permits the cache root to be configured.

**Impact**

Concurrent jobs can read or remove another job’s output, producing incorrect
previews or failures. A poorly chosen shared cache directory also creates a
predictable-file/symlink hardening concern.

**Action** — DONE (2026-07-24)

- [x] Give every video-thumbnail task a unique `QTemporaryDir` beneath the
      cache root (`<tmpDir>/videothumb-XXXXXX`).
- [x] Write the mpv frame to a fixed filename (`frame.png`) inside it.
- [x] Keep `--no-config` and `--load-scripts=no` — unchanged.
- [x] Check process start, timeout, exit status and output presence
      explicitly; a timeout now kills mpv instead of leaving it running. The
      `QTemporaryDir` handles cleanup on every path, including early returns.
- [x] Ensure the private directory is owner-accessible only
      (`ReadOwner | WriteOwner | ExeOwner`).

Care taken: the caller dereferences the returned image unconditionally, so
every failure path returns a valid **empty** `QImage` rather than a null
pointer. Verified that mpv still produces a valid PNG frame at the new path
shape and exits 0, so the added checks do not reject the happy path.

**Acceptance**

- Parallel thumbnail jobs for identically named videos never share a path.
- Timeout and decode failure leave no persistent temporary output.
- A configured cache root cannot make task output globally predictable.

## Cross-cutting validation

After each independently implemented item:

- [ ] Run the smallest targeted regression test first.
- [ ] Run the complete thumbgrid behavior suite.
- [ ] Run `scripts/check-packaging.sh` for packaging or release changes.
- [ ] Run `bash -n` over every changed shell script.
- [ ] Run `git diff --check`.
- [ ] Review the diff for accidental secrets, credentials, absolute local
      paths, and unrelated changes.

## Audit limitations and clean checks

- A repository scan found no committed private keys or recognizable access
  tokens. The GitHub token reference in the Arch workflow is a normal runtime
  secret reference, not a committed credential.
- `bash -n` passed for `run.sh` and all tracked scripts reviewed by this audit.
- A focused `clang-tidy` pass was attempted, but the current compilation
  database contains `-mno-direct-extern-access`, which the installed clang
  rejects. Automated analyzer coverage is therefore incomplete and this
  review must not be described as exhaustive.
- This review did not perform a current CVE inventory of Qt, Exiv2, OpenCV,
  mpv, image-format plugins, Homebrew/MSYS2/Arch packages, or GitHub Actions.
- This review did not fuzz media parsers, perform dynamic instrumentation, or
  test macOS/Windows behavior on those operating systems.
