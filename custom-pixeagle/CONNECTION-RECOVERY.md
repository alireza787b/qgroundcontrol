# Sign-in and vehicle connection recovery

The sign-in form starts with `admin/admin`, matching the beginner PixEagle
lab installation. Existing edited or securely remembered account credentials
take precedence. This is a form default, not an automatic attempt to authenticate
with factory credentials on an arbitrary endpoint.

QGC retains the current address, username and password in the current session
when the first vehicle appears or the last vehicle disappears. An aircraft's
saved endpoint takes precedence over an inherited companion address once its
UID arrives; an address explicitly edited before that UID is retained instead.
Additional vehicles remain separate connections and require their own verified
association. A matching system ID alone does not establish aircraft identity.

Vehicle identity/link changes invalidate aircraft verification and pending
operator actions, while preserving the backend's authenticated HTTP session.
Read-only backend polling continues while the vehicle is offline, so camera
video can remain available. Single-vehicle verification resumes automatically
when fresh matching backend and QGC aircraft identities return. Identity
conflicts and duplicate associations still block flight operations.

Expired backend sessions automatically retry sign-in using credentials from
the last successful sign-in in this QGC session. Retry delays grow from two
seconds to a maximum of fifteen seconds during transient connection failures.
Default form credentials and unaccepted edits are never used for recovery.
Credential refusal, policy refusal and TLS errors remain explicit. Explicit
sign-out, disabling the integration or changing endpoints cancels recovery.

Passwords are retained only in session memory and, when Remember sign-in is
enabled, the existing system password store. They never enter QSettings or
plaintext configuration. Remembering is enabled by default; an unavailable
system store does not introduce a plaintext fallback. Across application
restarts, secure remembered credentials are required for automatic sign-in.

Recovery refreshes authoritative status and restarts video as needed. It never
replays target selection, camera movement, following, configuration changes or
flight commands. Backend restart also remains an explicit guarded action.

## Validation and operator checkpoint

Regression tests cover initial UID arrival during login, offline/online vehicle
transitions, backend session expiry, accepted and rejected recovery credentials,
explicit sign-out cancellation, endpoint inheritance/restoration and raw UID
conflicts. The same UI uses QGC controls and preserves edited fields without
clearing them on ordinary client status updates.

For the motorless Pixhawk bench, leave **Block PixEagle flight commands** enabled.
Check sign-in/video, then power or disconnect/reconnect the vehicle link. The
address and account should remain, video/status should recover, and a single
matching vehicle should verify without another click. No aircraft is needed
for camera-only video/tracking. Unexpected failures need the matching QGC
version, approximate time and diagnostic logs.

## Closeout evidence

The 2026-10-07 local Debug build succeeded. All eight PixEagle test suites
passed, including authentication recovery and vehicle identity transitions.
The broader Unit/Integration run with `Flaky|Network` exclusions passed 429
of 430 tests; `BluetoothWorkerTest` reproduced the previously documented
local BlueZ failure. This is a baseline blocker, not a passing full-suite claim.
Changed-file lint passed using the locked developer tools.

Hosted Linux Debug qualification uses the actual `custom-pixeagle` overlay
and the standard Unit/Integration exclusions, retaining StockUI coverage.
The stock Custom Build workflow continues to qualify `custom-example`; it is
not evidence for this native overlay. The platform workflow records exact
source, shared upstream version anchor and checksums for each artifact.
Private qualification tags are excluded from that package-version selection;
stock QGC version selection remains unchanged.

PixEagle 7.3.0 corrects misleading periodic PX4 connection logging using the
canonical connection status. It does not change gains, limits, factory
defaults or the private camera profile. The Pi/Pixhawk audit found matching
MAVSDK/telemetry UID and fresh disarmed telemetry, enabled and confirmed
flight-command blocking, and disabled a backed-up, broken legacy dashboard
service while preserving the live MAVLink router. No aircraft command or
camera movement was sent by the audit. Backend release CI, source tag and
Pi deployment provenance are tracked separately from QGC binary provenance.
