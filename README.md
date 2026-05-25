# SecureVault ESP32

SecureVault ESP32 is a local, offline-first password and TOTP manager for the LilyGO T-Display S3 / ESP32-S3.

The project uses a small device UI plus a local Wi-Fi web portal for easier management.

---

## Current Status

SecureVault currently supports:

- PIN lock screen
- Encrypted password vault
- Encrypted Wi-Fi and TOTP secrets storage
- Local web portal with PIN login
- Multiple TOTP accounts
- USB HID auto-type in Product Mode
- Encrypted backup export
- Encrypted backup import with rollback protection
- DEV and PRODUCT PlatformIO environments

---

## Hardware

Tested with:

- LilyGO T-Display S3
- ESP32-S3 N16R8
- 170 x 320 display
- Two-button navigation
- USB HID support

---

## Development Setup

This project uses:

- Windows 11
- VS Code
- PlatformIO
- Arduino framework
- TFT_eSPI
- LittleFS

---

## PlatformIO Environments

The project has two environments:

```bash
lilygo-t-display-s3-dev
lilygo-t-display-s3-product
```

---

## Development Mode

Use this while coding and uploading.

```bash
pio run -e lilygo-t-display-s3-dev
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
pio device monitor -e lilygo-t-display-s3-dev --port COM3
```

Development Mode:

- `SECUREVAULT_DEV_MODE=1`
- USB HID typing is disabled
- Debug logs are enabled
- Safer for development
- Easier to recover during upload problems

---

## Product Mode

Use this only when testing USB auto-type.

```bash
pio run -e lilygo-t-display-s3-product
pio run -e lilygo-t-display-s3-product --target upload --upload-port COM3
```

Product Mode:

- `SECUREVAULT_DEV_MODE=0`
- USB HID typing is enabled
- Auto-type works
- Debug logs are reduced

Important:

Always test Product Mode first in Notepad, not on real login pages.

---

## Default PIN

Default PIN:

```text
0000
```

PIN can be changed on the device:

```text
Settings → Change PIN
```

When the PIN is changed:

- `/vault.enc` is re-encrypted with the new PIN-derived key
- `/secrets.enc` is re-encrypted with the new PIN-derived key

---

## Storage

SecureVault stores encrypted data in LittleFS:

```text
/vault.enc
/secrets.enc
```

---

## `/vault.enc`

Contains encrypted password vault entries.

Vault entries include:

- Title
- Username
- Password
- Category

---

## `/secrets.enc`

Contains encrypted secret settings:

- Wi-Fi SSID
- Wi-Fi password
- TOTP labels
- TOTP Base32 secrets

---

## Security Model

SecureVault is designed as a local-only device.

Security features:

- Vault entries are encrypted on-device
- Wi-Fi credentials are encrypted on-device
- TOTP secrets are encrypted on-device
- Master key is derived from the PIN
- Master key is cleared on lock
- Vault RAM is cleared on lock
- Wi-Fi and TOTP secrets are cleared from RAM on lock
- Secrets are loaded only after PIN unlock
- Web portal requires PIN login
- Web portal has brute-force lockout
- Saved Wi-Fi passwords are not shown back in the browser
- Saved TOTP secrets are not shown back in the browser
- Factory reset deletes encrypted vault and encrypted secrets

---

## Web Portal

The web portal runs locally on the ESP32.

Default portal address:

```text
http://192.168.4.1
```

The portal is started from the device UI.

The web portal can manage:

- Vault entries
- Wi-Fi settings
- TOTP accounts
- Backup export
- Backup import

No cloud server is used.

---

## Web Portal Security

The web portal requires the device PIN.

Protection:

- PIN login required
- Wrong PIN attempt counter
- Temporary lockout after too many failed attempts
- Portal timeout after inactivity
- TOTP secrets are not displayed
- Wi-Fi passwords are not displayed

---

## Password Vault

Vault can be managed through the web portal.

Supported actions:

- Add entry
- Edit entry
- Delete entry
- View entries
- Auto-type username/password from device

---

## TOTP

SecureVault supports multiple TOTP accounts.

TOTP accounts are stored encrypted in:

```text
/secrets.enc
```

On the device:

```text
DOWN = next TOTP account
SELECT = sync time or type current code
HOLD SELECT = back
```

Time sync uses Wi-Fi and NTP.

TOTP web portal actions:

- Add TOTP account
- Edit TOTP account
- Delete TOTP account

