# PixEagle platform CI

The custom platform workflow is `.github/workflows/pixeagle-platform.yml`. It
is deliberately manual: stock QGroundControl workflows remain unchanged, and
custom artifacts are not produced or published on ordinary stock builds.

After the integration branch is pushed to the QGC fork, open **Actions →
PixEagle Platform Artifacts → Run workflow** and select:

- `all` to build Linux, Windows and Android;
- `linux`, `windows` or `android` for one platform; and
- `Release` for a release-shaped unsigned/test artifact, or `Debug` for a
  faster diagnostic build.

The workflow uses QGC's pinned build configuration, locked Qt setup actions,
the explicit `QGC_CUSTOM_DIR=custom-pixeagle` overlay, and the platform runners
already used by QGC. It uploads short-lived CI artifacts with SHA-256 sidecar
files. Release builds receive GitHub build-provenance attestations.

Android CI creates a temporary debug keystore for the test APK. It does not use
a production signing key, Play Store credentials, or store deployment. The APK
must be re-signed through a separately reviewed release workflow before any
distribution.

The workflow does not upload to AWS, create a GitHub Release, or alter the
stock QGC package. Linux packages an x86_64 AppImage; Windows installer
verification runs on the native Windows runner. Android runs Gradle lint and
packages the configured PixEagle application ID
`io.github.alireza787b.pixeagle.qgroundcontrol`.

The 2026-10-05 qualification runs were dispatched from
`feature/pixeagle-native-integration-upstream-2026-10-05`:

- Run `37280978250` passed Linux and Android. Android forwards
  `QGC_CUSTOM_DIR` to Qt's per-ABI sub-builds.
- Run `37295499089` passed Windows after fetching QGC version tags before CPack.

The Windows installer and Linux AppImage are unsigned test artifacts. The
Android APK uses a temporary CI debug keystore. None is a store or production
release artifact.

If the workflow fails, retain the run URL, commit SHA, selected inputs, runner
image, artifact checksums, and logs in the release checkpoint. A successful CI
build proves compilation and packaging on that runner; it does not replace
touch, PiP, suspension, credential-store, camera, Raspberry Pi, or onboard
ground acceptance.
