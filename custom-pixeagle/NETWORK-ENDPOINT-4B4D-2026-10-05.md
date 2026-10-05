# Network endpoint and camera-control recovery checkpoint

This checkpoint records the final repair applied after the Raspberry Pi bench
session. It is based on the QGC commit that contains this checkpoint on
`feature/pixeagle-native-integration-upstream-2026-10-05`.

## Operator-visible fixes

- QGC accepts an explicitly entered `http://` backend on a private bench or
  trusted LAN, including an IP address or hostname. Credentials, query strings,
  fragments, malformed paths and unsupported schemes remain rejected. HTTPS is
  required for an untrusted network.
- A failed sign-in keeps the endpoint, username and password in the active form
  so the operator can correct the credentials. The password is cleared after a
  successful authenticated state, on sign-out and when the active client
  changes.
- The dashboard link continues to derive its host and protocol from the
  backend endpoint and uses port 3040 by default. An explicit dashboard URL is
  still available for a proxy or different port; it is not duplicated in the
  backend settings.
- After authentication, video and target tracking are available in a
  companion-only session when the backend advertises the unbound tracking
  capability. A verified aircraft association is still required before QGC
  can start following or dispatch any PixEagle aircraft command. This keeps
  camera/video bench tests useful without weakening aircraft-command safety.
- A camera command rejected because its source, target or camera generation
  changed is treated as synchronization. QGC retires the old gesture, refreshes
  the authoritative camera guard and shows a short refresh state. It does not
  present a false motor fault or send a redundant stop. Genuine transmission,
  permission and unknown-outcome errors remain visible.

## Verification

The local Release/Debug source build links successfully with the configured Qt
6.11.1 toolchain and the host EGL library. The eight PixEagle QGC tests pass,
including the media-without-aircraft and typed camera-context conflict
regressions. The exact focused
commands were:

```text
cmake --build build/pixeagle-custom-debug-latest -j2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/pixeagle-custom-debug-latest \
  -R '^PixEagle' --output-on-failure
```

The repository pre-commit checks pass for C++, QML changes and safety checks
except that this workstation does not have `qmllint` installed; the hook
reports that missing tool rather than a QML diagnostic. Hosted platform builds
must be taken only from the workflow run for this exact commit.

## Pi connection reminder

For the commissioned Pi, use the reachable interface address (for example
`http://192.168.1.160:5077` from the Wi-Fi GCS path, or the `192.168.0.226`
address when the GCS is routed to that LAN). Do not use the Dashboard port for
native QGC sign-in. The current Pi service, CORS allowlist and GStreamer RTSP
profile were verified separately; credentials and raw logs remain private.
