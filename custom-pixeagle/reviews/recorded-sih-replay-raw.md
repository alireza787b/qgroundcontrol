# Recorded SIH replay — raw AI review, 2026-10-03

Reviewer: `/root/restart_review`. These are AI software/UX observations, not
physical operator acceptance.

## Implementation review

- The replay opt-in remains separate from Dashboard Follower Test and ordinary PX4 execution. Factory default is off.
- Normal supervision clears the SIH binding marker; isolated supervision supplies it. Private binding, namespace, instance, routes, expected UID and fresh telemetry constrain authorization independently of the checkbox.
- Existing startup readiness rechecks before and after Offboard use the shared helper. Fresh replay provenance remains intact; cached/stale frames are denied.
- The non-video provider regression was correctly identified and, with the added requires-video condition, stays outside image-replay authorization.
- QGC clearly distinguishes confirmed SIH following from command preview without adding another switch.

No additional material implementation defect found in this pass. Final acceptance
still requires the real CSRT → existing publisher probe and honest resolution/reporting
of QGC suite results. Preserve hardware replay refusal, CB restoration, and the
distinction between simulated response and visual convergence.

## Correction found during review

The first runtime guard acted on an optional replay display even when angle
tracking did not require video. It now checks the tracker's video requirement;
a regression verifies that a non-video provider reaches its normal evidence
path without a replay handoff.

The historical v17 handoff incorrectly implied recorded input could already
start aircraft following. Its synthetic-target SIH publications and actual
recorded CSRT/Smart acquisition evidence were separate. The new checkpoint
must establish the complete recorded-tracker-to-SIH path independently.

## Final focused review

Read-only check passed; no additional defect found in these focused changes.

The shared classifier correctly reports a prediction-only target as tracker
unavailable when fresh SIH replay is authorized, and retains the replay-refusal
code when authorization is absent. Startup and legacy preflight use the same
classifier.

The runtime replay guard now applies only to video-dependent trackers. Its
regression verifies that an optional recorded display cannot interrupt non-video
guidance.

The reported 168 successful publications, independent yaw response, and confirmed
Hold support the actual CSRT-to-SIH command path. The stable text fixture remains
acquisition/publication evidence, with no tracking-accuracy or visual-convergence
claim.

## Test-harness timing review

No blockers in these two test fixes.

- Qt::PreciseTimer prevents the deliberate 3.1-second late delivery from firing early inside the 3-second deadline.
- GPS condition waits now allow the documented one-second Fact update interval plus scheduling delay. Assertions remain intact; no fixed sleeps or production GPS changes.

These corrections address test timing without weakening the behavior being verified.

## V21 completed-Stop repair — raw independent AI review

Source: `/root/restart_review`; read-only review of the clean-completion flag
release and regression additions. AI review, not operator acceptance.

> No blocker in the scoped fix. Clean completion reopens target selection while failed or incomplete teardown remains blocked. The retained future and handoff result preserve concurrency and idempotency.
>
> The regressions cover the reported sequence and failure guards. Describe native coverage precisely: it uses real AppController teardown, native follower selection and native target execution; Stop itself calls the internal teardown method.
>
> Failed-teardown cleanup retry remains unchanged and explicitly deferred.
