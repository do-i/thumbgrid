# Popup / Dialog Polish Action Plan

Three visual defects reported 2026-07-17. Each item below carries its own
**Execution model**: which AI model should run the item (with rationale), the
concrete edit steps, the files touched, and how the result gets verified
before it is checked off.

Model tiers referenced below: Haiku 4.5 (`claude-haiku-4-5-20251001`) for
mechanical edits, Sonnet 5 (`claude-sonnet-5`) for standard coding, Opus 4.8
(`claude-opus-4-8`) for design/judgment-heavy iteration.

Verification for all items uses the click-driven visual flow from memory
(`import` screenshots + the XTest helper), since all three are appearance bugs
that unit tests can't see.

---

## P1. Settings popups' buttons ≠ color picker buttons

**Symptom:** the frameless popups opened from the Settings window (version
info, preset-switch confirm — all `CustomMessageBox`) have buttons styled
differently from the buttons in the embedded color picker dialog
(`QDialog#ColorPickerDialog`, opened from Settings → theme color swatches).

**Diagnosis** (`src/res/styles/style-template.qss`):

| property        | CustomMessageBox (`:1208`)      | ColorPickerDialog (`:1041`) |
|-----------------|---------------------------------|------------------------------|
| padding         | `6px 14px`                      | `5px 12px`                   |
| min-width       | `70px`                          | `60px`                       |
| `:default`/`:focus` accent | yes (`:1236`)        | **missing**                  |
| cursor          | pointing hand (set in C++)      | default arrow                |

The color picker's OK/Apply/Cancel come from a `QDialogButtonBox`
(`src/gui/customwidgets/colorselectorbutton.cpp:46`), which never gets the
pointing-hand cursor that `CustomMessageBox::addButton()` sets
(`src/gui/dialogs/custommessagebox.cpp:72`).

**Fix:** make ColorPickerDialog buttons identical to the CustomMessageBox ones
(that is the app-wide dialog button style; the color picker is the outlier).

**Execution model**

*Model:* **Haiku 4.5** — pure selector regrouping in QSS plus one mechanical
C++ loop; the diagnosis above already pins every line, no judgment left.
Escalate to Sonnet 5 only if the screenshot check reveals a further mismatch
not covered by the table.

1. In `style-template.qss`, delete the bespoke
   `QDialog#ColorPickerDialog QPushButton` / `:hover` / `:pressed` rules
   (lines ~1041–1053) and instead append `QDialog#ColorPickerDialog QPushButton`
   to the shared selector groups at lines ~1202, ~1216, ~1226, and add
   `QDialog#ColorPickerDialog QPushButton:default` to the accent group at ~1236.
2. In `colorselectorbutton.cpp`, after creating the `QDialogButtonBox`, set
   `Qt::PointingHandCursor` on each of its buttons
   (`for (auto *b : buttons->buttons()) b->setCursor(...)`).
3. Build (`touch` edited header-adjacent sources first per the
   background-build-race note, then full rebuild).
4. Verify visually: open Settings → trigger the version popup, screenshot;
   open a theme color swatch → screenshot the picker; confirm both button rows
   match (size, padding, accent on the default button, hand cursor).

---

## P2. "Convert to..." popup page — oversized gaps between items

**Symptom:** after pressing "Convert to..." in the grid context menu, the
format page (Back / JPEG / PNG / WebP) shows its 4 items spread far apart.

**Diagnosis** (`src/gui/folderview/gridcontextmenu.cpp`): both pages live in
one `QStackedWidget` (`:20`). The stack is sized once at construction
(`adjustSize()`, `:101`) to fit the **main** page (11 rows + separators). When
the 4-row convert page becomes current, its `QVBoxLayout` (`spacing(2)`, `:87`)
has to fill the same tall area, so the surplus height is distributed between
the items — the "big margins".

**Fix:** resize the popup to the current page instead of letting the short
page stretch.

**Execution model**

