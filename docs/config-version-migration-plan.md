# Plan: version-gate thumbgrid.conf migrations

## Problem

Config migrations today (`migrateConfigGroups`, `migrateLegacyThemeConf`,
`migrateLegacyShortcuts` in settings.cpp) detect "do I need to migrate?" by
sniffing file shape: does a key exist, is it in the old flat location, does
`theme.conf` exist but not `theme.ini`, etc. This works but every future
migration adds another heuristic, and they all run unconditionally on every
boot even when there's nothing to do.

## Key finding: this mechanism partially exists already

`Settings::lastVersion()`/`setLastVersion()` store `lastVerMajor/Minor/Micro`
under `[General]` in thumbgrid.conf, and `Core::onUpdate()` (core.cpp:262-284)
already does exactly the version-gated pattern discussed:

```cpp
QVersionNumber lastVer = settings->lastVersion();
if(lastVer < QVersionNumber(0,9,2)) { ... }
#ifdef USE_OPENCV
if(lastVer < QVersionNumber(0,9,0)) settings->setScalingFilter(...);
#endif
actionManager->adjustFromVersion(lastVer);
...
settings->setLastVersion(appVersion);   // bumped once, at the end
```

This only runs when `appVersion > lastVersion` (checked in `Core::Core()`,
core.cpp:37); `onFirstRun()` handles the fresh-install case and stamps the
version immediately since there's nothing to migrate.

So: no new schema-version key is needed, and no "old configs predate the
version field" bootstrap problem exists — every shipped config already has
`lastVerMajor/Minor/Micro`. The actual gap is that **settings.cpp's own
migrations don't use this value**; they re-derive "is this old?" from shape
instead of reading the version that's sitting right there.

## Proposed change

1. **Reuse `lastVersion()`/`appVersion` as the single migration gate**, not a
   separate counter. One version number, one meaning: "schema + behavior as
   of this release."

2. **Convert settings.cpp migrations to version-gated blocks**, mirroring
   `onUpdate()`'s style:
   - `migrateConfigGroups()` gains a `QVersionNumber lastVer` parameter (read
     once, passed down from the constructor via `lastVersion()`).
   - Each migration step gets a threshold, e.g.
     `if(lastVer < QVersionNumber(0,10,0)) { /* move flat keys into groups */ }`.
   - `migrateLegacyThemeConf` / `migrateLegacyShortcuts` get the same
     treatment instead of (or in addition to, see below) their current
     `QFile::exists()` checks.

3. **Keep the existing shape checks as a cheap inner guard, not the primary
   gate.** Once version-gated, a migration block only runs at all when
   `lastVer` is old; the `if(!QFile::exists(...))` checks inside it become
   redundant-but-harmless double protection rather than the only protection.
   This is a safety net for any config that, for whatever reason, has a
   version number newer than its actual contents (e.g. hand-edited).

4. **Do not bump the stored version from inside `Settings`.** The version
   write must stay a single event at the end of the whole migration chain —
   currently `Core::onUpdate()` / `Core::onFirstRun()` — so that:
   - Settings-layer migrations (run in the `Settings` constructor, before
     `Core` exists) and
   - Core-layer migrations (run in `Core::onUpdate()`, which also reads
     `lastVersion()`)

   both see the *same* pre-migration version number during a given boot.
   `Settings::Settings()` reads `lastVersion()` for its own gating but never
   calls `setLastVersion()`.

5. **Downgrade guard.** `Core::onUpdate()` is already only invoked when
   `appVersion > lastVersion()`, so a downgrade (old build, newer stored
   version) already skips the Core-layer migrations and never rewrites the
   version field. Settings-layer migrations need the same explicit guard
   once they're version-gated, since the `Settings` constructor doesn't
   otherwise know whether this is an upgrade or downgrade boot:
   ```cpp
   if(lastVer < appVersion) runVersionedMigrations(lastVer);
   ```
   On a downgrade, nothing in settings.cpp touches the file at all.

## Code touchpoints

- `src/settings.h`: no new stored fields (`lastVersion()` already exists).
  Add a private `void runVersionedMigrations(const QVersionNumber &lastVer);`
  replacing today's unconditional `migrateConfigGroups()` call.
- `src/settings.cpp`:
  - `Settings::Settings()` constructor: call
    `runVersionedMigrations(lastVersion())` after the per-platform
    `settingsConf`/`themeConf`/`mShortcutsJsonPath` setup, guarded by
    `lastVer < appVersion`.
  - Fold `migrateConfigGroups`, `migrateLegacyThemeConf`,
    `migrateLegacyShortcuts` into version-thresholded blocks inside (or
    called from) `runVersionedMigrations`.
  - `#include "appversion.h"` for `appVersion`.
- `src/core.cpp`: no behavior change; `onUpdate()` keeps doing what it does,
  just now sits alongside settings.cpp migrations that read the same
  `lastVer` value.

## Migration-authoring convention going forward

When a schema or default changes, add one dated, version-thresholded block
in the appropriate layer:
- Data shape / on-disk key layout → `settings.cpp` `runVersionedMigrations()`.
- Behavior / action defaults / user-facing messaging → `core.cpp`
  `Core::onUpdate()` (unchanged, already this way).

This reads like a changelog (`if(lastVer < X) { ... }`) instead of scattered
existence checks, and new contributors have one place to look for "what
migrates when."

## Testing

- Extend `test_shortcut_contexts_persist_and_legacy_configs_migrate.cpp` and
  `test_custom_theme_persistence.cpp` (or add a new behavior test) with two
  cases:
  1. Seed `lastVerMajor/Minor/Micro` below a migration's threshold + legacy
     key shape → assert migration runs.
  2. Seed current version + a legacy-shaped key left over by hand → assert
     migration is *skipped* (proves the version gate is actually the primary
     gate, not just harmlessly redundant with the shape check).
- Add a downgrade case: seed a version newer than the running build's
  `appVersion` → assert settings.cpp does not modify the file.

## Non-goals

- No separate `confVersion` key distinct from `lastVerMajor/Minor/Micro`.
- No filename-based versioning (`thumbgrid-v3.conf`) — rejected earlier in
  discussion: an old binary already can't run past `onUpdate()`'s version
  check to begin with, and versioned filenames would require reworking the
  fixed-path `QSettings` construction in both the Linux and non-Linux
  branches of the `Settings` constructor for no added safety.
- No changes to `theme.ini` / `shortcuts.json` file formats themselves.
