# Slice 4a private screenshot review — raw observations

These are an AI reviewer’s simulated operator observations from the private
1440×900 SIH QGC capture on 2026-09-27. They are not feedback from a human
operator or proof of touch behavior. The images are under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-ui/`.

- `07-fly-idle.png` and `08-follow-options-idle.png`: normal QGC vehicle,
  map/video, and flight controls remain visible. Follower choice is tucked
  into Options and is present only in the PixEagle workflow. The panel is
  fairly tall; the unavailable Smart explanation adds several lines even when
  Classic is selected.
- `09-target-selected.png`: the target is visible and the Follower selector
  appears near Start. The video’s own PixEagle OSD adds headings, crosshair,
  and aircraft text on top of QGC’s telemetry. The busy image competes with
  the controls, especially on a small screen. This is a source/OSD setting
  question, not evidence that an extra QGC overlay should be added.
- `10-follow-active.png`: Stop following is discoverable in the same control
  location, and target changes are disabled during active following. The
  QGC status line and PixEagle OSD both report follow state.
- Critical finding in this first capture: QGC offered Start while PX4 was
  disarmed on the ground. PX4 accepted Offboard, but the UI could mislead an
  operator into believing the aircraft was pursuing the target. The backend
  now requires fresh, matching armed and in-air telemetry before Start; the
  QGC client shows “Take off before following.” The fixed guard passed a new
  client test and a disarmed/takeoff/follow/land SIH probe. A new human capture
  remains part of the operator checkpoint.

No cosmetic change is inferred from these observations. First ask the human
operator whether OSD density and the Smart explanation hinder the actual flow.

## Second feedback implementation — private screen observations

These are coding-agent observations, not independent operator feedback or user
acceptance. Screens: `feedback2-ui/02-settings.png`, `07-dark-placeholder.png`,
`08-default-restored.png`, `05-fly.png`, `09-smart-setup.png`,
`11-model-selected.png`, `12-smart-running.png`, `13-classic-target.png`.

- Dark theme: backend value and default placeholder are dark on the white field
  and readable. The demo aircraft address is populated before typing.
- Collapsed panel: two status rows and Options leave more video visible. The
  follower's readiness reason can wrap; its selected profile stays visible.
- Options: Classic/Smart, tracker or model, and follower are grouped together.
  Initial Smart setup is now possible inline when no model is configured.
- Smart mode hides the Classic dropdown. CPU fallback is reported in Options.
- Target-loss state remains visible, with Cancel still accessible. This moving
  clip did not establish sustained target tracking in this inspection.
- Expanded Options still covers part of the lower video at 1440×900. Mobile,
  smaller-window and physical camera operation are not qualified by these
  captures.

## Third feedback — observed connection transition

Coding-agent observations of private Release captures, not independent human
feedback: `feedback3-ui/02-unverified.png` presents one connection instruction
with Verify vehicle and Settings actions; the unavailable tracker panel is
absent. Clicking Verify vehicle led to `03-verified-video.png`: video and the
compact tracker/follower panel appeared. This directly exercises the previously
missing action from the user's screenshot.
