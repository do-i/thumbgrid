# Shortcuts & Keymap Redesign Plan

> Supersedes and absorbs `docs/inputmap-resource-redesign-plan.md` (the scan-code
> extraction is Part 1 below). This document adds viewer presets and the
> single-active-file shortcut model.

## Goals

1. **Move OS scan-code keymaps out of C++** into per-OS resource files, so the
   `keyMap.insert(...)` walls in `inputmap.cpp` become editable data.
2. **Introduce viewer presets** — resource files that mirror the shortcut schemes
   of major image viewers (qimgv, gwenview, XnViewMP, IrfanView, …). The user
   picks one at runtime.
3. **Make `shortcuts.json` the single active mapping.** Selecting a preset copies
   that preset's resource into `shortcuts.json`. There is no system-default
   mapping layer feeding dispatch anymore.
4. **First run**: if the user has no `shortcuts.json`, the app materializes it
   from the **qimgv** preset at boot (thumbgrid is a qimgv fork, so qimgv is the
   sensible default scheme).

## Current State (what we're changing)

- `src/utils/inputmap.cpp` — `initKeyMap()` hardcodes ~280 `keyMap.insert()`
  lines behind `#ifdef _WIN32` / `__linux__`. `keyNameCtrl/Alt/Shift()` hardcode
  macOS modifier glyphs behind `__APPLE__`.
- `src/res/default-shortcuts.json` — the default action→keys catalog, installed
  to `/etc/thumbgrid/default-shortcuts.json` and embedded as a qrc fallback.
- `ActionManager::resolveDefaultShortcutsPath()` prefers the installed system
  copy (`SHORTCUTS_PATH`), then the qrc copy. `initDefaults()` loads it into the
  in-memory `defaults` map.
- `initShortcuts()` reads user `shortcuts.json` into `shortcuts`; if empty, it
  copies `defaults`. It then **hardcodes** three "merge missing new action" calls
  (`mergeMissing("toggleStatusFooter", …)`, `cutFile`, `togglePlacesPanel`).
- `adjustFromVersion()` runs version-gated fix-ups plus a generic loop that adds
  any default action whose introduced-version is newer than the config's version.
- `defaults` is also consumed by the settings dialog (show default keys, reset a
  context) and by `core.cpp` version migration.

Net: defaults behave as a **live layer** loaded alongside user shortcuts, and
adding a new action or keymap entry requires editing C++.

## Target Design

### One active file, presets as seed

- `shortcuts.json` (user config dir, resolved by
  `PlatformDesktop::shortcutsJsonPath`) is the **only** runtime mapping. Dispatch,
  save, and the shortcut editor all operate on it — unchanged from today.
- The in-memory `defaults` map is kept, but its **source changes**: it is loaded
  from the **currently selected preset resource**, not from
  `/etc/thumbgrid/default-shortcuts.json`. This keeps reset-to-defaults and the
  settings dialog's "default keys" column working with no UI rewrite.
- No system-default mapping layer. `SHORTCUTS_PATH` lookup for the active mapping
  is removed; packaging stops installing `default-shortcuts.json`.

### Presets

- New resource dir `src/res/presets/` holds one file per scheme. Each file is the
  **current `default-shortcuts.json` schema** (context objects
  `global`/`document`/`grid`, a `platform` block, `$Ctrl`/`$Alt`/`$Shift` tokens)
  **plus a self-describing `_meta` block** — the metadata is per-file, not in a
  central manifest, so a preset carries its own OS support/label/order and can be
  dropped in without editing anything else. The existing `insertShortcutObject()`
  loader ignores `_meta`, so mapping parsing is reused verbatim.

  ```jsonc
  {
    "_meta": {
      "id": "irfanview",
      "name": "IrfanView-style",
      "description": "Shortcuts modeled on IrfanView defaults",
      "os": ["windows"],
      "order": 40,
      "source": "IrfanView documented shortcuts",
      "notice": "Independent reconstruction; names are trademarks of their owners; not affiliated."
    },
    "global":   { /* action -> [keys] */ },
    "document": { /* … */ },
    "grid":     { /* … */ },
    "platform": { "windows": { /* … */ }, "default": { /* … */ } }
  }
  ```

  - `presets/qimgv.json` — the current `default-shortcuts.json` content, renamed,
    with `_meta.os = ["linux","windows","macos"]`. Default + first-run seed.
  - `presets/gwenview.json` (`os:["linux"]`), `presets/xnviewmp.json`
    (`os:["linux","windows","macos"]`), `presets/irfanview.json`
    (`os:["windows"]`) — authored from each viewer's documented defaults.
  - `_meta` never reaches `shortcuts.json`: materializing goes load → ContextMap →
    `writeShortcutsJson`, which only emits the mapping.