Saved TOTP secrets are not shown back in the browser.

---

## Wi-Fi Settings

Wi-Fi credentials are used for TOTP time sync only.

Wi-Fi credentials are stored encrypted in:

```text
/secrets.enc
```

Saved Wi-Fi password is not shown back in the browser.

---

## USB HID Auto-Type

USB HID Auto-Type works only in Product Mode.

Supported actions:

- Type username
- Type password
- Type username + Tab + password
- Type TOTP code
- Type generated password

Recommended test:

```text
1. Flash Product Mode
2. Open Windows Notepad
3. Put cursor inside Notepad
4. Trigger auto-type from SecureVault
5. Check typed output
```

Do not test first on real banking, university, email, or production login pages.

---

## Backup Export

Backup export is available from:

```text
Info → Backup → Export Backup
```

The exported backup contains encrypted blocks:

```text
vault.enc
secrets.enc
```

The backup does not expose plaintext passwords or plaintext TOTP secrets.

---

## Backup Import

Backup import is available from:

```text
Info → Backup → Import Backup
```

To confirm import, user must type:

```text
IMPORT
```

Backup import includes rollback protection.

If import fails:

- old `/vault.enc` is restored if available
- old `/secrets.enc` is restored if available
- device tries to reload old data

Expected successful import result:

```text
Write vault.enc: OK
Write secrets.enc: OK
Load vault: OK
Load secrets: OK
Rollback: NOT NEEDED
```

---

## Factory Reset

Factory reset deletes:

- PIN/security preferences
- Vault data
- Encrypted vault file
- Encrypted secrets file
- Wi-Fi settings
- TOTP settings
- Other local preferences

Factory reset returns the device to default state.

Default PIN after reset:

```text
0000
```

---

## Button Navigation

General navigation:

```text
DOWN short = move/change
SELECT short = open/confirm
SELECT long = back
```

TOTP screen:

```text
DOWN short = next TOTP account
SELECT short = sync or type code
SELECT long = back
```

---

## Upload Troubleshooting

If upload fails because COM port disappears:

```text
1. Close monitor with CTRL + C
2. Hold BOOT
3. Press RST once
4. Release RST
5. Keep BOOT pressed for 2 seconds
6. Release BOOT
7. Run pio device list
```

Check port:

```bash
pio device list
```

Upload:

```bash
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
```

If COM port is different, replace `COM3`.

---

## Important Files

```text
src/main.cpp
src/web_portal.cpp
src/vault_model.cpp
src/vault_secure_storage.cpp
src/secrets_secure_storage.cpp
src/totp_settings.cpp
src/totp_manager.cpp
src/wifi_settings.cpp
src/master_key.cpp
src/hid_manager.cpp

include/config.h
include/debug_log.h
include/web_portal.h
include/vault_model.h
include/vault_secure_storage.h
include/secrets_secure_storage.h
include/totp_settings.h
include/totp_manager.h
include/wifi_settings.h
include/master_key.h
include/hid_manager.h
```

---

## Important Commands

Build DEV:

```bash
pio run -e lilygo-t-display-s3-dev
```

Upload DEV:

```bash
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
```

Monitor DEV:

```bash
pio device monitor -e lilygo-t-display-s3-dev --port COM3
```

Build PRODUCT:

```bash
pio run -e lilygo-t-display-s3-product
```

Upload PRODUCT:

```bash
pio run -e lilygo-t-display-s3-product --target upload --upload-port COM3
```

---

## Git Commands

Check status:

```bash
git status
```

Commit changes:

```bash
git add .
git commit -m "Your commit message"
git push
```

---

## Project Roadmap

Already implemented:

- PlatformIO migration
- TFT display setup
- PIN lock
- encrypted vault file
- encrypted secrets file
- web portal login
- web portal brute-force lockout
- add/edit/delete vault entries
- multi-account TOTP
- TOTP device selection
- HID auto-type
- backup export
- backup import with rollback
- DEV/PRODUCT mode separation

Still planned:

- UI polish on device
- Web portal design polish
- better password generator UI
- final security review
- stronger key derivation
- full documentation screenshots
- final product mode testing

---

## Security Warning

This is an experimental security project.

Do not store critical real-life passwords until:

- final security review is complete
- key derivation is improved
- backup/restore is tested thoroughly
- Product Mode is fully tested
- UI is finalized
- recovery behavior is reviewed

---

## License

Not decided yet.