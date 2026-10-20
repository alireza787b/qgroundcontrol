# October 20 private QGC release candidate

This checkpoint freezes the custom integration on official QGC master
`6133ce6d26832d3efdb81e9a7c324ec82898f42d`, merged as `4a14b8a90`.
It includes the pending-sign-in/context/verification cancellation repair
documented in [the direct Pi operator guide](OPERATOR-DIRECT-PI.md).
No public custom-QGC PR or binary release is part of this checkpoint.

## Renewed verification

- Incremental Linux build passed after the upstream merge.
- All 476 Unit/Integration tests passed with the standard Flaky/Network exclusions.
- All 170 settings-generator tooling tests passed.
- The merge's changed-file lint run reported findings in 34 files. Every reported
  file is byte-identical to the official upstream revision above; no integration
  edit introduced those diagnostics. Global lint is therefore not reported as
  passing. Retain the raw lint log and provenance receipt with qualification
  evidence rather than reformatting unrelated upstream code in this checkpoint.

The companion backend candidate is
[PixEagle 7.5.0-rc.1](https://github.com/alireza787b/PixEagle/releases/tag/v7.5.0-rc.1),
source `2d129ecdfc91e9f7a757f921e936a6a1cdcdcc5d`.
Its [draft PR #25](https://github.com/alireza787b/PixEagle/pull/25) targets `main`;
publishing the prerelease does not merge it into stable `main`.

Backend verification passed 3,799 unit tests, 196 integration tests, 155 API and
security guardrails, 13 API-inventory checks and 6 release-version checks.
Fourteen unit subtests passed; 11 optional GStreamer tests were skipped in the
ordinary Core environment. The Dashboard passed 493 tests in 64 suites and its
production build. Optional-runtime evidence is separate from those Core results.

## Artifact and deployment boundaries

Use one manually dispatched `pixeagle-platform.yml` run with `platform=all` and
`build_type=Release` from the frozen `pixeagle-qgc-2026.10.20-rc.1` tag. Verify
each artifact's checksum, source and package metadata before replacing the older
Desktop packages. Record the run URL and exact source in the private handoff.
Android currently uses a temporary CI debug signing identity, not a persistent
production signing key; a later differently signed APK may require uninstall.

Native WebRTC remains capability-gated in ordinary PixEagle deployments.
Authenticated JPEG fallback and browser WebRTC remain available. Impaired-link
pixel identity, concurrent allocation, browser parity, startup, sustained Pi
power/cooling and native Windows/Android acceptance remain open. A successful
platform build does not close these gates or establish real-flight acceptance.

The private Pi profile retains Camera tracking, vertical mounting, camera controls
and `gm_velocity_vector`; the saved bench copy has the circuit breaker enabled.
Public factory defaults remain local tracking without gimbal configuration.
Keep installation configuration, credentials and SD images outside the repository.
Reconnect the Pi for the normal updater, exact-revision and service/config checks
before making a new offline SD image. Retain a verified previous image until its
replacement has been verified. See the private handoff's sync/image instructions.