### The preset store

A new **`ShortcutPresetStore`** (`src/shortcutpresetstore.{h,cpp}`) owns the
catalog, mirroring `ThemeStore`'s role (catalog vs. running theme):

```cpp
struct PresetInfo { QString id, name, description; QStringList os; int order; };

class ShortcutPresetStore {
public:
    static QList<PresetInfo> available(bool currentOsOnly = true); // scan + read _meta, sort by order
    static PresetInfo meta(const QString &id);
    static QString resolvePath(const QString &id);                 // system dir -> qrc fallback
    static bool load(const QString &id, ActionManager::ShortcutMap &out); // tokens + platform resolved
    static QString currentOsToken();  // "windows" | "macos" | "linux" (FreeBSD -> "linux")
};
```

- **Enumeration** = scan the presets dir(s) and read each `_meta` (no central
  manifest to drift). Order comes from `_meta.order`; ties break alphabetically,
  with `qimgv` conventionally first.
- **Sources, mirroring themes' `THEMES_PATH`**: resolve a preset from a system
  dir (`/etc/thumbgrid/presets/`, admin-editable, add a `PRESETS_PATH` define),
  then the embedded qrc copy. The store merges both by id (system overrides
  embedded). This is the sanctioned way for admins/packagers/users to ship extra
  presets — the replacement for editing the retired system `default-shortcuts.json`.
  (Can ship qrc-only first and add the system dir as a fast follow.)
- `ActionManager::availablePresets()` / `applyPreset()` / the first-run seed all
  delegate here; `ActionManager` keeps only the active mapping and dispatch.

### OS-specific presets

Two independent kinds of OS-specificity — handle both:

1. **Whole-preset scoping (which presets are listed).** `_meta.os` declares where
   a preset is offered. `ShortcutPresetStore::available()` filters to presets whose
   `os` includes `currentOsToken()` (IrfanView-style on Windows, Gwenview-style on
   Linux, etc.). A niche "show presets for other platforms" toggle can pass
   `currentOsOnly=false`, off by default.
   - Which presets are *embedded* is a build choice (see **Build-time preset
     selection**); `_meta.os` drives both the build-time set and the runtime
     selector filter, so they stay consistent. A preset can also be supplied at
     runtime from the system `PRESETS_PATH` dir even if it wasn't compiled in.
   - If the stored `[Shortcuts]/preset` id resolves to no embedded/installed file,
     the selector shows it annotated (e.g. "IrfanView-style (unavailable)") and
     reset/compare degrade gracefully — **the mapping in `shortcuts.json` is never
     altered** (see graceful degradation below).

### Build-time preset selection

CMake can control which presets are compiled into the binary — useful for
minimal builds and for packagers who only want their platform's schemes.

- The project uses `CMAKE_AUTORCC ON` on a static `resources.qrc`. Keep that for
  everything else, but move presets into a **generated** resource list fed to
  `qt_add_resources(thumbgrid PREFIX "/res/presets" FILES ${THUMBGRID_PRESET_FILES})`
  (or a `configure_file`-generated `presets.qrc`), so the embedded set is
  computed at configure time instead of hard-coded in the `.qrc`.
