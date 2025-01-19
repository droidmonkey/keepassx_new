# Changelog

## 2.8.0 (Pending)
* Placeholder for future release notes

## 2.7.9 (2024-06-19)

### Changes
* Passkeys: Ability to easily remove a passkey from an entry [#10777]
* Snap: Use new desktop portal for native messaging integration [#10906]

### Fixes
* Improve entry placeholder/reference feature [#10846]
* Improve CSV importing when title field isn't specified [#10843]
* Improve encrypted Bitwarden importing [#10800]
* Improve database settings UX [#10821]
* Improve handling of clipboard actions from entry preview [#10810]
* Improve group/entry view resize behavior and set sensible defaults [#10641]
* Passkeys: Fix incorrect username fill [#10874]
* Passkeys: Return additional data to the extension [#10857]
* Fix password clear timer inconsistency on unlock view [#10708]
* Fix portability check [#10760]
* Fix page overflow on HTML exports [#10735]
* Fix broken builds when using system provided zxcvbn [#10717]
* Fix copy password button when text is selected [#10853]
* Fix tab ordering on application settings pages [#10907]
* SSH Agent: Fix broken decrypt button [#10638]
* Windows: Fix ALT Auto-Type modifier [#10795]
* Windows: Fix wrong DACL memory size allocation [#10712]
* macOS: Fix monospace font sizing [#10739]
* Flatpak: Fix configuration settings off-by-one error [#10688]
* BSD: Fix compiling with libusb implementation [#10736]

## 2.7.8 (2024-05-05)

### Changes
- Add hotkey for showing search help [#10591]
- Add hotkey for group switching (Ctrl+Shift+PgUp/PgDown) [#10625]
- Add per-database auto-save delay setting [#9100]
- Add setting to hide menubar [#10341]
- Improve Bitwarden 1PUX import and support organization collections [#10499]
- Show advanced settings checkbox only for settings that have them [#6513]
- Remove obsolete setting for requiring repeated password entry [#9722]
- Passkeys: Allow registering Passkeys to existing entries [#10408]
- Passkeys: Show warning about data being unencrypted before Passkey export [#10411]
- Passkeys: Support NFC and USB transports [#10402]
- Passkeys: Pass extension JSON data to browser [#10615]
- SSH Agent: Do not use entries from recycle bin [#10518]
- Linux: Change hotkey sequence used for {CLEARFIELD} Auto-Type [#10008]
- Windows: Improve DACL memory access protection [#10618]

### Fixes
- Fix crash when deleting history items [#10451]
- Fix crash on screen lock or computer sleep [#10458]
- Fix search field not being focused after unlock [#10459]
- Fix loss of window focus when Auto-Type needs to unlock a database [#10555]
- Fix inconsistent TOTP visibility on unlock [#10009]
- Fix CSV import skipping over single-name groups [#10575]
- Fix key file folder being remembered even if disabled in settings [#10636]
- Fix issues with entry editing and database locking [#10667]
- Fix key file text when provided on command line [#10642]
- Fix issues with hardware key auto detection [#10663]
- Do not override monospace font size [#10282]
- Perform group sort only when group view is in focus [#10202]
- Do not show decimals for attachment sizes in Bytes [#10595]
- Prevent merging of global custom data when merging databases [#10452]
- Fix minor translation issues [#10635]
- Passkeys: Fix StrongBox incompatibility [#10420]
- Passkeys: Set RP ID to effective domain if unset instead of returning an error [#10384]
- Passkeys: Various UI fixes and improvements [#10427, #10608, #10609]
- AppImage: Fix URL opening [#10624]
- Flatpak: Fix application autostart [#10563]
- Linux/macOS: Fix button sizes on modal alert popups [#10500]
- Linux: Fix clipboard clear on Wayland [#10500]
- Windows: Preserve file-hidden attribute [#10343]

## 2.7.7 (2024-03-09)

### Changes
- Support USB Hotplug for Hardware Key interface [#10092]
- Support 1PUX and Bitwarden import [#9815]
- Browser: Add support for PassKeys [#8825, #9987, #10318]
- Build System: Move to vcpkg manifest mode [#10088]

### Fixes
- Fix multiple TOTP issues [#9874]
- Fix focus loss on save when the editor is not visible anymore [#10075]
- Fix visual when removing entry from history [#9947]
- Fix first entry is not selected when a search is performed [#9868]
- Prevent scrollbars on entry drag/drop [#9747]
- Prevent duplicate characters in "Also choose from" field of password generator  [#9803]
- Security: Prevent byte-by-byte and attachment inference side channel attacks [#10266]
- Browser: Fix raising Update Entry messagebox [#9853]
- Browser: Fix bugs when returning credentials [#9136]
- Browser: Fix crash on database open from browser [#9939]
- Browser: Fix support for referenced URL fields [#8788]
- MacOS: Fix crash when changing highlight/accent color [#10348]
- MacOS: Fix TouchID appearing even though lid is closed [#10092]
- Windows: Fix terminating KeePassXC processes with MSI installer [#9822]
- FdoSecrets: Fix database merge crash when enabled [#10136]

## 2.7.6 (2023-08-15)

### Changes
- Significant improvement to visual when drag/drop entries [#9698]
- Automatically prompt for Quick Unlock when showing unlock dialog [#9697]
- Improve colorful lock icon and fix file MIME icon on KDE [#9632]
- Ability to search by entry UUID [#9571]
- Add challenge-response support for NitroKey 3 [#9631]
- Auto-Type: Disable entry level Auto-Type when disabled at group/entry [#9672]
- Browser: Show warning when adding duplicate URL's to entry [#9588][#9635]
- Browser: Improve error message when proxy cannot be found [#9385]

### Fixes
- Fix crash on exit on macOS [#9620]
- Fix crash on search if entry doesn't have a group [#9633]
- Fix several issues with Quick Unlock [#9697]
- Enable save button when not auto-saving non-data changes [#9634]
- Several UI/UX fixes [#9647]
- Move toolbar back to top of window when disabling movement [#9699]
- Browser: Fix closing password generator dialog with X button [#9636]
- Browser: Fix handling of expired credentials [#9595]
- Windows: Prevent white flicker when launching application [#9637]
- Linux: Fix warning message about allow screencapture [#9638]
- FdoSecrets: Fix access confirmation dialog showing even when disabled [#9690]

## 2.7.5 (2023-05-14)

### Changes
- Add menu option to allow screenshots [#8841]
- Add support for Botan 3 [#9388]
- Increase max TOTP step to 24 hours [#9149]
- Improve HTML export layout [#8987]
- Turn search reset off by default [#9153]
- Use QClipboard::clear() instead of setting blank text [#9148]
- Hide group column header choice when not in search [#9171]
- Improve look of KeePassXC logo and icons [#9355]
- Add keyboard shortcuts for app and database settings [#9007]
- Hide rename button from attachments preview panel [#8842]
- Linux: Set SingleMainWindow in .desktop file [#7430]

### Fixes
- Fix crash when search clears while creating new entry [#9230]
- Fix crash when using Windows Hello in a Remote Desktop session [#9006]
- Fix crash in Group Edit after enabling Browser Integration [#8778]
- Fix canceling quick unlock when it is unavailable [#9034]
- Set password input field font correctly [#8732]
- Greatly improve performance when rendering entry view [#9398]
- Fix various accessibility issues [#9138]
- Fix arrows size when expand/collapse a group [#9096]
- Select the clone instead of the original after cloning an entry [#9070]
- Fix bugs with preview widget [#9170]
- Fix status bar update when switching to other DB [#9073]
- Fix database settings spin box bug [#9101]
- Fix Ctrl+Tab shortcut to cycle databases in unlock dialog [#8839]
- Fix TOTP QR code maintaining square ratio [#9027]
- Fix Auto-Type configuration page on custom sequence selection [#8752]
- Fix unexpected behavior of `--lock` when KeePassXC is not running [#8889]
- Make open folder icon exempt from "Apply group icon to entry" [#9205]
- Allow setting default file open directory with env var [#9192]
- SSH Agent: Fix support for AES-256/GCM openssh keys [#8968]
- Browser: Fix Native Messaging script path with BSD OS's [#8835]
- MacOS: Fix text selection for Auto-Type clear field [#9066]
- MacOS: Don't rely on AppleInterfaceStyle for theme switching [#8615]
- Windows: Remove registry detection of desktop shortcut [#9380]

## 2.7.4 (2022-10-29)

### Changes
- Add 2 months expiration preset [#8687]
- CLI: Add Unicode support on Windows [#8618]

### Fixes
- Fix crash on macOS when unlocking database [#8676]
- Fix display of passwords in preview panel [#8633]
- Fix clicking links in entry preview panel [#8644]
- Prevent expired entries search if no results returned [#8643]
- Browser: Revert code causing connection problems [#8665]
- Browser: Fix socket file symbolic link on Linux [#8656]
- Flatpak: Fix launching browser proxy service [#8680]
- SSH Agent: Fix paegent support on Windows [#8619]

## 2.7.3 (2022-10-23)

### Changes
- Enhance Tags Support and Add Saved Searches [#8435, #8607]
- Significant improvements to entry preview panel [#7993]
- Add password strength indicator to all password fields [#7885]
- Limit zxcvbn entropy estimation length to 128 characters [#7748]
- Try full URL path when fetching favicon [#8565]
- Hide usernames in preview panel when hidden in entry view [#8608]
- Enable dark title bar on windows when accent color is not used [#8498]
- Add option to display passwords in color in preview panel [#7097]
- Add XML Export option to GUI [#8524]
- Increase entropy required for a "good" password rating to 75 [#8523]
- Add shortcut to copy password with TOTP appended [#8443]
- Show entry count in status bar [#8435]
- Allow KeePassXC to be built without X11 [#8147]
- Enable use of VivoKey Apex and Dangerous Things FlexSecure tokens [#8332]
- Add setting for number of recent files [#8239]
- Add Ctrl+Tab shortcut to cycle databases in unlock dialog [#8168]
- Replace offensive words in eff_large.wordlist [#7968]
- Auto-Type: PICKCHARS can specify attribute and ignore BEEP [#8118]
- Linux: Add isHardwareKeySupported and refreshHardwareKeys to DBus methods [#8055]
- Add config variable to specify default database file name [#8042]
- Support numeric aware sorting on Windows and macOS [#8363]
- CLI: Add `db-edit` command [#8400]
- CLI: Add option to display all attributes with `show` command [#8256]
- CLI: Show UUID and tags with `show` and `clip` commands [#8241]
- Browser: Move socket into separate directory on Linux [#8030]
- Browser: Add group setting to omit WWW subdomain when matching URLs [#7988]
- FdoSecrets: Ask to unlock the database when creating items [#8022, #8028]
- FdoSecrets: Skip entries in recycle bin when searching [#8021]

### Fixes
- Fix potential deadlock in UI when saving [#8606]
- Fix newlines when copying notes from preview panel [#8542]
- Fix dark mode detection on Linux [#8477]
- Fix crash when deleting items in recycle bin while searching [#8117]
- Fix crash when trying to close database during unlock [#8144]
- Fix tabbing around the interface [#8435, #8520]
- Fix OPVault import when there are multiple OTP fields [#8436]
- Fix various Windows Hello bugs [#8354]
- Fix use of Apple Watch for Quick Unlock [#8311]
- Better handling of "Lock on Minimize" setting [#8202]
- Check for write permission before entering portable mode [#8447]
- Correct regex escape logic to prevent parse errors [#7778]
- Normalize slashes and file case for last used databases [#7864, #7214]
- Link ykcore against pthread [#7807]
- Auto-Type: Fix menu entries in selection dialog on Windows [#7987]
- Auto-Type: Fix use of modifiers under macOS [#8111]
- CLI: Fix output when using clip with the -t flag [#8271]
- Browser: Use asynchronous access confirm dialog [#8273]
- Browser: Always send database locked/unlocked status [#8114]

## 2.7.1 (2022-04-05)

### Changes
- Show when tags are changed in entry history [#7638]
- Improve tags editing and allow spaces in tags [#7708]
- Improve layout of entry preview panel [#7767]
- Incorporate patches to support Flatpak distribution [#7728]
- Add expiration presets for 12 and 24 hours [#7738]

### Fixes
- Fix crash when building history change list [#7638]
- Fix hiding password on database unlock [#7725]
- Fix AES KDF slow transform speed [#7755]
- Auto-Type: Correct timing issue on macOS and Linux that prevented typing [#7588]
- Auto-Type: Fix use of Ctrl/Alt/Shift/Win modifiers on Windows [#7629]
- Auto-Type: Reduce/eliminate delay when searching for entries [#7598]
- Auto-Type: Map ASCII dead keys on Linux for international keyboards [#7614]
- CLI: Fix detection of hardware keys (YubiKey) [#7593]
- CLI: Add missing parameter `-c` to add/edit entries command [#7594]
- Secret Service: Fix crash when multiple prompts are shown [#7786]
- SSH Agent: Fix default agent selection on Windows [#7764]
- Fix database unlock dialog not being the top window on Linux [#7771]
- Fix drag/drop entries between tabs on Wayland [#7628]
- Fix compiling with minizip-ng [#7638]

## 2.7.0 (2022-03-21)

### Major Additions
- Implement KDBX 4.1 [#7114]
- Add direct write save option for cloud storage and GVFS [#6594]
- Prevent screen capture on Windows and macOS [#6030]
- Support quick unlock using Windows Hello [#7384]
- Support quick unlock using Apple Watch [#5526]
- Allow specifying database backup paths [#7035]
- Add tag functionality [#6487][#7436][#7446]
- Add password rating column to entry view [#4797]
- Add group clone action [#6124]
- Show modifications between entry history items [#6789]
- Ability to bulk-delete and purge unused custom icons [#5970]
- Support adding custom passphrase wordlists [#6799]
- Support passphrase wordlists in numbered and PGP-signed formats [#6791]
- Implement support for hardware keys via wireless NFC [#6895]
- SSH Agent: Add support for OpenSSH 8.2 FIDO/U2F keys [#6371]
- CLI: Implement attachment handling [#5538]
- CLI: Add support for okon in offline HIBP checks [#5478]
- CLI: Implement `search` command and remove `locate` [#6805]
- CLI: Add db statistic output to `db-info` command [#7032]
- CLI: Add -i/--include option to `generate` command. [#7112]
- CLI: Add a -n (--notes) option to `add` and `edit` commands [#4646]
- CLI: Add keyfile option to `import` command [#5402]
- CLI: Adding a best option to clip to copy a password of the best match [#4489]
- Browser: Add Microsoft Edge support on Linux [#7100]
- Browser: Support native password generator from the extension [#6529]
- Browser: Add group settings [#4180]
- Browser: Add feature to ignore entries for HTTP-Auth Logins [#5394]
- Browser: Support triggering Auto-Type from browser extension [#6272]
- Browser: Add delete-entry command to API [#6899]
- Browser: Add search 'by-path' url to API [#5535]
- Browser: search for entries by UUID to API [#4763]
- Browser: Support auto-download of favicon on entry addition [#7179]
- Auto-Type: Major improvements to Auto-Type [#5864][#7463][#7435][#7391][#7129][#6400][#6364][#6361][#5283][#7507]
- Auto-Type: Fix typing to virtual machines on Windows [#7366]
- Auto-Type: Re-implement X11 keysym emulation [#7098]
- Auto-Type: Support multiple Xkb layouts [#6247]
- Auto-Type: Abort keystroke if modifiers held on X11 [#6351][#6357]
- Auto-Type: Add TOTP option to entry level Auto-Type menu [#6675]
- FdoSecrets: Major Refactor and Code Consolidation [#5747][#5660][#7043][#6915]
- FdoSecrets: Implement unlock before search [#6943]
- Reports: Add browser statistics report [#7197]

### Major Changes
- Port crypto backend to [Botan](https://github.com/randombit/botan) [#6209]
- Improve attachment handling and security [#6606][#5034][#7083]
- Allow selecting any open database in unlock dialog [#5427]
- KeeShare: Remove checking signed container and QuaZip dependency [#7223]
- Introduce security option to enable copy on double click (default off) [#6433]
- Add 'delete entry without confirm' functionality [#5812]
- Improve macOS and Windows platform integration [#5851]
- Lock only the current database by default [#6652]
- Show expired entries on DB unlock [#7290]
- Update D-Bus adaptor interface class name to match definition file [#7523]

### Other Changes and Fixes
- Add countdown progress bar to TOTP preview [#6930]
- Enter favicon url directly on icons page [#6614]
- Set C++17 as standard in the build system [#7180]
- Internalize ykcore into code base [#6654]
- Transition to Visual Studio builds on Windows [#5874]
- Ability to delete entries from health check reports [#6537]
- Enhance remembering last-used directories [#6711]
- Implement org.freedesktop.appearance.color-scheme support on Linux [#7422]
- Support sorting HTML export [#7011]
- Add display number of characters in passphrases [#5449]
- Use Alt+Tab on macOS to switch between databases [#5407]
- Add feature to sort groups using shortcut keys [#6999]
- Add CTRL+Enter to apply password generator changes [#6414]
- Display `Database created` timestamp on statistics report [#6876]
- Browser: Improve best matching credentials setting [#6893]
- SSH Agent: Use both Pageant and OpenSSH agent simultaneously on Windows [#6288]
- SSH Agent: Allow using database path to resolve keys [#6365]
- SSH Agent: Show correct error messages in main window [#7166]
- Multiple fixes for MSI installer [#6630]
- Fix tab order for CSV import dialog to match screen order [#7315]
- Don't mark kdbx:// urls as invalid [#7221]
- Make selected text copyable instead of copying password [#7209]
- Detect timestamp resolution for CSV files [#7196]
- Fix crash while downloading favicon [#7104]
- Correct naming of newly generated keyx files [#7010]
- Place the 'Recycle Bin' at the bottom of the list when groups are sorted [#7004]
- Handle tilde with custom browser paths [#6659]
- Don't scroll up when deleting an entry [#6833]
- Set the MIME-Type to text/plain when using wl-copy on wayland [#6832]
- Fix adaptive icon painting [#5989][#6033]
- Fix favicon download from URL with non-standard port [#5509]
- Ignore recycle bin on KeePassHTTP migration [#5481]
- Fix keepassxc-cr-recovery utility [#7521]
- Fix Auto-Type not working when audio recording indicator is active on macOS 12.2+ [#7526]

## 2.6.6 (2021-06-12)

### Fixed

- Fix focusing search when pressing hotkey [#6603]
- Trim whitespace from TOTP key input prior to processing [#6604]
- Fix building on macOS [#6598]
- Resolve compiler warnings for unused return values [#6607]

## 2.6.5 (2021-06-08)

### Added

- Show search bar when toolbar is hidden or in overflow [#6279]
- Show countdown for clipboard clearing in status bar [#6333]
- Command line option to lock all open databases [#6511]
- Allow CSV import of bare TOTP secrets [#6211]
- Retain file creation time when saving database [#6576]
- Set permissions of saved attachments to be private to the current user [#6363]
- OPVault: Use Text instead of Name for attribute names [#6334]

### Changed

- Reports: Allow resizing of reports columns [#6435]
- Reports: Toggle showing expired entries [#6534]
- Save Always on Top setting [#6236]
- Password generator can exclude additional lookalike characters (6/G, 8/B) [#6196]

### Fixed

- Allow setting MSI properties in unattended install [#6196]
- Update MainWindow minimum size to enable smaller verticle space [#6196]
- Use application font size when setting default or monospace fonts [#6332]
- Fix notes not clearing in entry preview panel in some cases [#6481]
- macOS: Correct window activation when restoring from tray [#6575]
- macOS: Better handling of minimize after unlock when using browser integration [#6338]
- Linux: Start after the system tray is available on LXQt [#6216]
- Linux: Allow selection of modal dialogs on X11 in Auto-Type [#6204]
- KeeShare: prevent crash when file extension is missing [#6174]

## 2.6.4 (2021-01-31)

### Added

- Automatically adapt to light/dark system theme changes (Windows/macOS only) [#6034]

### Changed

- Show window title as tooltip on system tray [#5948]
- Compress Snap release as LZO for faster initial startup [#5877]
- Password generator: Set maximum selectable password length to 999 [#5937]

### Fixed

- Fix crash on app close when using SSH agent [#5935]
- Fix KDF selection showing wrong item when using Argon2id [#5923]
- Automatically close About dialog on database lock if it is still open [#5947]
- Linux: Fix automatic launch at system startup with AppImages [#5901]
- Linux: Fix click-to-move on empty area activating when using menus [#5971]
- Linux: Try multiple times to show tray icon if tray is not ready yet [#5948]
- macOS: Fix KeePassXC blocking clean shutdown [#6002]

## 2.6.3 (2021-01-12)

### Added

- Support Argon2id KDF [#5778]
- Support XMLv2 key files [#5798]

### Changed

- Improve CSV Import/Export, include time fields and TOTP [#5346]
- Support empty area dragging of the application window [#5860]
- Display default Auto-Type sequence in preview pane [#5654]
- Remove strict length limit on generated passwords [#5748]
- Hide key file path by default when unlocking database [#5779]
- Document browser extension use with Edge in managed mode [#5692]
- Windows: Prevent clipboard history and cloud sync [#5853]
- macOS: Update the application icon to Big Sur styling [#5851]

### Fixed

- Re-select previously selected entry on database unlock [#5559]
- Properly save special character choice in password generator [#5610]
- Fix crash in browser integration with multiple similar entries [#5653]
- Remove offset on username field in classic theme [#5788]
- Ensure entry history is copied when drag/dropping entries and groups [#5817]
- Close modal dialogs when database is locked [#5820]
- Prevent crash when KeeShare modifies an entry that is currently being edited [#5827]
- Improve preview of entry attributes [#5834]
- Always activate/focus database open dialog preventing mistype [#5878]
- Reports: fix calculation of average password length [#5862]
- Linux: Delay startup on login to correct tray icon issues [#5724]

## 2.6.2 (2020-10-21)

### Added

- Add option to keep window always on top to view menu [#5542]
- Move show/hide usernames and passwords to view menu [#5542]
- Add command line options and environment variables for changing the config locations [#5452]
- Include TOTP settings in CSV import/export and add support for ISO datetimes [#5346]

### Changed

- Mask sensitive information in command execution confirmation prompt [#5542]
- SSH Agent: Avoid shortcut conflict on macOS by changing "Add key" to Ctrl+H on all platforms [#5484]

### Fixed

- Prevent data loss with drag and drop between databases [#5536]
- Fix crash when toggling Capslock rapidly [#5545]
- Don't mark URL references as invalid URL [#5380]
- Reset entry preview after search [#5483]
- Set Qt::Dialog flag on database open dialog [#5356]
- Fix sorting of database report columns [#5426]
- Fix IfDevice matching logic [#5344]
- Fix layout issues and a stray scrollbar appearing on top of the entry edit screen [#5424]
- Fix tabbing into the notes field [#5424]
- Fix password generator ignoring settings on load [#5340]
- Restore natural entry sort order on application load [#5438]
- Fix paperclip and TOTP columns not saving state [#5327]
- Enforce fixed password font in entry preview [#5454]
- Add scrollbar when new database wizard exceeds screen size [#5560]
- Do not mark database as modified when viewing Auto-Type associations [#5542]
- CLI: Fix two heap-use-after-free crashes [#5368,#5470]
- Browser: Fix key exchange not working with multiple simultaneous users on Windows [#5485]
- Browser: Fix entry retrieval when "only best matching" is enabled [#5316]
- Browser: Ignore recycle bin on KeePassHTTP migration [#5481]
- KeeShare: Fix import crash [#5542]
- macOS: Fix toolbar theming and breadcrumb display issues [#5482]
- macOS: Fix file dialog randomly closing [#5479]
- macOS: Fix being unable to select OPVault files for import [#5341]

## 2.6.1 (2020-08-19)

### Added

- Add menu entries for auto-typing only username or only password [#4891]
- Browser: Add command for retrieving current TOTP [#5278]
- Improve man pages [#5010]
- Linux: Support Xfce screen lock signals [#4971]
- Linux: Add OARS metadata to AppStream markup [#5031]
- SSH Agent: Substitute tilde with %USERPROFILE% on Windows [#5116]

### Changed

- Improve password generator UI and UX [#5129]
- Do not prompt to restart if switching the theme back and forth [#5084]
- Change actions for F1, F2, and F3 keys [#5082]
- Skip referenced passwords in health check report [#5056]
- Check system-wide Qt translations directory for downstream translations packaging [#5064]
- macOS: Change password visibility toggle shortcut to Ctrl+H to avoid conflict with system shortcut [#5114]
- Browser: Only display domain name in browser access confirm dialog to avoid overly wide window sizes [#5214]

### Fixed

- Fix clipboard not being cleared when database is locked while timeout is still active [#5184]
- Fix list of previous databases not being cleared in some cases [#5123]
- Fix saving of non-data changes on database lock [#5210]
- Fix search results banner theming [#5197]
- Don't enforce theme palette in Classic theme mode and add hover effect for buttons [#5122,#5267]
- Fix label clipping in settings on high-DPI screens [#5227]
- Fix excessive memory usage by icons on systems with high-DPI screens [#5266]
- Fix crash if number of TOTP digits exceeds ten [#5106]
- Fix slot detection when first YubiKey is configured on the second slot [#5004]
- Prevent crash if focus widget gets deleted during saving [#5005]
- Always show buttons for opening or saving attachments [#4956]
- Update link to Auto-Type help [#5228]
- Fix build errors with Ninja [#5121]
- CLI: Fix db-info command wrongly labelled as db-show in usage listing [#5140]
- Windows: Use Classic theme by default if high-contrast mode is on [#5191]
- Linux: Add workaround for qt5ct bug, causing icons not to show up [#5011]
- Linux: Correct high-DPI display by not allowing fractional scaling [#5185]
- Browser: Consider subdomain and path when requesting only "best-matching credentials" [#4832]
- SSH Agent: Always forget all keys on lock [#5115]

## 2.6.0 (2020-07-06)

### Added

- Custom Light and Dark themes [#4110, #4769, #4791, #4892, #4915]
- Compact mode to use classic Group and Entry line height [#4910]
- New monochrome tray icons [#4796, #4803]
- View menu to quickly switch themes, compact mode, and toggle UI elements [#4910]
- Search for groups and scope search to matched groups [#4705]
- Save Database Backup feature [#4550]
- Sort entries by "natural order" and move lines up/down [#4357]
- Option to launch KeePassXC on system startup/login [#4675]
- Caps Lock warning on password input fields [#3646]
- Add "Size" column to entry view [#4588]
- Browser-like tab experience using Ctrl+[Num] (Alt+[Num] on Linux) [#4063, #4305]
- Password Generator: Define additional characters to choose from [#3876]
- Reports: Database password health check (offline) [#3993]
- Reports: HIBP online service to check for breached passwords [#4438]
- Auto-Type: DateTime placeholders [#4409]
- Browser: Show group name in results sent to browser extension [#4111]
- Browser: Ability to define a custom browser location (macOS and Linux only) [#4148]
- Browser: Ability to change root group UUID and inline edit connection ID [#4315, #4591]
- CLI: `db-info` command [#4231]
- CLI: Use wl-clipboard if xclip is not available (Linux) [#4323]
- CLI: Incorporate xclip into snap builds [#4697]
- SSH Agent: Key file path env substitution, SSH_AUTH_SOCK override, and connection test [#3769, #3801, #4545]
- SSH Agent: Context menu actions to add/remove keys [#4290]

### Changed

- Complete replacement of default database icons [#4699]
- Complete replacement of application icons [#4066, #4161, #4203, #4411]
- Complete rewrite of documentation and manpages using Asciidoctor [#4937]
- Complete refactor of config files; separate between local and roaming [#4665]
- Complete refactor of browser integration and proxy code [#4680]
- Complete refactor of hardware key integration (YubiKey and OnlyKey) [#4584, #4843]
- Significantly improve performance when saving and opening databases [#4309, #4833]
- Remove read-only detection for database files [#4508]
- Overhaul of password fields and password generator [#4367]
- Replace instances of "Master Key" with "Database Credentials" [#4929]
- Change settings checkboxes to positive phrasing for consistency [#4715]
- Improve UX of using entry actions (focus fix) [#3893]
- Set expiration time to Now when enabling entry expiration [#4406]
- Always show "New Entry" in context menu [#4617]
- Issue warning before adding large attachments [#4651]
- Improve importing OPVault [#4630]
- Improve AutoOpen capability [#3901, #4752]
- Check for updates every 7 days even while still running [#4752]
- Improve Windows installer UI/UX [#4675]
- Improve config file handling of portable distribution [#4131, #4752]
- macOS: Hide dock icon when application is hidden to tray [#4782]
- Browser: Use unlock dialog to improve UX of opening a locked database [#3698]
- Browser: Improve database and entry settings experience [#4392, #4591]
- Browser: Improve confirm access dialog [#2143, #4660]
- KeeShare: Improve monitoring file changes of shares [#4720]
- CLI: Rename `create` command to `db-create` [#4231]
- CLI: Cleanup `db-create` options (`--set-key-file` and `--set-password`) [#4313]
- CLI: Use stderr for help text and password prompts [#4086, #4623]
- FdoSecrets: Display existing secret service process [#4128]

### Fixed

- Fix changing focus around the main window using tab key [#4641]
- Fix search field clearing while still using the application [#4368]
- Improve search help widget displaying on macOS and Linux [#4236]
- Return keyboard focus after editing an entry [#4287]
- Reset database path after failed "Save As" [#4526]
- Make builds reproducible [#4411]
- Improve handling of ccache when building [#4104, #4335]
- Windows: Use correct UI font and size [#4769]
- macOS: Properly re-hide application window after browser integration and Auto-Type usage [#4909]
- Linux: Fix version number not embedded in AppImage [#4842]
- Auto-Type: Fix crash when performing on new entry [#4132]
- Browser: Send legacy HTTP settings to recycle bin [#4589]
- Browser: Fix merging browser keys [#4685]
- CLI: Fix encoding when exporting database [#3921]
- SSH Agent: Improve reliability and underlying code [#3833, #4256, #4549, #4595]
- FdoSecrets: Fix crash when editing settings before service is enabled [#4332]

## 2.5.4 (2020-04-09)

### Fixed

- Return keyboard focus after saving database edits [#4287]
- Windows: Use bare minimum settings in portable version [#4131]
- Windows: Use SHA256 code signing [#4129]
- macOS: Fix code signing incompatibility in latest macOS release [#4564]

## 2.5.3 (2020-01-19)

### Fixed

- Fix a possible database lockout when removing a YubiKey from a KDBX 3.1 database [#4147]
- Fix crash if Auto-Type is performed on a new entry [#4150]
- Fix crash when all entries are deleted from a group [#4156]
- Improve the reliability of clipboard clearing on Gnome [#4165]
- Do not check cmd:// URLs for valid URL syntax anymore [#4172]
- Prevent unnecessary merges for databases on network shares [#4153]
- Browser: Prevent native messaging proxy from blocking application shutdown [#4155]
- Browser: Improve website URL matching [#4134, #4177]

### Added

- Browser: Enable support for Chromium-based Edge Browser [#3359]

## 2.5.2 (2020-01-04)

### Added

- Browser: Show UI warning when entering invalid URLs [#3912]
- Browser: Option to use an entry only for HTTP auth [#3927]

### Changed

- Disable the user interface when merging or saving the database [#3991]
- Ability to hide protected attribute after reveal [#3877]
- Remove mention of "snaps" in Windows and macOS [#3879]
- CLI: Merge parameter for source database key file (--key-file-from) [#3961]
- Improve GUI tests reliability on Hi-DPI displays [#4075]
- Disable deprecation warnings to allow building with Qt 5.14+ [#4075]
- OPVault: Use 'otp' attribute for TOTP field imports [#4075]

### Fixed

- Fix crashes when saving a database to cloud storage [#3991]
- Fix crash when pressing enter twice while opening database [#3885]
- Fix handling of HTML when displayed in the entry preview panel [#3910]
- Fix start minimized to tray on Linux [#3899]
- Fix Auto Open with key file only databases [#4075]
- Fix escape key closing the standalone password generator [#3892]
- macOS: Fix monospace font usage in password field and notes [#4075]
- macOS: Fix building on macOS 10.9 to 10.11 [#3946]
- Fix TOTP setup dialog not closing on database lock [#4075]
- Browser: Fix condition where additional URLs are ignored [#4033]
- Browser: Fix subdomain matching to return only relevant site entries [#3854]
- Secret Service: Fix multiple crashes and incompatibilities [#3871, #4009, #4074]
- Secret Service: Fix searching of entries [#4008, #4036]
- Secret Service: Fix behavior when exposed group is recycled [#3914]
- CLI: Release the database instance before exiting interactive mode [#3889]
- Fix (most) memory leaks in tests [#3922]

## 2.5.1 (2019-11-11)

### Added

- Add programmatic use of the EntrySearcher [#3760]
- Explicitly clear database memory upon locking even if the object is not deleted immediately [#3824]
- macOS: Add ability to perform notarization of built package [#3827]

### Changed

- Reduce file hash checking to every 30 seconds to correct performance issues [#3724]
- Correct formatting of notes in entry preview widget [#3727]
- Improve performance and UX of database statistics page [#3780]
- Improve interface for key file selection to discourage use of the database file [#3807]
- Hide Auto-Type sequences column when not needed [#3794]
- macOS: Revert back to using Carbon API for hotkey detection [#3794]
- CLI: Do not show protected fields by default [#3710]

### Fixed

- Secret Service: Correct issues interfacing with various applications [#3761]
- Fix building without additional features [#3693]
- Fix handling TOTP secret keys that require padding [#3764]
- Fix database unlock dialog password field focus [#3764]
- Correctly label open databases as locked on launch [#3764]
- Prevent infinite recursion when two databases AutoOpen each other [#3764]
- Browser: Fix incorrect matching of invalid URLs [#3759]
- Properly stylize the application name on Linux [#3775]
- Show application icon on Plasma Wayland sessions [#3777]
- macOS: Check for Auto-Type permissions on use instead of at launch [#3794]

## 2.5.0 (2019-10-26)

### Added

- Add 'Paper Backup' aka 'Export to HTML file' to the 'Database' menu [#3277]
- Add statistics panel with information about the database (number of entries, number of unique passwords, etc.) to the Database Settings dialog [#2034]
- Add offline user manual accessible via the 'Help' menu [#3274]
- Add support for importing 1Password OpVault files [#2292]
- Implement Freedesktop.org secret storage DBus protocol so that KeePassXC can be used as a vault service by libsecret [#2726]
- Add support for OnlyKey as an alternative to YubiKeys (requires yubikey-personalization >= 1.20.0) [#3352]
- Add group sorting feature [#3282]
- Add feature to download favicons for all entries at once [#3169]
- Add word case option to passphrase generator [#3172]
- Add support for RFC6238-compliant TOTP hashes [#2972]
- Add UNIX man page for main program [#3665]
- Add 'Monospaced font' option to the notes field [#3321]
- Add support for key files in auto open [#3504]
- Add search field for filtering entries in Auto-Type dialog [#2955]
- Complete usernames based on known usernames from other entries [#3300]
- Parse hyperlinks in the notes field of the entry preview pane [#3596]
- Allow abbreviation of field names in entry search [#3440]
- Allow setting group icons recursively [#3273]
- Add copy context menu for username and password in Auto-Type dialog [#3038]
- Drop to background after copying a password to the clipboard [#3253]
- Add 'Lock databases' entry to tray icon menu [#2896]
- Add option to minimize window after unlocking [#3439]
- Add option to minimize window after opening a URL [#3302]
- Request accessibility permissions for Auto-Type on macOS [#3624]
- Browser: Add initial support for multiple URLs [#3558]
- Browser: Add entry-specific browser integration settings [#3444]
- CLI: Add offline HIBP checker (requires a downloaded HIBP dump) [#2707]
- CLI: Add 'flatten' option to the 'ls' command [#3276]
- CLI: Add password generation options to `Add` and `Edit` commands [#3275]
- CLI: Add XML import [#3572]
- CLI: Add CSV export to the 'export' command [#3278]
- CLI: Add `-y --yubikey` option for YubiKey [#3416]
- CLI: Add `--dry-run` option for merging databases [#3254]
- CLI: Add group commands (mv, mkdir and rmdir) [#3313].
- CLI: Add interactive shell mode command `open` [#3224]


### Changed

- Redesign database unlock dialog [ #3287]
- Rework the entry preview panel [ #3306]
- Move notes to General tab on Group Preview Panel [#3336]
- Enable entry actions when editing an entry and cleanup entry context menu [#3641]
- Improve detection of external database changes [#2389]
- Warn if user is trying to use a KDBX file as a key file [#3625]
- Add option to disable KeePassHTTP settings migrations prompt [#3349, #3344]
- Re-enabled Wayland support (no Auto-Type yet) [#3520, #3341]
- Add icon to 'Toggle Window' action in tray icon menu [#3244]
- Merge custom data between databases only when necessary [#3475]
- Improve various file-handling related issues when picking files using the system's file dialog [#3473]
- Add 'New Entry' context menu when no entries are selected [#3671]
- Reduce default Argon2 settings from 128 MiB and one thread per CPU core to 64 MiB and two threads to account for lower-spec mobile hardware [ #3672]
- Browser: Remove unused 'Remember' checkbox for HTTP Basic Auth [#3371]
- Browser: Show database name when pairing with a new browser [#3638]
- Browser: Show URL in allow access dialog [#3639]
- CLI: The password length option `-l` for the CLI commands `Add` and `Edit` is now `-L` [#3275]
- CLI: The `-u` shorthand for the `--upper` password generation option has been renamed to `-U` [#3275]
- CLI: Rename command `extract` to `export`. [#3277]

### Fixed

- Improve accessibility for assistive technologies [#3409]
- Correctly unlock all databases if `--pw-stdin` is provided [#2916]
- Fix password generator issues with special characters [#3303]
- Fix KeePassXC interrupting shutdown procedure [#3666]
- Fix password visibility toggle button state on unlock dialog [#3312]
- Fix potential data loss if database is reloaded while user is editing an entry [#3656]
- Fix hard-coded background color in search help popup [#3001]
- Fix font choice for password preview [#3425]
- Fix handling of read-only files when autosave is enabled [#3408]
- Handle symlinks correctly when atomic saves are disabled [#3463]
- Enable HighDPI icon scaling on Linux [#3332]
- Make Auto-Type on macOS more robust and remove old Carbon API calls [#3634, [#3347)]
- Hide Share tab if KeePassXC is compiled without KeeShare support and other minor KeeShare improvements [#3654, [#3291, #3029, #3031, #3236]
- Correctly bring window to the front when clicking tray icon on macOS [#3576]
- Correct application shortcut created by MSI Installer on Windows [#3296]
- Fix crash when removing custom data [#3508]
- Fix placeholder resolution in URLs [#3281]
- Fix various inconsistencies and platform-dependent compilation bugs [#3664, #3662, #3660, #3655, #3649, #3417, #3357, #3319, #3318, #3304]
- Browser: Fix potential leaking of entries through the browser integration API if multiple databases are opened [#3480]
- Browser: Fix password entropy calculation [#3107]
- Browser: Fix Windows registry settings for portable installation [#3603]

## 2.4.3 (2019-06-12)

### Added

- Add documentation for keyboard shortcuts to source code distribution [#3215]

### Fixed

- Fix library loading issues in the Snap and macOS releases [#3247]
- Fix various keyboard navigation issues [#3248]
- Fix main window toggling regression when clicking the tray icon on KDE [#3258]

## 2.4.2 (2019-05-31)

- Improve resilience against memory attacks - overwrite memory before free [#3020]
- Prevent infinite save loop when location is unavailable [#3026]
- Attempt to fix quitting application when shutdown or logout issued [#3199]
- Support merging database custom data [#3002]
- Fix opening URL's with non-http schemes [#3153]
- Fix data loss due to not reading all database attachments if duplicates exist [#3180]
- Fix entry context menu disabling when using keyboard navigation [#3199]
- Fix behaviors when canceling an entry edit [#3199]
- Fix processing of tray icon click and doubleclick [#3112]
- Update group in preview widget when focused [#3199]
- Prefer DuckDuckGo service over direct icon download (increases resolution) #2996]
- Remove apply button in application settings [#3019]
- Use winqtdeploy on Windows to correct deployment issues [#3025]
- Don't mark entry edit as modified when attribute selection changes [#3041]
- Use console code page CP_UTF8 on Windows if supported [#3050]
- Snap: Fix locking database with session lock [#3046]
- Snap: Fix theming across Linux distributions [#3057]
- Snap: Use SNAP_USER_COMMON and SNAP_USER_DATA directories [#3131]
- KeeShare: Automatically enable WITH_XC_KEESHARE_SECURE if quazip is found [#3088]
- macOS: Fix toolbar text when in dark mode [#2998]
- macOS: Lock database on switching user [#3097]
- macOS: Fix global Auto-Type when the database is locked[ #3138]
- Browser: Close popups when database is locked [#3093]
- Browser: Add tests [#3016]
- Browser: Don't create default group if custom group is enabled [#3127]

## 2.4.1 (2019-04-12)

- Fix database deletion when using unsafe saves to a different file system #2889
- Fix opening databases with legacy key files that contain '/' #2872
- Fix opening database files from the command line #2919
- Fix crash when editing master key #2836
- Fix multiple issues with apply button behavior #2947
- Fix issues on application startup (tab order, --pw-stdin, etc.) #2830
- Fix building without WITH_XC_KEESHARE
- Fix reference entry coloring on macOS dark mode #2984
- Hide window when performing entry auto-type on macOS #2969
- Improve UX of update checker; reduce checks to every 7 days #2968
- KeeShare improvements [#2946, #2978, #2824]
- Re-enable Ctrl+C to copy password from search box #2947
- Add KeePassXC-Browser integration for Brave browser #2933
- SSH Agent: Re-Add keys on database unlock #2982
- SSH Agent: Only remove keys on app exit if they are removed on lock #2985
- CLI: Add --no-password option #2708
- CLI: Improve database extraction to XML #2698
- CLI: Don't call mandb on build #2774
- CLI: Add debug info #2714
- Improve support for Snap theming #2832
- Add support for building on Haiku OS #2859
- Ctrl+PgDn now goes to the next tab and Ctrl+PgUp to the previous
- Fix compiling on GCC 5 / Xenial #2990
- Add .gitrev output to tarball for third-party builds #2970
- Add WITH_XC_UPDATECHECK compile flag to toggle the update checker #2968

## 2.4.0 (2019-03-19)

- New Database Wizard #1952
- Advanced Search #1797
- Automatic update checker #2648
- KeeShare database synchronization [#2109, #1992, #2738, #2742, #2746, #2739]
- Improve favicon fetching; transition to Duck-Duck-Go [#2795, #2011, #2439]
- Remove KeePassHttp support #1752
- CLI: output info to stderr for easier scripting #2558
- CLI: Add --quiet option #2507
- CLI: Add create command #2540
- CLI: Add recursive listing of entries #2345
- CLI: Fix stdin/stdout encoding on Windows #2425
- SSH Agent: Support OpenSSH for Windows #1994
- macOS: TouchID Quick Unlock #1851
- macOS: Multiple improvements; include CLI in DMG [#2165, #2331, #2583]
- Linux: Prevent Klipper from storing secrets in clipboard #1969
- Linux: Use polling based file watching for NFS #2171
- Linux: Enable use of browser plugin in Snap build #2802
- TOTP QR Code Generator #1167
- High-DPI Scaling for 4k screens #2404
- Make keyboard shortcuts more consistent #2431
- Warn user if deleting referenced entries #1744
- Allow toolbar to be hidden and repositioned [#1819, #2357]
- Increase max allowed database timeout to 12 hours #2173
- Password generator uses existing password length by default #2318
- Improve alert message box button labels #2376
- Show message when a database merge makes no changes #2551
- Browser Integration Enhancements [#1497, #2253, #1904, #2232, #1850, #2218, #2391, #2396, #2542, #2622, #2637, #2790]
- Overall Code Improvements [#2316, #2284, #2351, #2402, #2410, #2419, #2422, #2443, #2491, #2506, #2610, #2667, #2709, #2731]

## 2.3.4 (2018-08-21)

- Show all URL schemes in entry view #1768
- Disable merge when database is locked #1975
- Fix intermittent crashes with favorite icon downloads #1980
- Provide potential crash warning to Qt 5.5.x users #2211
- Disable apply button when creating new entry/group to prevent data loss #2204
- Allow for 12 hour timeout to lock idle database #2173
- Multiple SSH Agent fixes [#1981, #2117]
- Multiple Browser Integration enhancements [#1993, #2003, #2055, #2116, #2159, #2174, #2185]
- Fix browser proxy application not closing properly #2142
- Add real names and Patreon supporters to about dialog #2214
- Add settings button to toolbar, Donate button, and Report a Bug button to help menu #2214
- Enhancements to release-tool to appsign intermediate build products #2101


## 2.3.3 (2018-05-09)

- Fix crash when browser integration is enabled #1923

## 2.3.2 (2018-05-07)

- Enable high entropy ASLR on Windows #1747
- Enhance favicon fetching #1786
- Fix crash on Windows due to autotype #1691
- Fix dark tray icon changing all icons #1680
- Fix --pw-stdin not using getPassword function #1686
- Fix placeholders being resolved in notes #1907
- Enable auto-type start delay to be configurable #1908
- Browser: Fix native messaging reply size #1719
- Browser: Increase maximum buffer size #1720
- Browser: Enhance usability and functionality [#1810, #1822, #1830, #1884, #1906]
- SSH Agent: Parse aes-256-cbc/ctr keys #1682
- SSH Agent: Enhance usability and functionality [#1677, #1679, #1681, #1787]

## 2.3.1 (2018-03-06)

- Fix unnecessary automatic upgrade to KDBX 4.0 and prevent challenge-response key being stripped #1568
- Abort saving and show an error message when challenge-response fails #1659
- Support inner stream protection on all string attributes #1646
- Fix favicon downloads not finishing on some websites #1657
- Fix freeze due to invalid STDIN data #1628
- Correct issue with encrypted RSA SSH keys #1587
- Fix crash on macOS due to QTBUG-54832 #1607
- Show error message if ssh-agent communication fails #1614
- Fix --pw-stdin and filename parameters being ignored #1608
- Fix Auto-Type syntax check not allowing spaces and special characters #1626
- Fix reference placeholders in combination with Auto-Type #1649
- Fix qtbase translations not being loaded #1611
- Fix startup crash on Windows due to missing SVG libraries #1662
- Correct database tab order regression #1610
- Fix GCC 8 compilation error #1612
- Fix copying of advanced attributes on KDE #1640
- Fix member initialization of CategoryListWidgetDelegate #1613
- Fix inconsistent toolbar icon sizes and provide higher-quality icons #1616
- Improve preview panel geometry #1609

## 2.3.0 (2018-02-27)

- Add support for KDBX 4.0, Argon2 and ChaCha20 [#148, #1179, #1230, #1494]
- Add SSH Agent feature [#1098, #1450, #1463]
- Add preview panel with details of the selected entry [#879, #1338]
- Add more and configurable columns to entry table and allow copying of values by double click #1305
- Add KeePassXC-Browser API as a replacement for KeePassHTTP #608
- Deprecate KeePassHTTP #1392
- Add support for Steam one-time passwords #1206
- Add support for multiple Auto-Type sequences for a single entry #1390
- Adjust YubiKey HMAC-SHA1 challenge-response key generation for KDBX 4.0 #1060
- Replace qHttp with cURL for website icon downloads #1460
- Remove lock file #1231
- Add option to create backup file before saving #1385
- Ask to save a generated password before closing the entry password generator #1499
- Resolve placeholders recursively #1078
- Add Auto-Type button to the toolbar #1056
- Improve window focus handling for Auto-Type dialogs [#1204, #1490]
- Auto-Type dialog and password generator can now be exited with ESC [#1252, #1412]
- Add optional dark tray icon #1154
- Add new "Unsafe saving" option to work around saving problems with file sync services #1385
- Add IBus support to AppImage and additional image formats to Windows builds [#1534, #1537]
- Add diceware password generator to CLI #1406
- Add --key-file option to CLI [#816, #824]
- Add DBus interface for opening and closing KeePassXC databases #283
- Add KDBX compression options to database settings #1419
- Discourage use of old fixed-length key files in favor of arbitrary files [#1326, #1327]
- Correct reference resolution in entry fields #1486
- Fix window state and recent databases not being remembered on exit #1453
- Correct history item generation when configuring TOTP for an entry #1446
- Correct multiple TOTP bugs #1414
- Automatic saving after every change is now a default #279
- Allow creation of new entries during search #1398
- Correct menu issues on macOS #1335
- Allow compilation on OpenBSD #1328
- Improve entry attachments view [#1139, #1298]
- Fix auto lock for Gnome and Xfce [#910, #1249]
- Don't remember key files in file dialogs when the setting is disabled #1188
- Improve database merging and conflict resolution [#807, #1165]
- Fix macOS pasteboard issues #1202
- Improve startup times on some platforms #1205
- Hide the notes field by default #1124
- Toggle main window by clicking tray icon with the middle mouse button #992
- Fix custom icons not copied over when databases are merged #1008
- Allow use of DEL key to delete entries #914
- Correct intermittent crash due to stale history items #1527
- Sanitize newline characters in title, username and URL fields #1502
- Reopen previously opened databases in correct order #774
- Use system's zxcvbn library if available #701
- Implement various i18n improvements [#690, #875, #1436]

## 2.2.4 (2017-12-13)

- Prevent database corruption when locked #1219
- Fixes apply button not saving new entries #1141
- Switch to Consolas font on Windows for password edit #1229
- Multiple fixes to AppImage deployment [#1115, #1228]
- Fixes multiple memory leaks #1213
- Resize message close to 16x16 pixels #1253

## 2.2.2 (2017-10-22)

- Fixed entries with empty URLs being reported to KeePassHTTP clients #1031
- Fixed YubiKey detection and enabled CLI tool for AppImage binary #1100
- Added AppStream description #1082
- Improved TOTP compatibility and added new Base32 implementation #1069
- Fixed error handling when processing invalid cipher stream #1099
- Fixed double warning display when opening a database #1037
- Fixed unlocking databases with --pw-stdin #1087
- Added ability to override QT_PLUGIN_PATH environment variable for AppImages #1079
- Fixed transform seed not being regenerated when saving the database #1068
- Fixed only one YubiKey slot being polled #1048
- Corrected an issue with entry icons while merging #1008
- Corrected desktop and tray icons in Snap package #1030
- Fixed screen lock and Google fallback settings #1029

## 2.2.1 (2017-10-01)

- Corrected multiple snap issues [#934, #1011]
- Corrected multiple custom icon issues [#708, #719, #994]
- Corrected multiple Yubikey issues #880
- Fixed single instance preventing load on occasion #997
- Keep entry history when merging databases #970
- Prevent data loss if passwords were mismatched #1007
- Fixed crash after merge #941
- Added configurable auto-type default delay #703
- Unlock database dialog window comes to front #663
- Translation and compiling fixes

## 2.2.0 (2017-06-23)

- Added YubiKey 2FA integration for unlocking databases #127
- Added TOTP support #519
- Added CSV import tool [#146, #490]
- Added KeePassXC CLI tool #254
- Added diceware password generator #373
- Added support for entry references [#370, #378]
- Added support for Twofish encryption #167
- Enabled DEP and ASLR for in-memory protection #371
- Enabled single instance mode #510
- Enabled portable mode #645
- Enabled database lock on screensaver and session lock #545
- Redesigned welcome screen with common features and recent databases #292
- Multiple updates to search behavior [#168, #213, #374, #471, #603, #654]
- Added auto-type fields {CLEARFIELD}, {SPACE}, {{}, {}} [#267, #427, #480]
- Fixed auto-type errors on Linux #550
- Prompt user prior to executing a cmd:// URL #235
- Entry attributes can be protected (hidden) #220
- Added extended ascii to password generator #538
- Added new database icon to toolbar #289
- Added context menu entry to empty recycle bin in databases #520
- Added "apply" button to entry and group edit windows #624
- Added macOS tray icon and enabled minimize on close #583
- Fixed issues with unclean shutdowns [#170, #580]
- Changed keyboard shortcut to create new database to CTRL+SHIFT+N #515
- Compare window title to entry URLs #556
- Implemented inline error messages #162
- Ignore group expansion and other minor changes when making database "dirty" #464
- Updated license and copyright information on souce files #632
- Added contributors list to about dialog #629

## 2.1.4 (2017-04-09)

- Bumped KeePassHTTP version to 1.8.4.2
- KeePassHTTP confirmation window comes to foreground #466

## 2.1.3 (2017-03-03)

- Fix possible overflow in zxcvbn library #363
- Revert HiDPI setting to avoid problems on laptop screens #332
- Set file meta properties in Windows executable #330
- Suppress error message when auto-reloading a locked database #345
- Improve usability of question dialog when database is already locked by a different instance #346
- Fix compiler warnings in QHttp library #351
- Use unified toolbar on Mac OS X #361
- Fix an issue on X11 where the main window would be raised instead of closed on Alt+F4 #362

## 2.1.2 (2017-02-17)

- Ask for save location when creating a new database #302
- Remove Libmicrohttpd dependency to clean up the code and ensure better OS X compatibility [#317, #265]
- Prevent Qt from degrading Wifi network performance on certain platforms #318
- Visually refine user interface on OS X and other platforms #299
- Remove unusable tray icon setting on OS X #293
- Fix compositing glitches on Ubuntu and prevent flashing when minimizing to the tray at startup #307
- Fix AppImage tray icon on Ubuntu [#277, #273]
- Fix global menu disappearing after restoring KeePassXC from the tray on Ubuntu #276
- Fix result order in entry search #320
- Enable HiDPI scaling on supported platforms #315
- Remove empty directories from installation target #282

## 2.1.1 (2017-02-06)

- Enabled HTTP plugin build; plugin is disabled by default and limited to localhost #147
- Escape HTML in dialog boxes #247
- Corrected crashes in favicon download and password generator [#233, #226]
- Increase font size of password meter #228
- Fixed compatibility with Qt 5.8 #211
- Use consistent button heights in password generator #229

## 2.1.0 (2017-01-22)

- Show unlock dialog when using autotype on a closed database [#10, #89]
- Show different tray icon when database is locked [#37, #46]
- Support autotype on Windows and OS X [#42, #60, #63]
- Add delay feature to autotype [#76, #77]
- Add password strength meter [#84, #92]
- Add option for automatically locking the database when minimizing
  the window #57
- Add feature to download favicons and use them as entry icons #30
- Automatically reload and merge database when the file changed on
  disk [#22, #33, #93]
- Add tool for merging two databases [#22, #47, #143]
- Add --pw-stdin commandline option to unlock the database by providing
  a password on STDIN #54
- Add utility script for reading the database password from KWallet #55
- Fix some KeePassHTTP settings not being remembered [#34, #65]
- Make search box persistent [#15, #67, #157]
- Enhance search feature by scoping the search to selected group [#16, #118]
- Improve interaction between search field and entry list [#131, #141]
- Add stand-alone password-generator [#18, #92]
- Don't require password repetition when password is visible [#27, #92]
- Add support for entry attributes in autotype sequences #107
- Always focus password field when opening the database unlock widget [#116, #117]
- Fix compilation errors on various platforms [#53, #126, #130]
- Restructure and improve kdbx-extract utility #160

## 2.0.3 (2016-09-04)

- Improved error reporting when reading / writing databases fails. [#450, #462]
- Display an error message when opening a custom icon fails.
- Detect custom icon format based on contents instead of the filename. #512
- Keep symlink intact when saving databases. #442.
- Fix a crash when deleting parent group of recycle bin. #520
- Display a confirm dialog before moving an entry to the recycle bin. #447
- Repair UUIDs of inconsistent history items. #130
- Only include top-level windows in auto-type window list when using gnome-shell.
- Update translations.

## 2.0.2 (2016-02-02)

- Fix regression in database writer that caused it to strip certain special
  characters (characters from Unicode plane > 0).
- Fix bug in repair function that caused it to strip non-ASCII characters.

## 2.0.1 (2016-01-31)

- Flush temporary file before opening attachment. #390
- Disable password generator when showing entry in history mode. #422
- Strip invalid XML chars when writing databases. #392
- Add repair function to fix databases with invalid XML chars. #392
- Display custom icons scaled. #322
- Allow opening databases that have no password and keyfile. #391
- Fix crash when importing .kdb files with invalid icon ids. #425
- Update translations.

## 2.0 (2015-12-06)

- Improve UI of the search edit.
- Clear clipboard when locking databases. #342
- Enable Ctrl+M shortcut to minimize the window on all platforms. #329
- Show a better message when trying to open an old database format. #338
- Fix global auto-type behavior with some window managers.
- Show global auto-type window on the active desktop. #359
- Disable systray on OS X. #326
- Restore main window when clicking on the OS X docker icon. #326

## 2.0 Beta 2 (2015-09-06)

- Fix crash when locking with search UI open #309
- Fix file locking on Mac OS X #327
- Set default extension when saving a database [#79, #308]

## 2.0 Beta 1 (2015-07-18)

- Remember entry column sizes #159
- Add translations
- Support opening attachments directly
- Support cmd:// URLs #244
- Protect opened databases with a file lock #18
- Export to csv files #57
- Add optional tray icon #153
- Allow setting the default auto-type sequence for groups #175
- Make the kdbx parser more lenient
- Remove --password command line option #285

## 2.0 Alpha 6 (2014-04-12)

- Add option to lock databases after user inactivity #62
- Add compatibility with libgcrypt 1.6 #129
- Display passwords in monospace font #51
- Use an icon for the button that shows/masks passwords #38
- Add an option to show passwords by default #93
- Improve password generator design #122
- On Linux link .kdbx files with KeePassX
- Remember window size #154
- Disallow global auto-typing when the database is locked

## 2.0 Alpha 5 (2013-12-20)

- Support copying entries and groups using drag'n'drop #74
- Open last used databases on startup #36
- Made the kdbx file parser more robust
- Only edit entries on doubleclick (not single) or with enter key
- Allow removing multiple entries
- Added option to minimize window when copying data to clipboard
- Save password generator settings
- Fixed auto-type producing wrong chars in some keyboard configurations #116
- Added some more actions to the toolbar

## 2.0 Alpha 4 (2013-03-29)

- Add random password generator #52
- Merge the 'Description' tab into the 'Entry' tab #59
- Fix crash when deleting history items #56
- Fix crash on Mac OS X Mountain Lion during startup #50
- Improved KeePassX application icon #58

## 2.0 Alpha 3 (2012-10-27)

- Auto-Type on Linux / X11
- Database locking
- Fix database corruption when changing key transformation rounds #34
- Verify header data of kdbx files
- Add menu entry to open URLs in the browser
- Add menu entry to copy an entry attribute to clipboard

## 2.0 Alpha 2 (2012-07-02)

- Import kdb (KeePass 1) files #2
- Display history items #23
- Implement history item limits #16
- Group and entry icons can be set #22
- Add keyboard shortcuts
- Search in databases #24
- Sortable entry view
- Support building Mac OS X bundles

## 2.0 Alpha 1 (2012-05-07)

- First release.
