# TODO

Mark as [COMPLETED] when a listed item is fully implemented.

## Major Re-design

clean up shortcuts.json: some shortcut actions are in both contexts but others are context specific. design consideration: would it be cleaner by adding the 3rd context, 'Global'? if same key is defined in both, honor context specific key? It can be tricky. but if this is done right, configuration can remove lots of unnecessary duplicates.


## Grid view Right click menu

Currently there is only 'Convert to' menu item when right click in grid view.

* Add more toggle header title bar, left side panel, bottom status bar.
* Reuse the same menu component style as the menu used in document view.
* Use icon, short text description, primary key shortcut.
