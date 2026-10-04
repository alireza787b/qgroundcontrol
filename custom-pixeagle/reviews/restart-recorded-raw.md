# Shared restart review — raw AI feedback

Reviewer: `/root/restart_review`. Software review; not a physical operator test.
The following is the final review response, preserved verbatim:

> **Remaining isolation defect:** SIH restart checks only `SYSTEM_ADDRESS`. The external MAVSDK server address/port independently determine the command owner. Validate `EXTERNAL_MAVSDK_SERVER=true`, `MAVSDK_SERVER_ADDRESS=127.0.0.1`, port `50051`, and the local MAVLink route before initial launch and every supervised replacement. The current prepared profile is correct; the guard needs to reject conflicting saved edits.
>
> **Harness consistency:** recorded-video preparation with an explicit vertical mount leaves the inactive mount at its horizontal default, then fails its own manifest validation. Keep those values consistent.
>
> **Probe robustness:** repeated login on every readiness poll can exhaust authentication limits during slower startup. Reuse the session; retry authentication only when necessary, with bounded backoff and distinct handling of 401/403/429.
>
> Previously reported restart races, proxy bypass, context handling, reconnect status, and loop ownership appear corrected. The reported live restart and renewed suite results support the software checkpoint. Recorded-video acceptance does not establish hardware camera/joystick acceptance or physical closed-loop tracking.

Disposition, separate from the raw feedback: all three concrete findings were
fixed. Central SIH route validation runs at preparation and every supervised
replacement; negative route fixtures pass. Inactive mount metadata remains
consistent. The probe reuses sessions with bounded authentication recovery.
A fresh copied-source live restart passed in 7.161 seconds with unchanged
sidecars and restored command blocking. Physical acceptance remains deferred.

## Final read-only review after corrections

> Final read-only review: **no remaining material defect found in these fixes or checkpoint documents.**
>
> - Shared SIH validation now covers the actual external MAVSDK command route and local telemetry route at preparation and every replacement.
> - Recorded mount metadata is consistent.
> - The restart probe reuses sessions and bounds authentication recovery.
> - Evidence confirms a **7.161-second** restart, stale confirmation refusal, cleared pending configuration, unchanged sidecars, commands blocked, and no resumed operations.
> - The documented **3,419 backend unit passes** match the suite log.
> - Operator instructions correctly distinguish recorded acquisition, synthetic SIH guidance, and deferred physical acceptance. Uncommitted source, release limitations, CPU fallback, and unchanged gains are disclosed.
>
> This supports closing the shared-restart and recorded-video software checkpoint; hardware qualification remains open.
