# SecureVault Product Mode Test Plan

This document is used to test SecureVault in Product Mode before real usage.

---

## Goal

Product Mode enables USB HID auto-type.

In Product Mode, SecureVault can type:

- Username
- Password
- Username + Tab + Password
- TOTP code
- Generated password

Product Mode must always be tested in a safe text field first.

---

## Important Warning

Do not test first on:

- banking websites
- university portals
- email accounts
- real login pages
- production systems

Always test first in:

```text
Windows Notepad
```

---

## Environments

### DEV Mode

Used for development.

```bash
pio run -e lilygo-t-display-s3-dev
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
pio device monitor -e lilygo-t-display-s3-dev --port COM3
```

Expected:

- HID typing disabled
- Debug logs visible
- safer upload behavior

---

### PRODUCT Mode

Used for HID auto-type testing.

```bash
pio run -e lilygo-t-display-s3-product
pio run -e lilygo-t-display-s3-product --target upload --upload-port COM3
```

Expected:

- HID typing enabled
- Debug logs reduced
- device can type into PC

---

## Upload Recovery

If COM port disappears after Product Mode:

```text
1. Hold BOOT
2. Press RST once
3. Release RST
4. Keep BOOT pressed for 2 seconds
5. Release BOOT
6. Run:
```

```bash
pio device list
```

Then flash DEV Mode back:

```bash
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
```

---

## Test 1: Basic Product Mode Boot

Steps:

```text
1. Flash PRODUCT Mode
2. Device boots
3. Enter PIN 0000
4. Main menu appears
```

Expected:

```text
SecureVault unlocks normally.
No crash.
No reset loop.
```

---

## Test 2: Vault Load

Steps:

```text
1. Unlock device
2. Open Vault
3. Open All entries
```

Expected:

```text
Saved vault entries are visible.
If no entries exist, "No entries yet" is shown.
```

---

## Test 3: Web Portal Still Works

Steps:

```text
1. Unlock device
2. Open Settings
3. Start Web Portal
4. Connect PC/phone to SecureVault Wi-Fi
5. Open http://192.168.4.1
6. Login with PIN
```

Expected:

```text
Web Portal opens.
PIN login works.
Vault/TOTP/Backup pages open.
```

---

## Test 4: Username Auto-Type

Preparation:

```text
Open Windows Notepad.
Put cursor into empty Notepad document.
```

Steps:

```text
1. Unlock SecureVault
2. Open Vault
3. Select an entry
4. Choose Username
```

Expected:

```text
Only the username is typed into Notepad.
No extra characters.
No missing characters.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 5: Password Auto-Type

Preparation:

```text
Open Windows Notepad.
Put cursor into empty Notepad document.
```

Steps:

```text
1. Unlock SecureVault
2. Open Vault
3. Select an entry
4. Choose Password
```

Expected:

```text
Only the password is typed into Notepad.
Special characters match the selected keyboard layout.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 6: Username + Tab + Password

Preparation:

```text
Open Windows Notepad or a safe test form.
```

Steps:

```text
1. Unlock SecureVault
2. Open Vault
3. Select an entry
4. Choose User+Tab+Pass
```

Expected:

```text
Username is typed.
TAB is sent.
Password is typed.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 7: TOTP Code Auto-Type

Preparation:

```text
Open Windows Notepad.
Put cursor into empty Notepad document.
Make sure TOTP time is synced.
```

Steps:

```text
1. Unlock SecureVault
2. Open TOTP
3. Select a TOTP account with DOWN
4. Wait until code is visible
5. Press SELECT
```

Expected:

```text
6-digit TOTP code is typed into Notepad.
The typed code matches the display.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 8: Generated Password Auto-Type

Preparation:

```text
Open Windows Notepad.
Put cursor into empty Notepad document.
```

Steps:

```text
1. Unlock SecureVault
2. Open Add / Generator
3. Generate password
4. Press SELECT to type it
```

Expected:

```text
Generated password is typed into Notepad.
All characters are correct.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 9: Keyboard Layout US

Steps:

```text
1. Settings → Keyboard
2. Set layout to US
3. Type password with special characters into Notepad
```

Expected:

```text
Characters match US keyboard layout.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 10: Keyboard Layout DE

Steps:

```text
1. Settings → Keyboard
2. Set layout to DE
3. Type password with special characters into Notepad
```

Expected:

```text
Characters match German keyboard layout as much as supported.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 11: Lock Clears Data

Steps:

```text
1. Unlock device
2. Load Vault and TOTP
3. Lock device
4. Unlock again
```

Expected:

```text
After lock, device requires PIN again.
After unlock, vault and secrets reload successfully.
```

Expected monitor in DEV mode:

```text
Load encrypted secrets: OK
Load encrypted vault: OK
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 12: PIN Change Re-Encryption

Steps:

```text
1. Unlock with current PIN
2. Settings → Change PIN
3. Change PIN
4. Reset device
5. Unlock with new PIN
```

Expected:

```text
Vault loads with new PIN.
Secrets load with new PIN.
Old PIN no longer works.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 13: Backup Export

Steps:

```text
1. Start Web Portal
2. Login
3. Info → Export Backup
4. Copy backup text
```

Expected:

```text
Backup contains encrypted vault.enc and secrets.enc blocks.
No plaintext passwords.
No plaintext TOTP secrets.
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 14: Backup Import

Steps:

```text
1. Export backup
2. Import same backup
3. Type IMPORT
4. Submit
```

Expected:

```text
Write vault.enc: OK
Write secrets.enc: OK
Load vault: OK
Load secrets: OK
Rollback: NOT NEEDED
```

Result:

```text
PASS / FAIL:
Notes:
```

---

## Test 15: Return to DEV Mode

After Product Mode testing, flash DEV Mode back:

```bash
pio run -e lilygo-t-display-s3-dev --target upload --upload-port COM3
```

Expected:

```text
Device returns to DEV Mode.
HID typing disabled.
Monitor works.
```

---

## Final Product Mode Checklist

Before real use:

```text
[ ] Product build succeeds
[ ] DEV build succeeds
[ ] Upload recovery works
[ ] Username auto-type works
[ ] Password auto-type works
[ ] User+Tab+Pass works
[ ] TOTP code auto-type works
[ ] Generated password typing works
[ ] US layout tested
[ ] DE layout tested
[ ] PIN change re-encryption tested
[ ] Backup export tested
[ ] Backup import tested
[ ] Factory reset tested
[ ] No plaintext secrets shown in Web Portal
[ ] README updated
[ ] SECURITY_ROADMAP updated
```
