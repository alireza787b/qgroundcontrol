# Live native video diagnostic

`PixEagleLiveVideoTest` checks a real backend through QGC's authenticated client,
WebSocket JPEG source, GStreamer decoder, QVideoSink and presented PixEagle
surface. It reads video/status only and sends no tracking, camera or aircraft
commands. The test is labelled `Network`, excluded from normal CI, and skips
unless explicitly configured. A successful Dashboard stream alone does not
prove this native path.

Use a build with `QGC_BUILD_TESTING=ON`. Create a private JSON file outside the
repository containing `endpoint`, `username` and `password`; protect it with
file permissions. Do not paste its contents into logs or bug reports.

```bash
PIXEAGLE_VIDEO_DIAGNOSTIC_CREDENTIALS=/private/path/credentials.json \
PIXEAGLE_VIDEO_DIAGNOSTIC_OUTPUT=/tmp/pixeagle-native-video.json \
PIXEAGLE_VIDEO_DIAGNOSTIC_IMAGE=/tmp/pixeagle-native-video.png \
  ctest --test-dir build/pixeagle-custom-debug-latest2 \
  --output-on-failure -R '^PixEagleLiveVideoTest$'
```

CTest uses the repository's isolated offscreen software-rendering environment.
For an installed debug build on Windows, set the same environment variables
and run `PixEagle-QGroundControl.exe --unittest:PixEagleLiveVideoTest`. A release
build without the test framework cannot run this diagnostic.

The JSON report contains authentication/context readiness, decoded frame count,
dimensions, frame-handle type and presented-frame identity/freshness. It omits
the cookie, password and selection token. An optional PNG records the rendered
surface and may contain a private camera scene; review it before sharing.

Compare outcomes by stage: failed authentication/context, no decoded frame,
decoded frame without presentation, or fresh presentation. A Linux pass narrows
the investigation; it does not establish Windows decoder/driver compatibility
or physical display latency. This test does not alter backend configuration.
