# Windows startup

The Auto start switch saves the selected connection and enables a Windows scheduled task. MintVPN restores that connection when it starts with the same profile.

For installations with an existing startup coordinator, create `startup-task.txt` in the active profile directory. Put the exact name of the existing Windows scheduled task on a single UTF-8 line. The switch will enable or disable that task instead of creating a separate MintVPN logon task. MintVPN will leave the coordinator's triggers and actions unchanged. The current user must have permission to query and change the task.

The coordinator is responsible for waiting for any required network service, starting MintVPN with the correct `-appdata` directory, checking the connection, and then starting dependent applications. Remove any old standalone MintVPN logon trigger when installing a coordinator to avoid competing startup paths. This file and the coordinator are local configuration and are not included in published releases.

If the coordinator task is missing or cannot be enabled, the switch reports failure. Remove `startup-task.txt` to return to the built-in startup task after removing the coordinator's MintVPN action. Keep a backup of the profile before changing startup configuration.