- **No hard-coded per-OS lists in CMake — infer from `_meta.os`.** CMake reads
  each preset's `_meta.os` and embeds only those whose array contains the target
  OS token (exactly your `filter(dest_os in preset.os)`). `_meta.os` is the single
  source of truth for both the build set and the runtime selector. This needs
  `string(JSON)` (**CMake ≥3.19**); bump the declared floor from 3.13 (Qt6 already
  requires ≥3.16, so this is cheap):

  ```cmake
  if(WIN32)                                   set(TARGET_OS windows)
  elseif(APPLE)                               set(TARGET_OS macos)
  else()                                      set(TARGET_OS linux)   # incl. *BSD (shares keymap_linux)
  endif()

  file(GLOB PRESET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/presets/*.json")
  set(THUMBGRID_PRESET_FILES "")
  foreach(pf IN LISTS PRESET_FILES)
    file(READ "${pf}" j)
    string(JSON id GET "${j}" _meta id)
    string(JSON os_len ERROR_VARIABLE e LENGTH "${j}" _meta os)
    set(supported FALSE)
    if(NOT e AND os_len GREATER 0)
      math(EXPR last "${os_len} - 1")
      foreach(i RANGE 0 ${last})
        string(JSON os_i GET "${j}" _meta os ${i})
        if(os_i STREQUAL TARGET_OS)
          set(supported TRUE)
          break()
        endif()
      endforeach()
    endif()
    if(supported OR id STREQUAL "qimgv")          # qimgv always embedded
      list(APPEND THUMBGRID_PRESET_FILES "${pf}")
    endif()
  endforeach()
  ```

  `CONFIGURE_DEPENDS` re-globs when a preset file is added; `qimgv` is force-added
  regardless of its `_meta.os` because it is the default and first-run seed and
  must never be prunable.
- **Optional explicit override** for packagers who want an exact set:
  `-DTHUMBGRID_PRESETS="qimgv;xnviewmp"` (list of ids) short-circuits the inference
  and embeds exactly those (still with `qimgv` forced in).
- **Pre-3.19 fallback only** (if the floor cannot be raised): a short explicit
  per-OS list in CMake, documented as a duplication of `_meta.os`. Not preferred.
- **Graceful degradation** (required, since pruning can drop a selected preset):
  `ShortcutPresetStore::resolvePath()` tries the system `PRESETS_PATH` dir, then
  the embedded qrc. If a selected preset resolves to neither:
  - Dispatch is unaffected — the full mapping already lives in `shortcuts.json`.
  - Reset-to-defaults falls back to `qimgv` (always embedded); the selector marks
    the preset "unavailable".
  - `modified` cannot be recomputed by comparison, so it is left unchanged
    (treated as diverged). The mapping is never rewritten.

2. **Per-OS bindings within one preset.** A cross-platform preset (qimgv,
   XnView MP) may need a few keys to differ per OS. This is the existing
   `platform` block — today only `apple` vs `default`. **Generalize the loader**
   from `apple`/`default` to `windows` / `macos` / `linux` / `default`, selecting
   the block matching the current OS and falling back to `default`. One file per
   preset still covers per-OS differences; no OS-suffixed preset files needed.
   (Modifier glyphs like `⌘` are already handled separately by the keymap
   `modifiers` block + `$Ctrl` tokens, so `platform` carries only genuine
   key-choice differences.)
- The selected preset id is stored in `thumbgrid.conf` under a **`[Shortcuts]`**
  group, alongside a `modified` flag:

  ```ini
  [Shortcuts]
  preset=qimgv
  modified=false
  ```

  - `preset` — the selected preset id (default `qimgv`).
  - `modified` — `true` once the active `shortcuts.json` diverges from that
    preset (the user edited a binding). It drives UI affordances like showing
    "qimgv (modified)" and warning before a preset switch discards edits.
  - Both keys route to the `[Shortcuts]` section by adding
    `{"preset", "Shortcuts"}` and `{"modified", "Shortcuts"}` to the
    `settingGroupFor` map in `settings.cpp`; `readSetting`/`writeSetting` then
    place them correctly with no other plumbing. Only these pointers live in
    config; the mapping itself lives in `shortcuts.json`.

### Keymaps (Part 1 — scan-code extraction)

- New resource dir `src/res/keymaps/`:
  - `keymap_linux.json` (also FreeBSD), `keymap_windows.json`,
    `keymap_fallback.json`.
  - Schema: `{ "keys": { "<scancode>": "<CanonicalName>", … },
    "modifiers": { "ctrl": "Ctrl", "alt": "Alt", "shift": "Shift" } }`.
  - Preserve the Windows Qt 6.7 alternate scan-code entries and all current
    canonical names **exactly** (they must match user `shortcuts.json` strings).
- `keymap_macos.json` carries only `modifiers` (the `⌘`/`⌥`/`⇧` glyphs); macOS
  keeps its text-event path with an empty key map — **no** new native scan-code
  table (unchanged behavior).
- `InputMap::initKeyMap()` / `initModMap()` load the JSON, selecting the resource
  path with a single `#ifdef` (all keymap files are embedded via qrc; they are
  code data, not admin-editable, so no system path and no CMake define needed).
