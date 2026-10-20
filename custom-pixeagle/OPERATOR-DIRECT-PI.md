# Direct Pi operator bench

Use the Pi's normal supervised service for a GCS/tablet test. An isolated native
qualification replay is a different backend: its loopback port, temporary login
and SSH forwarding do not configure or start the normal service.

For the October 20 installation:

- QGC backend: `http://192.168.0.226:5077`.
- Dashboard: `http://192.168.0.226:3040`.
- The existing operator-requested bench account is `admin` / `admin`.
- Saved camera engine, vertical mount and `gm_velocity_vector` are preserved.
- Circuit breaker is confirmed on for the bench; no aircraft action starts on
  login or reconnection. Camera controls/tracking can be tested independently.

These addresses and credentials describe this private bench, not application
constants. Other installations use their configured host and account. No SSH
tunnel or access to the camera subnet is required from the GCS.

The Pi updater applied backend `ab7f35f` from `feature/video-quality-adaptive`
through the standard stopped-runtime transaction. Existing source-built
OpenCV/GStreamer and Full AI were preserved/reconciled. Normal systemd startup
uses that checkout; the isolated media benchmark is stopped. The branch is a
qualification candidate, not a new stable-main release.

The normal backend still advertises native WebRTC as unqualified. QGC uses its
advertised authenticated JPEG fallback; the browser may use browser WebRTC.
Do not describe direct tablet playback as the same native transport used by
the experimental Linux replay, or as native Windows acceptance.

## Sign out and changing endpoints

Sign out must remain available while login, verification or context refresh is
pending. It cancels the pending request, invalidates its generation and suppresses
automatic sign-in recovery. A late reply cannot authenticate or verify the
retired session. An authenticated session uses the existing bounded server
revocation path; repeated Sign out cannot create duplicate logout requests.

The cancellation regression covers pending login, context and verification,
duplicate Sign out and late replies. The local build, changed-file lint and
all 471 Unit/Integration tests passed with standard Flaky/Network exclusions.
Existing installers need the updated build to receive this UI behavior.

## Operator check

1. Sign out, enter the direct backend address, then sign in with the Pi account.
2. Open Fly View and confirm video plus Camera Classic/Camera Smart choices.
3. Test acquisition, retargeting and camera movement/release/Stop on the bench.
4. Confirm commands remain blocked. Aircraft-following qualification is separate.

Retain the Pi's configuration backup/receipt and service logs with the operator
result. Authentication/API success alone does not prove tablet decoding or
presentation; that remains the physical operator check.
