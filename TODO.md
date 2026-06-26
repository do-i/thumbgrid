# TODO

Action items for maintaining this hard fork of [easymodo/qimgv](https://github.com/easymodo/qimgv).

## Consequence message Move file/folder 
Show a popup dialogue to confirm or cancel move /copy file/folder by drag and drop

## Add commit hash (7 chars) in About screen.
This helps for dev

## bug: Delete does not work
when I delete a selected file, confirmation popup shows up, I confirm, file is not deleted.
It could be file / dir permission issue? I don't know. I set containing dir permission to 555. and delete a file. file disappears from the folder. But, go to other dir and come back, file re-appears.

Suggested solution: check file permission and containing dir permission. if not possible to delete, popup should display message and only show cancel button.