- `keyNameCtrl/Alt/Shift()` read from the loaded `modifiers` block instead of
  `#ifdef __APPLE__`. (Alternative kept in "Open questions": leave the tiny C++
  helper. Recommendation: move to data for consistency with the goal.)

## Naming & Attribution

Not legal advice — engineering guidance. The presets emulate third-party viewers,
so the concern is trademark (mild), not copyright.

- **Copyright is not implicated by the mappings.** "Which key does which action"
  is a functional/factual system, not creative expression. **Author each preset
  independently** from the target viewer's documentation / observed behavior — do
  **not** copy its config files or source (those carry their own license: qimgv
  and Gwenview are GPL; XnView MP and IrfanView are proprietary freeware).
- **Trademark — use nominative/descriptive framing.** Refer to a scheme for
  compatibility only; don't imply endorsement or affiliation.
  - Display names use a **"-style"** / "-compatible" convention rather than a bare
    product name: e.g. `Gwenview-style`, `XnView MP-style`, `IrfanView-style`.
    The `_meta.id` stays a short slug (`gwenview`, `xnviewmp`, `irfanview`);
    `_meta.name` carries the qualifier.
  - **Do not ship third-party logos or icons.**
  - Put a per-file disclaimer in each preset's `_meta.notice` and a shared
    `src/res/presets/NOTICE.md` beside the presets:

    > Preset names refer to third-party applications for keyboard-shortcut
    > compatibility only. Those names are trademarks of their respective owners,
    > who do not endorse or sponsor thumbgrid. Mappings are independent
    > reconstructions, not derived from the applications' source or config files.
