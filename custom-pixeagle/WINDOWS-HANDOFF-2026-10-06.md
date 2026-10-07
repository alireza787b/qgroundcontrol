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
The script also records Windows edition/build, Media Foundation DLL presence
and startup result, installation hashes and `qt.conf`. Plugin-loader tracing
is enabled before application startup. QGC's global `info` override is omitted
because it hid the requested transport debug messages in the first capture.
The script restores its diagnostic environment overrides when QGC exits.
Actual Windows PowerShell execution remains part of the operator retest.

## Tablet diagnostics, 7 October

`pixeagle-qgc-diagnostics-20261007-113226` records `No QtMultimedia backends
found`, repeated `Failed to create QVideoSink "Not available"`, and a Windows
Media Foundation initialization failure. Without a platform video sink,
Qt's [QVideoSink implementation](https://github.com/qt/qtmultimedia/blob/v6.11.1/src/multimedia/video/qvideosink.cpp)
does not deliver submitted frames. This establishes a client-side video
failure independently of any additional network problem.

The installed executable SHA256 is
`e220640ba2a02133200620f3f16e12597649255c1d9e3c440739d5b14278f41d`.
It matches the executable extracted from the `30c6a1528` installer. Extraction
also confirmed both multimedia plugins, FFmpeg and Visual C++ runtime DLLs,
and `bin/qt.conf` with `Prefix = ..`. Therefore this archive is not missing
those packaged components; the actual tablet installation still needs to be
checked against those files.

Both Windows multimedia plugins import Windows Media Foundation DLLs.
Missing Media Foundation, damaged dependencies or plugin-loading configuration
remain candidates; the first capture does not identify which dependency failed.
Use the updated launcher to obtain Qt's exact plugin-loader refusal and the
tablet's system information before changing packaging or OS components.
For a confirmed Windows N installation lacking media features, use
[Microsoft's Media Feature Pack instructions](https://support.microsoft.com/en-us/windows/experience/platform-variants/media-feature-pack-list-for-windows-n-editions).
Do not infer an N edition from the current logs alone.

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
in their artifact. This changes version provenance only. All three jobs in
run `37563660848` passed at `30c6a1528`, with common version
`v5.2.0-dev-170-g30c6a1528`. Windows and Linux downloads matched their checksums;
the Linux AppImage passed its version/boot check. Windows tablet video acceptance
remains open despite packaging CI success. Android still uses a
temporary CI signing key; it is a private test artifact, not a signed public
release or a guaranteed in-place upgrade.
