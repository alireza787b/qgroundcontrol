# Slice 4b.4 — v13 closeout

## Position

The v13 repair is implemented and the backend, Dashboard, QGC and isolated
SIH gates pass. The fresh command-blocked handoff is ready; the new operator gate
has not passed. Earlier camera-v5/v6 tracking/control acceptance remains
separate from the changed guidance path. No real-aircraft claim is made.

## V12 operator evidence

The private v12 traces contain 1,046 successful Offboard publications. Two
Vector retargets restored guidance in approximately 1.5 and 1.8 seconds with
matching simulated yaw changes. Chase following produced yaw/altitude changes,
but no active Chase retarget was captured. The unexpected stop around 07:21:17
Tehran time followed an invalid edge tap, not recovery-budget exhaustion.

The application entered continuity recovery before the adapter rejected the
selection. Shutdown then briefly reported an unhealthy publisher although all
recorded sends succeeded. Retarget authority clamping could abruptly reduce a
command, startup elapsed time could accelerate Vector, and processing timestamps
could mask older angle samples. These findings motivated the approved repair.

## Changed behavior

- Validate/reuse camera selections before generation or ownership changes.
  Invalid edge selections preserve the current target/follower.
- Serialize shutdown and retain its authoritative session/aircraft-bound result.
- Use actual camera measurement timestamps/sequences, post-selection evidence,
  bounded monotonic timing, body-line-of-sight filtering and final command slew.
- Budget recovery against commands actually submitted, after shaping.
- Fresh gimbal defaults use bounded recovery: 8 seconds/4 metres, 0.5-second
  confirmation/restoration, 70% provisional angle blend and half-authority new
  guidance. Vector starts with coordinated turn; gains/limits are unchanged.
- Preserve explicit saved/legacy configurations; adopt new defaults through
  inactive Config Sync. Other followers retain their existing policies.
- Keep the accepted QGC layout and use clear transition labels. Advanced
  installation configuration remains in the Dashboard.

## Verification

| Gate | Result |
| --- | --- |
| Backend Unit, standard non-hardware markers | 3,304 passed; 41 optional-dependency skips; 14 subtests passed |
| Backend Integration, same marker exclusions | 185 passed |
| Continuity/schema/profile focused checks | 117 passed |
| Camera/native focused checks | 133 passed after final shutdown guard |
| Video-source/hygiene regression | 86 passed |
| Route/configuration focused checks | 81 passed |
| API inventory/schema | Generated contracts checked; schema up to date |
| Final smoothing-help schema/config checks | 94 passed; 1 optional skip |
| CI route/docs/config/binary/remote-browser contracts | 154 passed |
| Additional security/config/Dashboard contracts | 47 passed; 1 optional skip |
| Dashboard | 489 tests in 64 suites; build and lint passed |
| QGC debug build / focused PixEagle client | Passed |
| QGC Unit/Integration, excluding Flaky/Network | 413/413 passed in 484.43 seconds |
| Blocked capture/inference Stop timing | Stop 2.75–2.80 ms; lease expiry 367.51–369.70 ms; all four gates passed |

Backend commands match CI's `PYTHONPATH=src .venv/bin/python -m pytest`
with `-m 'not slow and not sitl and not px4 and not e2e and not hardware and not manual'
--strict-config`. QGC uses the pinned developer-tool PATH and:

```bash
PATH="$PWD/tools/.venv/bin:$PATH" ctest --test-dir build/pixeagle-custom-debug \
  --build-config Debug --output-on-failure -L 'Unit|Integration' \
  -LE 'Flaky|Network' --parallel 2 --no-tests=error
```

The final source includes four extra regressions for clearing inertial coast
lateral compensation within the same three-axis acceleration budget. A strict
SIH assertion exposed this small loss-to-guidance overshoot after the first
backend pass; it was repaired without changing any limit or gain, and the
full backend/SIH evidence was renewed.

The initial QGC run had a short GPS UI timeout and missing Ninja PATH;
focused investigation passed both, followed by the complete corrected-env
413-test pass. The initial backend run exposed two outdated camera-source test
fixtures; they now use genuine pure SIP selection preparation and still prove
that unrelated UDP video cannot authorize camera selection. Test-hygiene stubs
were corrected. Final combined runs include these fixes.

Source-only QML lint, touched C++ format/null guards, Python fatal-error lint,
schema check and diff-whitespace checks pass. The known broader upstream/import
lint blockers in [BASELINE.md](BASELINE.md) remain separate release debt.
Build-aware QML lint also retains the known unresolved QGCCorePlugin type
warning; it is not claimed green.