- **qimgv** is low risk — thumbgrid is its fork and already carries its GPL
  lineage; naming the default preset after it is natural. Keep its label a plain
  `qimgv` (the fork's own ancestor), no "-style" qualifier needed.

## Data Flows

- **First run (no `shortcuts.json`)**: `initShortcuts()` detects the file is
  absent/empty → loads the selected preset (default qimgv) into `defaults` →
  copies it into `shortcuts` → `saveShortcuts()` writes `shortcuts.json` → sets
  `[Shortcuts]/preset=qimgv`, `modified=false`. "Copy" goes through the loader so
  `$Ctrl` tokens and the `platform` block are resolved before writing (a raw byte
  copy would leave tokens unexpanded).
- **Select a preset at runtime**: `ActionManager::applyPreset(id)` → load preset
  resource → `shortcuts = preset` → `setSelectedPreset(id)` → `saveShortcuts()` →
  set `modified=false` → `rebuildShortcutLookup()`. This **overwrites** the user's
  customizations, so the UI gates it behind a confirm dialog.
- **User edits a binding**: any mutation that persists `shortcuts.json`
  (`addShortcut`/`removeShortcut`/single-action reset/settings-dialog apply)
  recomputes `modified`. Recommended rule: on save, set
  `modified = (shortcuts != selected-preset materialized)` — a self-correcting
  comparison, so manually reverting to the preset scheme clears the flag rather
  than a one-way dirty bit that never resets.
- **Reset to defaults** (existing buttons): unchanged code path; `defaults` now
  points at the selected preset, so "reset" restores the selected scheme. A full
  reset-all clears `modified`; a single-action reset recomputes it by comparison.
- **Upgrade (new action added in a release)**: `adjustFromVersion()`'s generic
  version-gated loop already adds any `defaults` action whose introduced-version
  is newer than the stored config version. Because `defaults` = the qimgv preset
  (which contains every shipped default), this loop covers new actions with **no
  per-action C++**. The three hardcoded `mergeMissing()` calls are removed once
  we confirm `toggleStatusFooter`/`cutFile`/`togglePlacesPanel` carry correct
  introduced-versions in `appActions` (audit step; add versions if missing).
- **Dispatch**: unchanged — `processEvent()` → `ShortcutBuilder::fromEvent()` →
  `shortcutLookup`. `ShortcutBuilder` reads the same `InputMap`, now data-backed.

## Migration

**Guiding principle: the migration never rewrites an existing `shortcuts.json`.**
User bindings already live in that file and carry over untouched. Migration only
backfills the new `[Shortcuts]` conf pointers and retires the system-default
source. There is no key-name or schema change, so nothing in the mapping needs
converting.

### Existing user upgrading (has `shortcuts.json`, no `[Shortcuts]` group)

Version-gated in `Settings::runVersionedSettingsMigrations()` behind a new
`lastVer < QVersionNumber(<redesign-release>)` gate — same mechanism as the
existing `2026,7,7` gate (date-versioned; pick the release that ships this):

1. If `[Shortcuts]/preset` is absent → write `preset=qimgv`.
2. Materialize the qimgv preset exactly as first-run does (token expansion,
   `platform` block, sort) and write `modified = (user shortcuts != qimgv
   materialized)`. Untouched configs → `false`; customized configs → `true`.

Because `selectedPreset()` **defaults to `qimgv`** when the key is absent,
`ActionManager::initDefaults()` already loads the qimgv preset into `defaults`
even on the first migrated boot — nothing depends on the backfill having run
first, so ordering between the settings migration and ActionManager init is not a
hazard. `qimgv.json` is the renamed old `default-shortcuts.json`, so an untouched
user's file compares **equal** and correctly reads as unmodified.

### New-action continuity (removing `mergeMissing`)

`adjustFromVersion()`'s generic loop keeps running and now seeds from the qimgv
preset (which contains every shipped default), so actions introduced since
`lastVer` are still back-filled. Deleting the three hardcoded `mergeMissing()`
calls is safe **iff** `toggleStatusFooter`/`cutFile`/`togglePlacesPanel` carry
correct introduced-versions in `appActions` — audit and add any missing versions
**before** removing the calls. Users who deliberately dropped an action (tracked
in `_disabled`) keep it off; this is intended.

### Fresh install

Not a migration — the first-run flow materializes qimgv and sets
`preset=qimgv, modified=false`.

### Retiring the system default file

- Packaging stops installing `/etc/thumbgrid/default-shortcuts.json`; the
  `SHORTCUTS_PATH` runtime lookup is removed.
- A leftover system file from an older package is **harmless** (nothing reads it);
  packagers may delete it on upgrade.
- **Behavior change to announce:** admins who *edited*
  `/etc/thumbgrid/default-shortcuts.json` for site-wide defaults will find those
  edits ignored under the new model. Mitigation: drop a preset file into
  `${SYSCONFDIR}/thumbgrid/presets/` (resolved by `ShortcutPresetStore` via
  `PRESETS_PATH`) — the sanctioned replacement for editing the global mapping.
  Note this in packaging/`NOTICE`.

### Untouched by this migration

`migrateLegacyShortcuts()` (INI→JSON) and `readShortcutsJson()`'s legacy-shape /
`_primary` / `_disabled` handling are orthogonal and unchanged.

### Rollback safety

Because the mapping file is never rewritten, **downgrading** to a pre-redesign
build still reads the same `shortcuts.json`; old builds simply ignore the stray
`[Shortcuts]` conf group. The upgrade is reversible with no data loss. Optionally,
`applyPreset()` writes `shortcuts.json.bak` before overwriting (guarding the one
genuinely destructive action).

### Migration tests

- Upgrade with an untouched old-defaults `shortcuts.json` → `preset=qimgv`,
  `modified=false`, bindings byte-stable.
- Upgrade with a customized `shortcuts.json` → `modified=true`, custom bindings
  preserved.
- Upgrade across a version that introduced a new default action → action
  back-filled from the qimgv preset with `mergeMissing` gone.
- Idempotency: a second boot at the redesign version does not re-run the backfill
  or flip `modified`.

## Presets to Author

| id | display label | os | source scheme | notes |
|----|---------------|----|---------------|-------|
| `qimgv` | `qimgv` | linux, windows, macos | current `default-shortcuts.json` | default + first-run seed |
| `gwenview` | `Gwenview-style` | linux | KDE Gwenview defaults | KDE/Linux app |
| `xnviewmp` | `XnView MP-style` | linux, windows, macos | XnView MP defaults | cross-platform |
| `irfanview` | `IrfanView-style` | windows | IrfanView defaults | Windows-only app |

Display labels use the `-style` convention (see **Naming & Attribution**); the
`id` is the short slug used in `[Shortcuts]/preset` and the resource filename.

Only actions thumbgrid actually has can be mapped; unmatched viewer shortcuts are
dropped, and thumbgrid actions with no counterpart fall back to the qimgv binding
(or are left unbound). Author these from each viewer's published shortcut list;
treat exact fidelity as best-effort, noted in each file's `_meta.source`.

## Component Change List

1. **New resources**: `src/res/keymaps/keymap_{linux,windows,macos,fallback}.json`;
   `src/res/presets/{qimgv,gwenview,xnviewmp,irfanview}.json` (each with a
   self-describing `_meta` block, incl. per-preset `notice`) +
   `src/res/presets/NOTICE.md` (trademark disclaimer, see **Naming &
   Attribution**). No central manifest. Register all in `src/resources.qrc`;
   remove `res/default-shortcuts.json` (content moves to `presets/qimgv.json`).
2. **`src/shortcutpresetstore.{h,cpp}`** (new): the preset catalog —
   `available()` (scan dirs, read `_meta`, OS-filter, sort by `order`), `meta()`,
   `resolvePath()` (system `PRESETS_PATH` → qrc), `load()` (materialized,
   tokens + `platform` block resolved), `currentOsToken()`. Mirrors `ThemeStore`.
3. **`src/utils/inputmap.{h,cpp}`**: JSON-load keys + modifiers; `#ifdef`-select
   the keymap resource; replace the hardcoded `keyName*` glyphs with data reads.
4. **`src/components/actionmanager/actionmanager.{h,cpp}`**:
   - Delegate to `ShortcutPresetStore` instead of `resolveDefaultShortcutsPath()`.
   - `initDefaults()` loads the **selected** preset (via the store) into `defaults`.
   - `initShortcuts()`: first-run materialize from the selected preset; save.
   - New public: `QList<PresetInfo> availablePresets()`, `QString selectedPreset()`,
     `void applyPreset(const QString &id)` — thin wrappers over the store + settings.
   - Generalize the `platform` sub-block selection in `insertPlatformShortcutObject`
     / the store loader from `apple`/`default` to `windows`/`macos`/`linux`/`default`
     (current-OS block, `default` fallback) so a single preset file can carry
     per-OS binding differences.
   - Remove the three `mergeMissing()` calls (after the version audit).
5. **`src/settings.{h,cpp}`**: add `{"preset", "Shortcuts"}` and
   `{"modified", "Shortcuts"}` to the `settingGroupFor` map, then
   `selectedPreset()` / `setSelectedPreset()` and
   `shortcutsModified()` / `setShortcutsModified()` backed by `[Shortcuts]`; a
   `shortcutsJsonExists()` helper for the first-run check.
6. **`src/gui/dialogs/settingsdialog.{h,cpp,ui}`**: a "Shortcut preset" combobox on
   the Controls page (parallel to the theme selector), populated from
   `availablePresets()` (already OS-filtered) → confirm dialog → `applyPreset()` →
   refresh the shortcut table. Reflect `modified` in the label (e.g.
   "qimgv (modified)"); annotate a stored preset that isn't in the current OS's
   list ("… (other platform)"). Deeper editor UX is untouched.
7. **`src/CMakeLists.txt`**: drop the `default-shortcuts.json` install and the
   `SHORTCUTS_PATH` define. Add a `PRESETS_PATH` define + install the presets dir
   to `${SYSCONFDIR}/thumbgrid/presets` (admin-editable, mirroring `THEMES_PATH`).
   Embed presets via a **generated** `qt_add_resources` file list selected by the
   `THUMBGRID_PRESETS` cache var (see **Build-time preset selection**), not the
   static `resources.qrc`; `qimgv` is always embedded. Keymaps stay qrc-only.
8. **Packaging**: `/etc/thumbgrid/default-shortcuts.json` is no longer used; stale
   copies are harmless. Admin/site default customizations move to a preset file
   under `${SYSCONFDIR}/thumbgrid/presets/` (see Migration).

## Risks

- **Canonical key-name stability** — keymap values and preset keys must match the
  strings already in users' `shortcuts.json` (`C`, `F12`, `PgUp`, `Enter`,
  `$Ctrl+C`). Deriving `qimgv.json` from the current defaults and copying scan-code
  values verbatim keeps them stable. Add a test asserting representative names.
