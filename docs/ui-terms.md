# UI terms and style
Reference for every user-visible string (menus, buttons, labels, titles, tooltips, on-screen messages, CLI help).

## Terms

| Use | Never | Notes |
|---|---|---|
| folder | directory, dir | Also in errors and titles ("Select folder"). Code identifiers unchanged. |
| Move to trash | Trash, Remove, Delete | Action, menu item and confirmation title. Question: "Move %n item(s) to trash?" |
| Delete permanently | Delete, Remove | Action and confirmation title. Question: "Delete %n item(s) permanently?" |
| Moved to trash / Deleted | File removed, Removed: | Results. Plural: "Moved %n file(s) to trash", "Deleted %n file(s)". |
| Remove | — | Only for list entries (bookmarks, folder lists, keys) and metadata, never for files. |
| Move to… / Copy to… | Quick move, Quick copy, Move to... | `moveFile` / `copyFile` in every menu (both ask for a destination). |
| Grid | Folder view, folder view, thumbnail view | Name of the view; "the grid" in sentences. The viewer context stays "Document". |
| Settings | Preferences | Window title, menu items, prose. |
| Exif, XMP, ICC | EXIF, exif | "Exif" is the standard's own casing and the dominant one here (8 vs 1). |
| ThumbGrid | thumbgrid, Thumbgrid, qimgv | In prose and titles; `applicationName()` stays "thumbgrid" (paths), so use a literal. |
| mpv | Mpv, MPV | Lower case even at the start of a label. |
| checkerboard | grid (for transparency) | The pattern behind transparent images; avoids a clash with the view name. |

## Sort modes

One set for the grid combo (`folderview.ui`), the settings combo (`settingsdialog.ui`) and the on-screen message `"Sorting: <name>"` (`mainwindow.cpp`). Dash is an en dash (U+2013).

| Mode | Name |
|---|---|
| SORT_NAME / SORT_NAME_DESC | Name A–Z / Name Z–A |
| SORT_SIZE / SORT_SIZE_DESC | Smallest first / Largest first |
| SORT_TIME / SORT_TIME_DESC | Oldest first / Newest first |

## Style

| Rule | Example |
|---|---|
| Sentence case for titles, buttons, menu items, labels, tooltips, combo items | "Find duplicates", "Aspect ratio", "Dark blue" |
| "…" (one character U+2026) only on a menu item or button that opens a dialog needing more input, and on in-progress text | "Resize…", "Browse…", "Moving…"; not "Open with" (submenu), not titles |
| Titles and headers never end in "…" or "?" | "Move to", "Save as" |
| British spelling (found dominant: colour ×12 lines incl. "cancelled", vs color/center/gray/neighbor ×10) | colour, centre, grey, neighbour, cancelled |
| Buttons are verbs naming the outcome, never Yes/No | "Replace", "Skip", "Merge", "Cancel"; OK only in settings-style dialogs |
| On-screen messages: sentence case, no trailing period; toggles read "X: on" / "X: off" | "Slideshow: on", "Folder is empty" |
| One action, one label, in every menu, tooltip and title | `moveFile` = "Move to…" everywhere |
| Dialog sentences keep their punctuation; do not glue fragments (`tr("a ") + n + tr(" b")`) | `tr("Deleted %n file(s)", nullptr, n)` |

Not violations: all-caps panel headers ("BOOKMARKS"). Out of scope: raw action ids on the Shortcuts page (`removeFile`) need a display-name map.

## Strings to change

Worklist for T-032, T-039, T-042 (line numbers as of 419823f4; locate by text). Delete rows as they land.

| File:line | Current | New |
|---|---|---|
| gui/dialogs/filereplacedialog.cpp:44-45 | "Directory already exists" / "Merge directories?" | "Folder already exists" / "Merge folders?" |
| gui/dialogs/filereplacedialog.ui:118, 125 | "Yes" / "No" | "Replace" (folder mode: "Merge") / "Skip" |
| gui/dialogs/duplicatefinderdialog.cpp:98, 534 | "Find Duplicates" | "Find duplicates" |
| gui/dialogs/duplicatefinderdialog.cpp:393 | "Move selected..." | "Move selected…" |
| gui/dialogs/duplicatefinderdialog.cpp:396 | "Delete selected..." (it trashes) | "Move selected to trash" |
| gui/dialogs/shortcutcreatordialog.cpp:13 | "Add shortcut" | "New shortcut" (matches the .ui and the "+ New shortcut" button) |
| gui/dialogs/scripteditordialog.cpp:13 / .ui:14 | "New application/script" / "Add Script" | "New script" |
| gui/dialogs/scripteditordialog.cpp:34 | "Edit" | "Edit script" |
