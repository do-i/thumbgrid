# TODO

Mark as [COMPLETED] when a listed item is fully implemented.

## Preference (simple and urgent)

Change icons for Grid and Document in Preferences screen to res/icons/common/memuitem/folderview16 and document-view16.

## Preference > Shortcuts (important)

1. Add 'Count' column to the Preference > Shortcuts > table > b/w Key and Enabled cols. show keyboard shortcuts count in each cell.
2. Clicking on a key cell popup a modal. In that modal user could 'Add' or 'Delete' key(s). also, add indicator of system default keys.
3. By default, select user override key as primary.
4. newly added key automatically be primary unless user explicitly select other key as primary.

## Major Re-design (low priority)

clean up shortcuts.json: some shortcut actions are in both contexts but others are context specific. design consideration: would it be cleaner by adding the 3rd context, 'Global'? if same key is defined in both, honor context specific key? It can be tricky. but if this is done right, configuration can remove lots of unnecessary duplicates.


## Grid view Right click menu

Currently there is only 'Convert to' menu item when right click in grid view.

* Add more toggle header title bar, left side panel, bottom status bar.
* Reuse the same menu component style as the menu used in document view.
* Use icon, short text description, primary key shortcut.
