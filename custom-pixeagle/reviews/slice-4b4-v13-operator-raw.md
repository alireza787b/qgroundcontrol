# Slice 4b.4 v13 AI operator review

Date: 2026-10-02. This is an AI software/operator-workflow review, not a
physical pilot evaluation or camera acceptance.

## Raw observations

- The accepted compact panel should remain. The reported failure occurs during
  a normal tap-to-retarget workflow; another settings switch would not explain
  or repair it.
- “Checking…” hides the operational difference between changing a target,
  losing a target, reacquiring and stopping. Use the same short labels in the
  panel and toolbar. Keep explanatory text in the existing details/tooltip.
- A follower preview must say “Test” during transitions as well as steady
  operation. “Following” and “aircraft confirmation” would imply aircraft
  command delivery during a preview.
- After recovery ends, a confirmed Hold should remain identifiable. An
  unsuccessful Stop must not use the same label. Old history from another
  aircraft or stale telemetry must not imply a currently confirmed state.
- A rejected edge selection should preserve the current target and following,
  accompanied by a concise, actionable refusal. Quietly moving the requested
  point would surprise the operator.
- The v12 camera image and SIH aircraft pose are independent. Visible camera
  motion does not establish that aircraft guidance turned smoothly toward the
  requested target. Publication and independent pose traces are needed.
- Retain hold-to-confirm Start, immediate Stop, movable camera controls and
  installation settings in the web Dashboard. This checkpoint does not justify
  another layout redesign or gain increase.
- The camera-free SIH setup initially exposed an independent backend baseline
  issue: the installed MAVSDK telemetry API lacks `velocity_body`, which the
  MAVSDK-only telemetry path calls unconditionally. The instrumented run uses
  the existing MAVLink2REST telemetry path, consistent with the accepted v12
  profile. That pass must not be read as qualifying MAVSDK-only telemetry.

## Acceptance boundary

Automated label, history, freshness and identity tests support the software
review. The next logged operator retest must establish whether the labels and
retarget behavior are understandable in actual use. No physical acceptance is
claimed by this review.
