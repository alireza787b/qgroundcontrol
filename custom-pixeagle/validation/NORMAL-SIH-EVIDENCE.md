# Normal-camera follower evidence — 2026-10-02

The checked-in `run-normal-sih-evidence.py` reuses the gimbal evidence harness's
owned Docker launcher. Each simulator has only loopback networking, exposes no
host ports and is removed only after its ownership label matches. No camera or
existing operator stack is used. Source snapshots, configuration, package
versions, image/binary checksums and driver checksums accompany every attempt.

The observations are normalized `POSITION_2D` measurements with a bounding box
and confidence. They enter production AppController dispatch, continuity,
follower calculation, OffboardCommander and MAVSDK publication. Independent
PX4 attitude and NED position establish selected response directions. This is
not tracker acquisition, physical camera calibration or visual convergence.

## Results

Evidence root:
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01`.

| Profile | Artifact directory | Successful publications | Result |
| --- | --- | ---: | --- |
| MC Position | `normal-sih-mc-position-v3` | 199 | Positive/negative yaw and climb/descent response; Stop confirmed |
| MC Chase | `normal-sih-mc-chase-v1` | 199 | Positive/negative yaw and climb/descent response; Stop confirmed |
| MC Ground | `normal-sih-mc-ground-v1` | 199 | Forward/right and reverse command directions, right/left displacement; Stop confirmed |
| MC Visual Centering | `normal-sih-mc-distance-v1` | 198 | Right/left displacement and climb/descent response; Stop confirmed |
| MC Attitude Rate | `normal-sih-mc-attitude-v1` | 48 | Command signs, rate bounds, independent pitch response and Stop passed; attitude envelope **blocked** |
| FW Attitude Rate | `normal-sih-fw-attitude-v1` | 0 follower publications | Actual airplane SIH startup **blocked** before Offboard |

All **843** recorded MC publications succeeded. Every completed MC case stopped
publication and recorded one confirmed Hold. The CB check is a canonical gate
probe **after Stop**, not a test of changing CB during active following.

The simulation overlay enables altitude control and sets the MC Attitude Rate
capture-altitude offset to zero. Gains, speed/rate limits and repository default
configuration remain unchanged. Ground mode retains zero vertical command;
its image-Y error requests forward motion rather than climb/descent. Held
synthetic image coordinates are deliberate control stimuli, not observations
coupled to a world target. Simulated drift is not evidence of position-hold
quality or optimal gains.

## Release blockers discovered

**MC Attitude Rate:** absolute PX4 pitch reached **43.0455°**, exceeding the
profile's configured `MAX_PITCH_ANGLE: 35`. The publisher's rate limits were
respected, but the configuration does not establish a measured attitude
envelope. Negative body yaw commands in the second tilted phase did not yield
negative net Euler yaw; roll/pitch/yaw coupling must be considered. The passed
core-path result is not acceptance of this high-authority profile. Resolve the
angle envelope and airframe-specific thrust/response qualification before
release use of this mode. The generated result is preserved separately from
its explicit review annotations.

**Fixed-wing:** the pinned image supports `sihsim_airplane`. It armed and
acknowledged takeoff, but did not reach the requested altitude within the
readiness deadline, so no Offboard entry or follower publication was attempted.
The startup/launch workflow remains unqualified. Separately,
`PX4InterfaceManager` does not populate airspeed; the follower falls back to
ground speed. That cannot qualify stall protection or wind-dependent flight.
Neither blocker was masked by running the fixed-wing profile on a quad.

## Reproduce

Run from the QGC repository with a new output directory:

```bash
/home/alireza/PixEagle-qgc-integration/.venv/bin/python \
  custom-pixeagle/validation/run-normal-sih-evidence.py --execute \
  --follower mc_velocity_chase --output /absolute/path/new-evidence
```

`--backend-repo`, `--python`, `--bin-dir` and `--opencv-lib` override local
dependency locations. The six allowed profile names come from the explicit
qualification matrix. FW selects the airplane model; MC selects quad SIH.
Existing output directories are never overwritten. `--execute` is required.

Scoped verification passed: **63** backend follower direction/control tests
(including four new tests using actual fixed-wing PID objects), **3** mocked
launcher boundary tests, Ruff and whitespace checks. First Position attempts
failed only on runner import/limit-field mistakes; their logs remain separate,
and the corrected v3 evidence is the accepted run. The existing four v13 gimbal
follower/mount results are reused because production guidance was unchanged.

Physical camera acceptance, measured network/physical stopping behavior,
fixed-wing launch/airspeed, MC attitude-envelope qualification and release
platform gates remain distinct checkpoints.

## MC measured-attitude repair and retest

The focused repair adds BODY/Euler coupling protection after smoothing and on
every actual publication. It preserves thrust, gains and default limits. Actual
attitude receipt has its own freshness evidence: SDK stream receipt or advancing
REST `ATTITUDE.time_boot_ms`. Repeated REST data and fresh altitude alone cannot
qualify it. Invalid/stale/out-of-envelope attitude requests immediate handoff;
the local sender Stop is owned, coalesced and joined during manager shutdown.
Publication traces report the effective guarded rates.

| Frozen-source retest | Publications | Independent measured result |
| --- | ---: | --- |
| `normal-sih-mc-attitude-guard-final-short-v1` | 48 successful, 0 failed | Original 1.2 s positive/negative two-axis stimuli passed; absolute pitch **31.736°**, roll **15.089°**, both below 35°; Stop confirmed |
| `normal-sih-mc-attitude-guard-final-v1` | 107 successful, 0 failed | Longer 3 s reversal **blocked**: absolute pitch **35.171°**, roll **25.491°**; immediate safety handoff confirmed Hold |
| `gimbal-sih-attitude-source-regression-v1` | 550 successful, 0 failed | Unchanged Vector/Vertical guidance, source freshness, independent response and Stop passed |

The earlier `normal-sih-mc-attitude-guard-v1` is a pre-final development snapshot
and is not used as final combined-source evidence. Each final directory retains
its exact sources and dependency manifest. A subsequent two-line publisher
lifecycle correction restricts the old MC guard reason to MC failures; the
cross-profile regression passed in the final focused gate. It changes no MC
command calculation, so the SIH snapshots are retained with that exact final
source difference disclosed rather than relabelled. The prolonged failure's
`envelope-review.json` records measured angles, publication counts and confirmed
handoff without replacing its original failed result.

**MC Attitude Rate remains a release blocker for sustained inputs.** The software
projection substantially reduces the original 43.046° excursion and fails
closed when the envelope is crossed, but does not account for physical angular
inertia well enough to prove the 35° limit during prolonged reversal. The short
pass is a limited acceptance case. No numerical tolerance, gain increase or
changed default angle limit hides the longer failure.

Repair verification passed **254** focused/Phase 0 tests, **57** existing REST
manager tests and **3** launcher contract tests. Cases cover coupled directions,
EMA output, missing/stale attitude during a frozen-frame 500 ms command lifetime,
actual publisher timing overrides, blocked Stop RPCs, once-owned teardown and
session restart. Broad combined-source gates are reported in the main checkpoint.

Reproduce the longer case by adding `--phase-duration 3
--require-attitude-envelope` to the runner. The latter asserts actual absolute
roll and pitch, independently of command-rate bounds. These simulated inputs
remain unrelated to a live camera's target observations.
