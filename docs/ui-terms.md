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
| core.cpp:360 | "Welcome to " + applicationName() + " version " … "!" | "Welcome to ThumbGrid %1!" |
| core.cpp:367, 370 | "Shuffle mode: OFF" / "Shuffle mode: ON" | "Shuffle mode: off" / "Shuffle mode: on" |
| core.cpp:379, 385 | "Slideshow: OFF" / "Slideshow: ON" | "Slideshow: off" / "Slideshow: on" |
| core.cpp:395 | "Directory is empty." | "Folder is empty" |
| core.cpp:1128 | "Move to..." (folder dialog title) | "Move to" |
| core.cpp:1442, 1443 | "New Folder" (title and default name) | "New folder" |
| core.cpp:1450 | "Folder name cannot contain path separators." | "Folder name cannot contain path separators" |
| core.cpp:1536 | "Cannot view root folder." | "Cannot view root folder" |
| core.cpp:1635 | "Already at root folder." | "Already at root folder" |
| core.cpp:1805 | "End of directory." | "End of folder" |
| core.cpp:1894 | "Error: could not load image." | "Could not load image" |
| components/fileoperationscontroller.cpp:282, 403 | "Could not create directory " + dir | "Could not create folder %1" |
| components/fileoperationscontroller.cpp:470 | "File moved." / "File copied." | "File moved" / "File copied" |
| components/fileoperationscontroller.cpp:502-503 | "Move " + n + " items to trash?" / "Move item to trash?" | "Move %n item(s) to trash?" |
| components/fileoperationscontroller.cpp:505-506 | "Delete " + n + " items permanently?" / "Delete item permanently?" | "Delete %n item(s) permanently?" |
| components/fileoperationscontroller.cpp:524 | "File removed" | "Deleted" |
| components/fileoperationscontroller.cpp:529 | "Moved to trash: " + n + " files" | "Moved %n file(s) to trash" |
| components/fileoperationscontroller.cpp:531 | "Removed: " + n + " files" | "Deleted %n file(s)" |
| components/fileoperationscontroller.cpp:662 | "Resizing %1 images..." | "Resizing %1 images…" |
| utils/fileoperations.cpp:85 | "Operation completed succesfully." | "Done" |
| utils/fileoperations.cpp:87-103, 109 | "Destination file exists." … "Other error." (trailing periods) | same text without the period |
| utils/fileoperations.cpp:89 | "Destination directory exists." | "Destination folder exists" |
| utils/fileoperations.cpp:99 | "Directory is not empty." | "Folder is not empty" |
| utils/fileoperations.cpp:101 | "Containing directory is not writable." | "Containing folder is not writable" |
| main.cpp:123 | "File or directory path." | "File or folder path." |
| main.cpp:126, 127 | "Generate all thumbnails for directory." / "directory-path" | "Generate all thumbnails for a folder." / "folder-path" |
| gui/mainwindow.cpp:20 | applicationName() + " " + version (window title) | "ThumbGrid " + version |
| gui/mainwindow.cpp:231, 241, 251, 261 | "Zoom temporary disabled" | "Zoom temporarily disabled" |
| gui/mainwindow.cpp:396-401 | "Sorting: By Name" … "Sorting: By File Size (desc.)" | "Sorting: " + the Sort modes table |
| gui/mainwindow.cpp:416, 418, 425, 427 | "Zoom lock: ON/OFF", "View lock: ON/OFF" | "Zoom lock: on/off", "View lock: on/off" |
| gui/mainwindow.cpp:511 | "Filter " + filterName | "Filter: %1" (matches 486, 491) |
| gui/mainwindow.cpp:690 | "Save File as..." | "Save as" |
| gui/mainwindow.cpp:997 | "Folder view" (window title) | "Grid" |
| gui/contextmenu.cpp:62 | "Quick copy" | "Copy to…" |
| gui/contextmenu.cpp:66 | "Quick move" | "Move to…" |
| gui/contextmenu.cpp:80 | "Folder View" | "Grid" |
| gui/contextmenu.cpp:88 | "Open with..." | "Open with" |
| gui/contextmenu.cpp:107 | "Find duplicates..." | "Find duplicates…" |
| gui/contextmenu.cpp:233 | "Resize..." (tooltip) | "Resize…" |
| gui/folderview/gridcontextmenu.cpp:30 | "Convert to..." | "Convert to" |
| gui/folderview/gridcontextmenu.cpp:37, 42 | "Resize..." / "Rename..." | "Resize…" / "Rename…" |
| gui/folderview/gridcontextmenu.cpp:57 | "Move to..." | "Move to…" |
| gui/folderview/gridcontextmenu.cpp:67 | "Find duplicates..." | "Find duplicates…" |
| gui/folderview/folderview.ui:281-306 | "A - Z", "Z - A", "Size", "Size (desc)", "Oldest", "Newest" | Sort modes table, in enum order |
| gui/folderview/folderview.cpp:350 | "Select directory" | "Select folder" |
| gui/customwidgets/pathselectormenuitem.cpp:14 | "Select directory" | "Select folder" |
| gui/folderview/fvoptionspopup.cpp:15 | "Extended + Folders" | "Extended + folders" |
| gui/overlays/copyoverlay.cpp:14, 49 / 52 | "Copy to..." / "Move to..." (header) | "Copy to" / "Move to" |
| gui/overlays/moveprogressoverlay.cpp:57, 70 | "Moving..." / "Cancelling after the current file..." | "Moving…" / "Cancelling after the current file…" |
| gui/dialogs/filereplacedialog.cpp:44-45 | "Directory already exists" / "Merge directories?" | "Folder already exists" / "Merge folders?" |
| gui/dialogs/filereplacedialog.ui:118, 125 | "Yes" / "No" | "Replace" (folder mode: "Merge") / "Skip" |
| gui/dialogs/fileinfodialog.cpp:137 | "EXIF" (tab) | "Exif" |
| gui/dialogs/duplicatefinderdialog.cpp:78, 188 | "Add..." / "Browse..." | "Add…" / "Browse…" |
| gui/dialogs/duplicatefinderdialog.cpp:98, 534 | "Find Duplicates" | "Find duplicates" |
| gui/dialogs/duplicatefinderdialog.cpp:278 | "name or path..." (placeholder) | "Name or path" |
| gui/dialogs/duplicatefinderdialog.cpp:393 | "Move selected..." | "Move selected…" |
| gui/dialogs/duplicatefinderdialog.cpp:396 | "Delete selected..." (it trashes) | "Move selected to trash" |
| gui/dialogs/duplicatefinderdialog.cpp:541 | "Scanning..." | "Scanning…" |
| gui/dialogs/duplicatefinderdialog.cpp:635 | "Could not trash %n file(s):" | "Could not move %n file(s) to trash:" |
| gui/dialogs/duplicatefinderdialog.cpp:654 | "Move duplicates to..." (title) | "Move duplicates to" |
| gui/dialogs/settingsdialog.cpp:146 | "Preferences — " + applicationName() | "Settings — ThumbGrid" |
| gui/dialogs/settingsdialog.cpp:180 | "<b>thumbgrid %1</b>…" | "<b>ThumbGrid %1</b>…" |
| gui/dialogs/settingsdialog.cpp:259 | "Nearest neighbor" | "Nearest neighbour" |
| gui/dialogs/settingsdialog.cpp:275 | "Checker colors:" | "Checkerboard colours:" |
| gui/dialogs/settingsdialog.cpp:294, 296 | "Checker color 1" / "Checker color 2" | "Checkerboard colour 1" / "Checkerboard colour 2" |
| gui/dialogs/settingsdialog.cpp:1871 | "Edit keys..." | "Edit keys…" |
| gui/dialogs/settingsdialog.ui:47 | "Preferences" | "Settings" |
| gui/dialogs/settingsdialog.ui:483, 490, 511 | "Start in folder view by default" / "Show folder view top bar" / "Folder view filename font size:" | "Start in grid view by default" / "Show grid top bar" / "Grid filename font size:" (as the .cpp already sets) |
| gui/dialogs/settingsdialog.ui:1281, 2371 | "Center selected image" / "Center" | "Centre selected image" / "Centre" |
| gui/dialogs/settingsdialog.ui:1717-1742 | "A - Z" … "Newest" | Sort modes table, in enum order |
| gui/dialogs/settingsdialog.ui:1789 | "…in the folder view…" | "…in the grid…" |
| gui/dialogs/settingsdialog.ui:2426 | "Grid background on images with transparency" | "Checkerboard behind transparent images" |
| gui/dialogs/settingsdialog.ui:2936 | "Nearest neighbor" | "Nearest neighbour" |
| gui/dialogs/settingsdialog.ui:3173, 3183 | "Dark Blue" / "Light Blue" | "Dark blue" / "Light blue" |
| gui/dialogs/settingsdialog.ui:4451 | "Touchpad &amp; Mouse Wheel" | "Touchpad &amp; mouse wheel" |
| gui/dialogs/settingsdialog.ui:5396 | "Mpv binary:" | "mpv binary:" |
| gui/dialogs/settingsdialog.ui:5437 | "..." (mpv browse button) | "…" |
| gui/dialogs/settingsdialog.ui:5537 | "About thumbgrid" | "About ThumbGrid" |
| gui/dialogs/shortcutcreatordialog.cpp:13 | "Add shortcut" | "New shortcut" (matches the .ui and the "+ New shortcut" button) |
| gui/dialogs/scripteditordialog.cpp:13 / .ui:14 | "New application/script" / "Add Script" | "New script" |
| gui/dialogs/scripteditordialog.cpp:34 | "Edit" | "Edit script" |
| gui/dialogs/resizedialog.ui:63, 79 | "By Percent:" / "By Absolute Size:" | "By percent:" / "By absolute size:" |
| gui/dialogs/resizedialog.cpp:26, 157 | "By Long Edge:" / "Fit Within:" | "By long edge:" / "Fit within:" |
| gui/dialogs/printdialog.ui:348, 374, 384 | "Color mode:" / "Grayscale" / "Color" | "Colour mode:" / "Greyscale" / "Colour" |
| gui/panels/croppanel/croppanel.ui:334, 358, 363 | "Aspect Ratio" / "Current Image" / "This Screen" | "Aspect ratio" / "Current image" / "This screen" |