Regression logs are preserved under the 2026-10-01 slice-4b4 private cache:
`v13-backend-unit-qualified.log`, `v13-backend-integration-qualified.log`,
`v13-api-inventory-current.log`, `v13-ci-contract-gates-final.log`,
`v13-security-config-dashboard-contracts.log`, `v13-schema-final.log`, Dashboard logs and
`v13-camera-stop-timing.log`. The QGC complete log/JUnit artifact is copied
there at closeout. Failed/interrupted earlier runs remain available.

The private profile is `camera-sih-operator-v13`, instance
`gimbal-sih-8731fee94510e2e1`. Its manifest includes all 190 copied file
checksums; source content matches the tested worktree. Commands start blocked,
and profile preparation starts no services. Debug binary SHA-256:
`0c968d8ed28f8f685c9a3f4dbc2d2f46ba4379c2c268946d3b41df9cfae68dd2`.

Isolated camera-free SIH uses
[run-gimbal-sih-evidence.py](validation/run-gimbal-sih-evidence.py).
Each run snapshots sources/configuration and owns labelled Docker containers
with only loopback and no exposed host ports. Production guidance, continuity,
shaping and publication consume an independent world-target fixture and are
checked against PX4 pose. The runner asserts actual published yaw/vertical
signs, configured slew limits, confirmed restoration, horizontal-only loss and
one confirmed Hold on budget exhaustion. **All four cases passed, with 2,210
successful publications and no failed sends.**

| Follower / synthetic mount | Publications | Initial right/up: yaw, altitude | Retarget left/down: yaw, altitude |
| --- | ---: | --- | --- |
| Chase / vertical | 552 | +11.36°, +0.940 m | −2.13°, −2.361 m |
| Chase / horizontal | 553 | +13.74°, +0.930 m | −1.85°, −2.364 m |
| Vector / vertical | 552 | +22.12°, +0.365 m | −14.18°, −2.005 m |
| Vector / horizontal | 553 | +23.41°, +0.521 m | −11.09°, −2.444 m |

Each run restored confirmed ACTIVE guidance after retarget and reacquisition,
kept ordinary loss horizontal-only, passed submitted and actual-publication
slew assertions, and confirmed Hold after exhausted recovery. Evidence lives
under the 2026-10-02 slice-4b4 cache in
`v13-final-{chase,vector}-{vertical,horizontal}/`: immutable `manifest.json`,
`driver.py`, `result.json`, tracker/publication/observation traces and hover/log
records. All four source checksum maps match the tested source. Driver SHA-256:
`d479ba65f447c13e21845aae9a030a02ccda3484d70638996088d71093ea8f7d`.
Manifests record image identities, executable checksums and Python packages.
Owned evidence containers were removed; the live v12 stack was left intact.

Final help clarifies that `SMOOTHING_FACTOR` weight conventions differ between
followers; both gimbal references describe new-sample weighting. The final
schema/profile includes that description. All four SIH configuration snapshots
parse identically to current defaults: this correction changes descriptions,
not numerical settings or runtime code. A machine-readable evidence/hash recap
is retained as `v13-qualification-summary.json` in the 2026-10-01 cache.

Harness failures remain preserved. Early runs lacked current-provider/post-LOC
fixture metadata; later response windows included pre-existing climb or startup
heading drift. The final fixture waits for stable simulated hover, records that
precondition and uses decisive known world-target offsets. The declared response
thresholds, recovery budgets and production tuning were not relaxed. The strict
Vector slew failure was a real implementation defect and was corrected as above.

During harness bring-up, MAVSDK-only telemetry failed because the installed
SDK has no `velocity_body` method expected by the existing manager. Qualification
uses the production MAVLink2REST path matching v12. The unsupported MAVSDK-only
path remains a compatibility issue for slice 5, without a claim of qualification.

Raw AI reviews are distinct from the operator's acceptance and from simulated
vehicle response. See [operator/UI review](reviews/slice-4b4-v13-operator-raw.md)
and the backend's separate selection/shutdown and guidance reviews.
The real camera image is not coupled to the SIH vehicle. Horizontal installation
has deterministic/synthetic coverage, not physical operator acceptance.

## Next checkpoint

The fresh command-blocked v13 profile and [operator steps](OPERATOR-RETEST-4B-4.md)
are ready. Camera startup is verified by the operator launcher; preparation
did not disturb the active v12 camera session. Operator acceptance and log review
close the retest gate; network/load qualification remains
4b.4d. Linux/Windows/Android release, reviewed publication and the Pi/router
ground test remain slices 5 and 6. No source publication or release is claimed.
