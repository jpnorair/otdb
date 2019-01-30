# OTDB Features TODO

## Possible Thread Deadlock on server socket

Investigate and fix.

## ~~Fix time issue with load command~~

~~load command is not reporting meta time value that's the same as from save command.~~

## List Devices in DB

Output a list of Device IDs (UID) in the database.  The first and next iterator that's used in cmd_save can be used here.

## Device operations to otter/smut must be addressed

* Cmd_devmgr requires a device ID input parameter
* Cmd_devmgr requires a user parameter (guest, user, root)
* otter/smut might also have an inline command for pushing an embedded command to an addressed device

## Push (upload) Devices in DB to otter/smut

For each device in the DB, or for a subset of device IDs:

1. do mknode, adding VID and keys
2. Write each file as stored in the DB

## Pull (download) Devices from otter/smut into DB

For each device in the DB, or for a subset of device IDs:

1. do mknode, adding VID and keys
2. Write each file as stored in the DB

