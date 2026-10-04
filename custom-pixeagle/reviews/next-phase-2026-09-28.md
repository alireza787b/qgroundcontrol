# Next-phase review record — 2026-09-28

This is an internal planning record for the proposed PixEagle–QGroundControl 4b sequence. It contains independent AI-assisted repository reviews, not interviews with QGC maintainers, DJI/Auterion, or field operators. It is evidence for design discussion and must not be represented as vendor endorsement.

## Operator-oriented review

The accepted compact panel is a good operational baseline, but the generic camera toolbar icon and green video dot do not identify PixEagle or distinguish tracking from aircraft following. Use a PixEagle identity with independent target and follow activity states. Keep details in the existing QGC drawer and hide unsupported actions from the compact surface. The stale settings sentence saying that following is unavailable must be removed.

A single “mode” would conflate local PixEagle algorithms, camera-owned algorithms, gimbal control, video source, and aircraft following. The review recommends separate selectors and fresh state for each. The primary action should change with state: Select/Retarget, hold-to-Start, or Stop. Stop should remain available for the captured owner even if video becomes stale.

## Architecture and backend review

The inspected backend already has reload tiers: OSD immediate, tracker/model/provider changes requiring controlled tracker restart or runtime swap, and video-source changes requiring a system restart. Native settings therefore need saved/running/restart-pending state, explicit Apply, guarded restart, reconnect verification, and no automatic follow resume.

The current Topotek provider exposes Ethernet SIP/UDP control and paired RTSP. The next API should advertise provider/source identity, selection modes, supported axes, limits, orientation, readiness, and camera-owned tracking separately from local PixEagle tracking. Retain the host association check until an authoritative identity contract exists; do not expose vendor packet details in QGC.

The existing camera movement path needs bounded holds and captured-owner Stop semantics. A fresh frame is required for target selection, but emergency Stop must not depend on fresh video. Camera movement must not be enabled by expanding the local tracking action allowlist.

The current OSD legacy toggle applies renderer state but is not a sufficient authoritative persistent native contract. A bounded explicit desired-state endpoint with generation, permission, audit, and effective-state reporting is required before exposing it as a QGC setting.

## Evidence limitations

The latest target-path audit has no Smart action. Test9 contains installed models and is the required next demonstration. Existing synthetic video is open-loop, so it cannot prove closed-loop aircraft or gimbal accuracy. Historical hardware notes are partial and do not qualify camera-owned Smart, rectangle retention, rotated installation, or every axis.

## Result

These findings are incorporated into [NEXT-PHASE-PLAN.md](../NEXT-PHASE-PLAN.md). No implementation, service restart, model change, or hardware action was performed for this review.

## Cleanup policy accepted for planning

The user authorized changes to PixEagle configuration and underlying logic when they remove redundant, confusing, or unsafe legacy behavior without breaking supported workflows. The implementation must therefore keep one authoritative owner for each state, document any compatibility translation, test the migration boundary, and remove obsolete UI/routes rather than carrying two competing controls forward.
