# Windows video handoff and connection recovery

The Windows handoff archive was inspected on Linux. Its SHA256 is
`b330a9163d9ff62d9d7112cd91b7902e5ceeb4cc43a9be255a15cb821961d621`.
The report records live Windows QGC video at revision `0d8bdf06b` after
repairing Raspberry Pi browser CORS and signing out/in. This establishes a
working Windows stream on that machine; it does not qualify every GCS.

The old diagnostic command enabled all Qt debug categories. The Windows
stack showed the Qt Quick dirty-item dump creating deferred properties on
the render thread and causing a fatal cross-thread popup connection.
The logging fix disables that category for QGC debug/trace logging. Explicit
Qt environment rules retain precedence, as documented in
[Qt logging rules](https://doc.qt.io/qt-6/qloggingcategory.html).
The logging regression runs without the higher-priority CTest logging
restriction so it tests the application's rules rather than the test harness.

## Read-only recovery

Transient connection-context failures clear stale media/association state and
retry GET reads with delays of 2, 4, 8 and at most 15 seconds. A successful
fresh response restores normal two-second polling. No target, camera gesture,
following command or configuration mutation is replayed. Sign-out, endpoint
replacement and disabled integration cancel recovery. Authentication, policy
and TLS failures remain explicit; Settings offers Reconnect when authenticated
context is missing. Aircraft association still requires the normal identity
checks. An external backend restart does not automatically resume operations.

## Diagnostic launcher

Use `validation/collect-windows-video-debug.ps1`. It selects the custom
executable, accepts an explicit path, disables the unsafe Qt dump, and writes
separate stdout/stderr files plus binary checksum and exit status into a
unique Desktop diagnostics folder. Install the matching installer first.
The script has been reviewed on Linux; actual PowerShell execution remains
part of the Windows operator retest.

## Evidence boundary

Local checks and hosted runs are recorded below when complete. The next
operator test is Windows GCS sign-in/video, transient interruption/recovery,
and supervised backend restart. No aircraft is required for video/tracking.

## Linux closeout checks

The local Debug build succeeded. The final `PixEagleClientTest` and
`LogManagerTest` rerun passed (53.37 seconds), including companion/vehicle
context recovery, permission refusal without automatic retry, cancellation
on sign-out, and the offline Reconnect guard. Changed-file pre-commit gates
passed, including Qt 6.11.1 qmllint, C++ formatting and CMake formatting.

The broader `Unit|Integration` run with `Flaky|Network` exclusions exercised
430 tests: 428 initially passed. `CMake.QGCTestMultiConfig` then passed with
the locked tools directory added to PATH for Ninja. The remaining unchanged
`BluetoothWorkerTest` fails locally on an unexpected BlueZ invalid-address
warning; that same test passed in hosted run `37511039115`. No Bluetooth
source was changed or warning suppressed. All eight PixEagle suites and the
stock-UI coverage passed. This is a recorded local baseline blocker, not an
all-green full-suite claim.

Authenticated reads from the Pi at `192.168.137.161:5077` returned 200 for
connection and backend configuration. Its service was active and enabled.
A preflight carrying the robot-subnet dashboard Origin returned 200 with
credentialed CORS through Wi-Fi. Direct robot-subnet delivery and the new
Windows binary remain operator acceptance checks.

## Cross-platform version provenance

Hosted native build/test run `37518498245` passed at `490653960` and all
three packaging jobs in `37518492369` succeeded. Downloaded checksums matched;
the Linux AppImage started and returned its version. That check exposed
different version metadata: Windows fetched upstream tags, while Linux and
Android could fall back to a bare commit identifier.

All custom platform jobs now resolve the same upstream `v5.2.0-dev` annotated
tag, verify its pinned object and ancestry, and include `pixeagle-version.txt`
in their artifact. This changes version provenance only. A fresh all-platform
run is required before replacing the Desktop bundle. Android still uses a
temporary CI signing key; it is a private test artifact, not a signed public
release or a guaranteed in-place upgrade.
