# Windows installation and video readiness

This guide covers the private PixEagle QGC x64 installer on Windows 10/11.
It does not describe installing the PixEagle backend on Windows.

## Install and connect

1. Install the supplied x64 EXE and use the artifact manifest to verify its
   version and checksum. Keep the installed directory intact.
2. Check Windows edition in **Settings → System → About**. Windows N editions
   need Microsoft's [Media Feature Pack](https://support.microsoft.com/en-us/windows/experience/platform-variants/media-feature-pack-for-windows-n).
   Install it through Windows Optional Features, then **restart Windows even
   if the installer does not request a restart**. Other editions normally
   include these media components; do not install an N-only pack on them.
3. Enable PixEagle in QGC Settings, enter the reachable backend host and port,
   and sign in. Use HTTPS for deployment; the existing HTTP bench option is
   for an explicit trusted-lab setup. The dashboard uses the same host on
   port 3040 unless overridden.
4. Confirm moving video in Fly View. No aircraft is required for video and
   tracking. Sign-in success alone does not establish video readiness.

The inspected installer at `30c6a1528` bundles Qt multimedia plugins, FFmpeg,
Visual C++ runtime DLLs, GStreamer libraries and plugins. Windows media system
components remain an operating-system prerequisite. Both bundled Qt multimedia
plugins import Media Foundation DLLs, even though Qt normally selects its
[FFmpeg backend](https://doc.qt.io/qt-6.11/qtmultimedia-index.html).
QGC configures its bundled GStreamer plugin paths and prepends its application
directory for scanner dependencies.

**A separate GStreamer SDK installation is not a normal end-user requirement.**
Developer builds use the SDK described in QGC's build tooling. Do not copy DLLs
from another QGC version or change plugin paths to repair an installed package.

## If video remains unavailable

After enabling or repairing Windows media features, restart Windows and reopen
QGC before making further changes. If the problem remains, use
[the diagnostic launcher](validation/collect-windows-video-debug.ps1):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\collect-windows-video-debug.ps1
```

Copy the script to Windows first. It opens QGC and records OS edition, Media
Foundation startup, plugin-loader messages, installation hashes and video logs
in a timestamped Desktop folder. Reproduce the failure, then close QGC normally
and return the diagnostic ZIP. For multiple installations, pass `-Executable`
with the full path to the intended `PixEagle-QGroundControl.exe`.

`No QtMultimedia backends found`, `QVideoSink "Not available"`, or Media
Foundation initialization failure indicates a local video-runtime problem.
A working browser dashboard does not exclude that problem. Transport/auth/TLS
errors require their own diagnosis; do not change backend policy to repair a
missing client video sink.

If diagnostics identify missing or damaged Visual C++ dependencies, repair the
application installation or use Microsoft's
[Visual C++ v14 x64 Redistributable](https://aka.ms/vc14/vc_redist.x64.exe).
Match the x64 application architecture; follow any restart request. This is
a repair option, not evidence that every installation needs another runtime.

## Recorded tablet result, 7 October 2026

The operator installed the latest QGC, Windows media features, Visual C++
Redistributable and standalone GStreamer. Video still failed before restarting
Windows and worked after reboot with the camera available again. Earlier logs
showed missing Qt multimedia backends and Media Foundation initialization
failure. Media-feature activation after reboot is the leading explanation.

The exact Windows edition/feature and post-repair loaded modules were not
captured. Multiple installations and the camera restart prevent attributing
the recovery to one component conclusively. Record this as operator-confirmed
video recovery after a combined repair, not proof that standalone GStreamer
is required or that all Windows hosts and reconnect scenarios are qualified.
See [the handoff evidence](WINDOWS-HANDOFF-2026-10-06.md) for provenance.