*Model:* **Sonnet 5** — QStackedWidget/size-policy resize behavior is easy to
get subtly wrong (popup re-clamping while visible, sizeHint invalidation
order), so this needs a model that can reason about Qt layout mechanics and
iterate against screenshots, but it's still a contained single-file change.

1. In `gridcontextmenu.cpp`, add a helper used by both `switchToMainPage()`
   and `switchToConvertPage()`:
   - set the size policy of every non-current stack page to
     `QSizePolicy::Ignored/Ignored` and the current page to
     `Preferred/Preferred`,
   - then `stack->adjustSize(); adjustSize();` so the popup shrinks/grows to
     the visible page.
2. Keep the popup on-screen after the resize: re-run `clampToScreen()` with
   the current geometry when the switch happens while visible.
3. Safety net: `convertLayout->addStretch(1)` so any residual surplus goes
   below the items, not between them.
4. Build; verify visually: right-click a thumbnail → screenshot main page →
   click "Convert to..." → screenshot; item pitch on the convert page must
   equal the main page's (2 px spacing), and the popup must shrink to ~4 rows.
5. Regression check: switch back ("Back") and confirm the main page renders
   full-height again, and the popup still clamps correctly near screen edges.

---

## P3. "Convert to..." uses the crop icon

**Symptom:** the "Convert to..." menu item (and every format row on the
convert page) shows `image-crop16.png` — the same icon as the viewer's Crop
action (`src/gui/contextmenu.cpp:50`).

**Diagnosis:** placeholder icon; see `gridcontextmenu.cpp:29` (menu entry) and
`:113` (`addConvertFormat()` rows). No convert-ish icon exists in
`src/res/icons/common/menuitem/` (checked: back, copy, folder, move, print,
resize, rotate-*, flip-*, trash, settings, …), so per the ask we generate one.

**Icon constraints:** `IconWidget` recolors the pixmap with the theme's icon
color at runtime (`ImageLib::recolor`, `src/gui/customwidgets/iconwidget.cpp:94`)
— only the **alpha shape** matters. Deliverables: `convert16.png` (16×16) and
`convert16@2x.png` (32×32), monochrome-on-transparent, visual weight matching
the existing 16px set.

**Fix:**
- Generate a "convert" glyph: two small rounded-rect "file" frames with a
  right-pointing arrow between them (format A → format B), or a circular
  two-arrow recycle motif if the frames get muddy at 16 px.
- Use it for the "Convert to..." entry.
- Format rows (`JPEG`/`PNG`/`WebP`): reuse the same convert icon.

**Execution model**

*Model:* **Opus 4.8** — the hard part is design judgment: drawing a glyph that
reads as "convert" at 16 px, matching the existing set's stroke weight, and
iterating on screenshots until it's legible. The qrc/code wiring around it is
trivial, but the visual iteration loop drives the model choice.

1. Generate the glyph programmatically (Python/Pillow or QPainter one-off
   script in `$CLAUDE_JOB_DIR/tmp`) at 32×32, downscale to 16×16; solid
   white shape + alpha, no color (runtime tint handles color).
2. Save as `src/res/icons/common/menuitem/convert16.png` and
   `convert16@2x.png`; register both in `src/resources.qrc` next to the
   `image-crop16` entries (~line 59).
3. Point `gridcontextmenu.cpp:29` and `:113` at
   `:/res/icons/common/menuitem/convert16.png`.
4. Build; verify visually at 100% DPI: screenshot the context menu, confirm
   the new glyph reads as "convert" at 16 px, is distinct from crop, and its
   stroke weight matches neighbors (move/trash/settings). Iterate on the
   glyph if it's muddy — legibility at 16 px is the acceptance bar.
5. Confirm the viewer context menu's Crop item still shows the crop icon.

---

## Suggested order

P2 → P3 → P1. P2 and P3 touch the same file (`gridcontextmenu.cpp`) and are
verified from the same screenshots; P1 is independent QSS + one C++ line.

## Status

- [ ] P1 — button parity: settings popups vs color picker (Haiku 4.5)
- [ ] P2 — convert page item spacing (Sonnet 5)
- [ ] P3 — dedicated convert icon (Opus 4.8)
