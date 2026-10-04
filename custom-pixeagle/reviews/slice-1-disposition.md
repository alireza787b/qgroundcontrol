# Slice 1 operator review disposition

Sources: [raw review A](slice-1-operator-a-raw.md),
[raw review B](slice-1-operator-b-raw.md), and the
[follow-up review](slice-1-operator-followup-raw.md), and
[final Release addendum](slice-1-operator-final-raw.md).
The raw responses remain unchanged. These are simulated reviews, not feedback
from recruited human operators or evidence of field qualification.

Evidence root: `/home/alireza/.cache/pixeagle-qgc-baseline/slice-1-2026-09-21/`.
Original captures are in `ui/`; revised captures are in `followup/ui/`.

| Observation | Decision | Follow-up evidence |
| --- | --- | --- |
| A conflict recovery, B-03: locked address and unspecified duplicate make the next step unclear | Name the other vehicle and instruct the operator to sign out and check the address. | `25-duplicate-recovery.png`; actual sign-out, address correction, sign-in and verification exercised. |
| A verified-state interpretation: continued monitoring is not apparent | Say that the connection is monitored. Do not add a continuously changing timestamp to the normal form. A failed request or expired freshness clears trust. | `24-verified.png`, initial `18-disconnected.png`, `19-expired-session.png`; client expiry/disconnect tests. |
| A selector scope, B-01: companion profile selection versus active QGC vehicle | Label the control **Active vehicle in QGC**. Preserve the existing active-vehicle behavior. | `23`–`27` revised screenshots; switching between both authenticated endpoints exercised. |
| A rejected credentials: password is cleared without an explicit retry instruction | Tell the operator to check the username and re-enter the password. Continue clearing the password immediately after submission. | `23-rejected-credentials.png`; retry succeeded with the disposable fixture account. |
| B-02: release-roadmap text competes with connection setup | Shorten the limitation to one availability statement. Keep the following boundary explicit without adding disabled controls. | `24-verified.png`, `27-compact-details.png`. |
| B-04: lost verification on switch-back has no cause | Preserve the reason when a duplicate connection invalidates an established binding, including after polling resumes. Also explain connection interruption/change. | `26-reverification-reason.png`; client test verifies reason survives a fresh context read and clears after successful verification. |
| B-05: compact expanded details and errors were untested | Capture expanded 64-bit IDs at 480×720 and wrapped disconnect/session-expiry states. | `27-compact-details.png`; initial `18-disconnected.png`, `19-expired-session.png`. |
| F-01: unreachable signed-in endpoint appears locked while recovery says to check its address | Keep the editable-address instruction for signed-out users; tell signed-in users to verify after checking the service, or sign out to change its address. | Final Release disconnected capture in `final-ui/ui/`. |
| F-02: sign-in lifetime wording implies sessions cannot expire | State that sign-in credentials are not saved. Leave actual expiry governed by PixEagle's session policy. | Final Release sign-in and disconnected captures in `final-ui/ui/`. |

The form retains one setup sequence and optional diagnostics. It adds no Fly View
controls in this slice. The new Fly View capture `22-fly.png` retains native
vehicle selection and flight controls. Missing map tiles and mock parameter/
mission notices are fixture limitations already present in the baseline.

Remaining acceptance work includes touch and onscreen keyboard behavior, physical
high-DPI/outdoor readability, Android, Windows, long deployment addresses and
human operator feedback. Slice 2 owns video provenance, placement, PiP/fullscreen
and frozen-frame interpretation; slice 4 owns visible following status and
vehicle-specific Stop when another vehicle or view is selected. These remain
separate gates rather than conclusions inferred from connection screenshots.

The final Release addendum confirms F-01/F-02 are visually addressed. One minor
copy concern remains: the wrong-aircraft state offers selecting the intended
vehicle or checking the address, but does not explicitly repeat the sign-out
step for editing. Both the vehicle selector and Sign out remain available; carry
this into slice 2's connection/video recovery review rather than redesigning the
form at this checkpoint.
