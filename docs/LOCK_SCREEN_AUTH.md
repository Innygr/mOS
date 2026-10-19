# mOS lock screen and authentication foundation

## Included in the current foundation

- Explicit `SetupRequired`, `Locked`, and `Unlocked` states.
- Credential-verifier interface for PIN/password verification without storing credentials in the UI or state machine.
- Configurable failed-attempt cooldown (default: five failures, 30 seconds).
- Lock-on-screen-sleep policy (enabled by default).
- Optional biometric-provider interface. Biometrics are unavailable unless a real provider reports itself available.
- Host tests using fake verifier and biometric providers.

## Security boundary

`AuthManager` is a policy/state machine, not a complete secure credential store. The firmware integration must provide an `ICredentialVerifier` that stores a salted password hash using an appropriate password-hashing construction, compares in constant time where practical, and never logs entered credentials. Do not persist raw PINs/passwords or treat a plain fast hash as sufficient password storage. Credential setup/change needs a separate confirmation flow and recovery policy before being exposed in Settings.

## Touch ID / fingerprint support

The current watch hardware does not include a verified fingerprint sensor. The generic `IBiometricProvider` seam is for future hardware; it does not claim Apple Touch ID compatibility and biometrics stay unavailable until a sensor adapter is actually integrated. The CST816S touchscreen detects touch, not a person's fingerprint identity, so it cannot itself implement Touch ID.

## UI integration still required

The shell should route boot/wake to the lock screen when credentials are configured, block launcher/apps and notifications with private content until unlocked, and call `onScreenSleep()` on display sleep. Settings should offer PIN/password setup and lock preferences. The initial lock screen should use the existing Canvas and keyboard abstractions. These UI and firmware wiring tasks are not completed by adding the core state machine alone.
