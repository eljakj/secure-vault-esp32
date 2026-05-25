# SecureVault Security Roadmap

This document tracks security-related tasks that should be completed before using SecureVault with critical real-life passwords.

---

## Current Security Status

Implemented:

- PIN-based unlock
- Encrypted vault storage in `/vault.enc`
- Encrypted secrets storage in `/secrets.enc`
- Wi-Fi credentials encrypted
- TOTP secrets encrypted
- Master key cleared on lock
- Vault RAM cleared on lock
- Wi-Fi/TOTP RAM cleared on lock
- Secrets loaded only after unlock
- Web portal protected by PIN login
- Web portal brute-force lockout
- Web portal hides saved passwords/secrets
- Backup export uses encrypted blocks
- Backup import has confirmation and rollback
- Factory reset deletes vault and secrets
- DEV/PRODUCT mode separation

---

## Important Warning

SecureVault is still an experimental security project.

Do not store critical real-life passwords until the remaining items below are reviewed and tested.

---

## High Priority

### 1. Improve Key Derivation

Current state:

- Master key is derived from the PIN.
- This is functional but should be hardened.

Planned improvement:

- Use PBKDF2 or another proper KDF.
- Add random salt.
- Store salt separately.
- Use enough iterations for ESP32-S3 performance.
- Support migration from old key derivation.

Goal:

```text
PIN + Salt + KDF → Master Key
```

---

### 2. Add Backup Compatibility Check

Current state:

- Backup import writes encrypted files and tries to load them.
- Rollback protects old files if loading fails.

Planned improvement:

- Add backup metadata.
- Include version number.
- Include device/project identifier.
- Include KDF version.
- Include salt handling.

Goal:

```text
Backup import should clearly tell why a backup cannot be loaded.
```

---

### 3. Product Mode Full Test

Current state:

- Product Mode enables USB HID.
- Product Mode has been tested manually.

Planned checklist:

- Username auto-type
- Password auto-type
- Username + Tab + Password
- TOTP code auto-type
- Generated password auto-type
- DE keyboard layout
- US keyboard layout
- Special characters
- Upload recovery after Product Mode

---

### 4. Web Portal Security Review

Current state:

- Web portal uses PIN login.
- It has brute-force lockout.
- It runs only locally on ESP32 AP.

Planned review:

- Prevent cached pages from showing after logout.
- Add no-cache headers.
- Confirm every sensitive route checks login.
- Review import/export routes.
- Review session behavior.
- Review timeout behavior.

---

### 5. Memory Cleanup Review

Current state:

- Master key is cleared on lock.
- Vault RAM is cleared on lock.
- Wi-Fi/TOTP RAM is cleared on lock.

Planned review:

- Check temporary String usage for secrets.
- Reduce long-lived plaintext String objects.
- Clear buffers after encryption/decryption where possible.
- Review backup import/export memory behavior.

---

## Medium Priority

### 6. Stronger PIN Policy

Current state:

- 4-digit PIN.
- Brute-force lockout exists.

Possible improvements:

- Optional 6-digit PIN.
- Optional longer passphrase.
- More strict lockout.
- Exponential lockout delay.

---

### 7. Encrypted Backup Password

Current state:

- Backup contains encrypted `/vault.enc` and `/secrets.enc`.
- Backup depends on current PIN/key compatibility.

Possible improvement:

- Export backup encrypted with separate backup password.
- Allow restoring backup after changing device PIN.
- Use dedicated backup KDF.

---

### 8. Tamper / Corruption Detection

Current state:

- Decryption failure is detected.
- DEV mode can recreate test vault/secrets.
- Product mode keeps files.

Possible improvements:

- Stronger file headers.
- Versioned encrypted payloads.
- Clear error screen on device.
- Better recovery messages in web portal.

---

## Low Priority

### 9. Audit Logs

Optional:

- Count failed unlock attempts.
- Count web portal failed logins.
- Show last successful unlock time if RTC/NTP available.

Do not store sensitive data in logs.

---

### 10. Security Documentation

Add documentation for:

- Threat model
- What SecureVault protects against
- What SecureVault does not protect against
- Factory reset behavior
- Backup risks
- PIN loss behavior

---

## UI / Design Comes After Security

Final design should be done after the security core is stable.

Planned later:

- Device UI polish
- Web portal redesign
- Mobile-friendly layout
- Better backup UI
- Better TOTP account management UI
- Icons and visual hierarchy
