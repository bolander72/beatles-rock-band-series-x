# Native account integration: development checks

Version 0.6.0.11 is development work, not a published or console-verified release. It associates a console account with a persistent, offline Xbox 360 save profile. It does not authenticate to Xbox 360 Xbox Live or import cloud saves.

## What changes

The app reads signed-in console users through `Windows.System.User` and uses Xbox's own account picker when a selection is needed. An account's opaque, app-specific ID selects its local save identity. Display-name changes do not create a new save. The local profile label may differ from the console gamertag.

The manifest adds `userAccountInformation` and changes the app to support multiple users. Account IDs and the local association file stay in app storage. The integration does not add microphone capture or an online account service.

Before the emulator opens its configuration, the app stages existing per-user state into shared app storage. The original files remain in place. An existing single local profile is associated with the console account owning its previous app-data folder. Several prior save sets or several unassociated local profiles require explicit migration; the app stops rather than guessing an owner. A completed migration is not repeated over newer shared progress.

## Before installing a test build

1. Save a private backup of the current app's LocalState. Include profiles, content, configuration and cache. Do not publish the backup or diagnostic logs.
2. Verify the package's manifest, block hashes, matching source and imports against the installed Microsoft VCLibs runtime.
3. Review the added account-information permission with the person using the console before installing this update.
4. Leave **Treat UWP apps as games by default** enabled. Keep the current developer-storage allocation.

## Console verification

1. With one console account signed in, open the **The Beatles: Rock Band** tile. The configured game should start without an emulator profile wizard or Play dashboard. If account selection is needed, it should be Xbox's native picker.
2. Confirm that existing story progress is present. Complete a save, fully quit the app, reopen the tile and check that the new progress remains.
3. Repeat with the other console account. Check that each account reopens its own local profile and progress. Do not use a matching display name as evidence of ownership; inspect the private association and save identity if needed.
4. Exercise an in-game sign-in request and cancellation. Confirm the native picker opens, cancel returns safely, and selecting an account changes only the intended local player.
5. Play a song and check frets, strum, whammy, ordinary tilt, audio and prompt Menu/Start skipping. Repeat a full quit-and-reopen launch.

Native account association does not implement independent second-guitar routing or USB microphone capture. Those require separate instrument work and physical tests.

## Failure handling

If migration, account selection or save association fails, retain both old and new storage plus the private logs. Do not uninstall, resize developer storage, erase a binding file or substitute another user's ID as a repair. The existing 0.6.0.10 package and pre-update backup are available for a deliberate recovery after the failure has been inspected.

Automated profile, storage and migration tests support these checks; they do not establish console gameplay or story-save success.
