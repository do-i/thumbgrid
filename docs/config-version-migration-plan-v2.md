# Plan: hybrid version-gated config migrations

## Problem

Config migrations today (`migrateConfigGroups`, `migrateLegacyThemeConf`,
`migrateLegacyShortcuts` in `settings.cpp`) mostly decide whether to run by
sniffing file shape: does a key exist, is it still in the old flat location,
does `theme.conf` exist but not `theme.ini`, etc.

That is useful for recovery, but it is a poor primary structure for future
release migrations. Every new migration adds another unconditional startup
check, and the intended release ordering becomes harder to see.

## Key finding

The app already has a release-version gate:

- `Settings::lastVersion()` / `setLastVersion()` store
  `lastVerMajor/Minor/Micro` under `[General]` in `thumbgrid.conf`.
- `Core::Core()` calls `Core::onUpdate()` only when `appVersion > lastVersion`.
- `Core::onUpdate()` already uses `if(lastVer < QVersionNumber(...))` blocks
  and stamps the new version once at the end.

However, `Settings` is constructed before `Core`, and missing version keys read
as `0.0.0`. Fresh installs are not stamped until `Core::onFirstRun()`. So
settings-layer migrations cannot blindly treat `lastVer < appVersion` as "this
is an upgrade" without also checking `firstRun()`.

## Proposed approach

Use a two-bucket model:

```cpp
const QVersionNumber lastVer = lastVersion();
const bool hasExistingSettings = !settingsConf->allKeys().isEmpty();

runConfigRecoveryMigrations();

if((!firstRun() || hasExistingSettings) && lastVer < appVersion)
    runVersionedSettingsMigrations(lastVer);
```

Recovery runs before versioned grouping because the grouping migration preserves
the active theme pointer from a recovered `theme.ini`.

### 1. Versioned settings migrations

Use version-gated blocks for deterministic release-to-release layout changes.
These are changes where an existing config from an older release should be
rewritten once, and a current-version config should be trusted even if it still
contains odd hand-edited leftovers.

Initial candidate:

```cpp
void Settings::runVersionedSettingsMigrations(const QVersionNumber &lastVer) {
    if(lastVer < QVersionNumber(2026, 7, 7)) {
        migrateConfigGroups();
    }
}
```

The threshold must be the release version that first ships the migration. Do not
use a placeholder threshold in committed code.

### 2. Recovery migrations

Keep shape-based checks for safe user-data rescue paths. These should run
independently of `lastVersion()` because they are not really "upgrade from
release X" logic; they are "canonical target is missing, but legacy data exists"
logic.

Keep these as recovery-style migrations:

- `migrateLegacyThemeConf(confDir, themeConf)`: if `theme.conf` exists and
  `theme.ini` is absent, import the legacy theme file. This is safe even for a
  current-version config where `theme.ini` was deleted accidentally.
- `migrateLegacyShortcuts(settingsConf, mShortcutsJsonPath)`: if
  `shortcuts.json` is absent and `[Controls]/shortcuts` exists, import the old
  shortcut overrides. This is currently lazy inside `Settings::readShortcuts()`;
  keeping it lazy avoids creating or rewriting shortcut files before shortcuts
  are actually read.

Recovery migrations must remain guarded by target-file/key checks and should be
idempotent.

## Code touchpoints

- `src/settings.h`
  - Add private:
    `void runVersionedSettingsMigrations(const QVersionNumber &lastVer);`
  - Optionally add private:
    `void runConfigRecoveryMigrations();`

- `src/settings.cpp`
  - Include `appversion.h`.
  - In `Settings::Settings()`, after `settingsConf`, `themeConf`, and
    `mShortcutsJsonPath` are initialized:

    ```cpp
    const QVersionNumber lastVer = lastVersion();
    const bool hasExistingSettings = !settingsConf->allKeys().isEmpty();
    runConfigRecoveryMigrations();

    if((!firstRun() || hasExistingSettings) && lastVer < appVersion)
        runVersionedSettingsMigrations(lastVer);

    fillVideoFormats();
    ```

  - Move `migrateConfigGroups()` behind a version threshold inside
    `runVersionedSettingsMigrations()`.
  - Keep `migrateLegacyThemeConf()` recovery-style. On Linux/FreeBSD it can
    remain near the path setup, or it can move into `runConfigRecoveryMigrations()`
    after `themeConf` is constructed.
  - Keep `migrateLegacyShortcuts()` lazy in `readShortcuts()` unless there is a
    concrete reason to migrate shortcut overrides during `Settings` construction.

- `src/core.cpp`
  - No behavior change. `Core::onUpdate()` remains responsible for behavior
    migrations, action-default adjustments, user-facing update messages, and the
    final `setLastVersion(appVersion)` stamp.

## Migration-authoring convention

When a schema or default changes, choose the migration bucket explicitly:

- On-disk layout changes tied to a release:
  `settings.cpp` `runVersionedSettingsMigrations(lastVer)`.
- Safe rescue from legacy files or missing canonical files:
  `settings.cpp` `runConfigRecoveryMigrations()` or the narrow lazy reader that
  owns the data, such as `readShortcuts()`.
- Runtime behavior, action defaults, and user-facing update messaging:
  `core.cpp` `Core::onUpdate()`.

Do not bump the stored version from `Settings`. The stored version must still be
written once, at the end of `Core::onUpdate()` or `Core::onFirstRun()`, so both
settings-layer and core-layer migrations observe the same pre-migration version
for the current boot.

## Downgrade behavior

If the stored version is newer than the running `appVersion`, skip
`runVersionedSettingsMigrations()`. Do not rewrite the version field.

Recovery migrations may still run if their target-file/key guards prove the
operation is safe. For example, importing `theme.conf` only when `theme.ini` is
missing is a recovery operation, not a downgrade rewrite.

## Testing

Add focused behavior tests for:

1. Fresh install: no existing config and `firstRun=true` should not run
   versioned migrations just because missing `lastVer*` reads as `0.0.0`.
2. Upgrade: seed `firstRun=false`, an old `lastVer*`, and old flat keys; assert
   `migrateConfigGroups()` moves keys to the expected groups.
3. Current version with stale flat keys: seed `firstRun=false` and current
   `lastVer*`; assert versioned grouping is skipped, proving the version gate is
   primary for layout migrations.
4. Downgrade: seed `firstRun=false` and a version newer than `appVersion`;
   assert versioned settings migrations do not modify `thumbgrid.conf`.
5. Theme recovery: with current `lastVer*`, `theme.conf` present, and
   `theme.ini` absent, assert the legacy theme is imported.
6. Shortcut recovery: with current `lastVer*`, no `shortcuts.json`, and
   `[Controls]/shortcuts` present, assert `readShortcuts()` imports the legacy
   overrides into `shortcuts.json`.

## Non-goals

- No separate `confVersion` key distinct from `lastVerMajor/Minor/Micro`.
- No filename-based config versioning such as `thumbgrid-v3.conf`.
- No changes to the `theme.ini` or `shortcuts.json` formats themselves.
- No eager creation of `shortcuts.json` during startup unless a future feature
  needs it explicitly.