- **Applying a preset destroys customizations** — it overwrites `shortcuts.json`.
  Must be confirm-gated; consider a one-time backup of the prior file.
- **Removing `mergeMissing` could drop a new-action default** if that action's
  introduced-version is missing from `appActions`. Audit those versions before
  deleting; cover with an upgrade test.
- **Existing installs** already have `shortcuts.json` — untouched; only the
  *source of defaults* changes. No user migration needed.
- **Windows Qt-version scan-code drift** — keep both the pre-6.7 and 6.7 alternate
  entries in `keymap_windows.json`.
- **Preset fidelity** — matching another viewer exactly is impossible where
  actions don't line up; document per-file and set expectations in the UI copy.

## Implementation Phases

1. **Keymap extraction** (self-contained, no behavior change): add keymap
   resources, rewrite `InputMap` to load them, move modifier glyphs to data.
   Verify dispatch unchanged. This is the old `inputmap-resource-redesign-plan.md`.
2. **Preset store + plumbing**: rename `default-shortcuts.json` →
   `presets/qimgv.json` with a `_meta` block; add `ShortcutPresetStore`; add the
   `[Shortcuts]` settings; repoint `initDefaults()` at the selected preset via the
   store; first-run materialize + save; remove `SHORTCUTS_PATH` runtime use and the
   old system install. Verify reset/upgrade paths.
3. **Remove per-action C++ churn**: audit `appActions` versions; delete
   `mergeMissing`.
4. **Preset selection UI**: combobox + confirm + table refresh.
5. **Author the additional presets** (gwenview / xnviewmp / irfanview).

## Test Plan

- **Keymap loading**: on Linux the loaded map contains scancode `54 → "C"` and
  representative nav keys (`Left`/`Right`/`PgUp`/`PgDown`); modifiers resolve to
  `Ctrl`/`Alt`/`Shift`.
- **First run**: with no `shortcuts.json`, boot materializes it from qimgv;
  assert a representative binding (`Right → nextImage`) and that the file now
  exists.
- **Apply preset**: `applyPreset("gwenview")` rewrites `shortcuts.json`, updates
  `selectedPreset`, and dispatch reflects the new scheme.
- **Reset to defaults**: after customizing, reset restores the selected preset.
- **Upgrade migration**: simulate an older config version missing a newer action;
  `adjustFromVersion()` re-adds it from the preset without `mergeMissing`.
- **Regression**: existing behavior tests pass —
  `The same key runs different actions per view`,
  `Shortcut contexts persist and legacy configs migrate`.
- **Commands**:
  - `cmake --build build --target thumbgrid --parallel`
  - `ctest --test-dir build -R "(same key runs different actions|Shortcut contexts persist)" --output-on-failure`
  - `ctest --test-dir build --output-on-failure`
  - `git diff --check`

## Open Questions

- **Modifier glyphs**: move into `keymap_*.json` (recommended, matches the goal)
  vs. keep the tiny `keyNameCtrl/Alt/Shift` helper.
- **Reset semantics**: reset to the *selected* preset (recommended) vs. always
  qimgv.
- **Backup on preset apply**: write `shortcuts.json.bak` before overwrite? Cheap
  safety for a destructive action.
- **Preset naming**: ship the default as `qimgv` (per this plan) vs. a
  `thumbgrid` id that happens to equal the qimgv scheme.
- **Foreign-OS presets in the selector**: hide them entirely vs. reveal behind a
  "show other platforms" toggle (this plan assumes hidden by default, revealable).
- **Per-OS first-run default**: keep `qimgv` as the default seed on every OS
  (per this plan) vs. pick a platform-native default (e.g. an IrfanView-style
  default on Windows). Kept as qimgv for now since thumbgrid is its fork.
- **Default embedded preset set**: embed only the target platform's presets
  (per this plan, smaller binary, packager-friendly) vs. embed all presets
  everywhere (max config portability across machines). `THUMBGRID_PRESETS` lets a
  builder choose either way; the question is only the shipped default.
- **CMake floor for `_meta.os` inference**: the recommended design infers the
  embed set from `_meta.os` via `string(JSON)`, which needs CMake ≥3.19 (current
  floor 3.13; Qt6 already needs ≥3.16). Confirm bumping the floor is acceptable;
  the only reason not to is toolchains stuck below 3.19, which take the explicit-
  list fallback.

## Out of Scope

- No change to the user `shortcuts.json` schema or the in-memory `key→action`
  representation.
- No change to shortcut-context precedence, the `_primary`/`_disabled` metadata,
  or the existing shortcut editor beyond adding the preset selector.
- No macOS native scan-code table.
