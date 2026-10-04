# Slice 4b joystick — raw visual review

Independent AI review from an operator-workflow perspective; not a human
operator study or hardware test. The reviewer previously inspected the camera
control code, so this is not a blinded review. This record preserves observations
and uncertainties rather than declaring operator acceptance.

Evidence reviewed at 1440×900:

- `47-joystick-release.png`
- `50-joystick-drag.png`
- Final first-open capture: `52-joystick-final-neutral.png`
- Earlier comparison: `32-camera-final-controls.png`

All four are under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b-2026-09-29/ui/` and use a
synthetic camera fixture with no aircraft. No real camera, human touch session,
motor response or network-loss behavior was observed. Screenshot 52 was supplied
as the final first-open capture after the centering fix: the thumb visibly sits
at the center of both rings. This supports the rendered neutral appearance;
initial axis values and absence of outgoing commands rely on interaction tests.

## First reading

The old centered dialog reads like a settings form: Pan, Tilt, Roll and Zoom
are separate text rows, and the dimmed background makes the operator leave the
video task. The partly visible dialog behind it adds a second layer to navigate.

The new panel reads as an on-screen camera control. The joystick is the largest
control, zoom buttons sit beside it, and the short PixEagle camera title identifies
which subsystem owns it. The video is no longer dimmed. In screenshot 47 the
panel occupies an edge position and leaves the central viewing area clear. This
is a substantial reduction in intrusion and scanning effort.

Screenshot 50 shows the same panel at another position, with no visible clipping.
That demonstrates two rendered placements; successful dragging and release
behavior still need interaction evidence. Neither screenshot shows target
imagery underneath the panel, so the cost of occluding a moving target cannot
be judged here.

## Concerns and questions to retain

1. **Medium — joystick expectations differ from command behavior.** A circular
   pad commonly suggests two simultaneous axes and movement speed proportional
   to displacement. This implementation sends finite pulses on one dominant
   axis, at provider defaults. Nothing in these static images communicates that
   distinction. A first-use hint or existing tooltip can explain the actual
   behavior without filling the permanent panel with text. The hardware bench
   should explicitly ask whether diagonal drags and small adjustments behave as
   the operator expects. Do not present this as an analog-rate controller.

2. **Medium — target-selection pause is not visible.** The camera panel is open
   while the separate PixEagle target widget still displays Classic, tracker
   name and No target. From the images alone I would not know that video target
   selection is disabled until camera controls close. This protects against
   conflicting gestures, but an operator could interpret an ignored target tap
   as a broken tracker. Check this transition in the hands-on session and use a
   concise contextual explanation if it causes hesitation.

3. **Low — the Center icon needs learned meaning.** The camera-and-arrow icon
   is consistent with a camera action, but its exact effect is not self-evident
   in a static image. The implementation's Center camera tooltip and accessible
   name help mouse and assistive users. Touch discoverability remains untested.
   The plus/minus controls are easier to interpret as zoom from their position.

4. **Low — the ellipsis does not disclose Roll.** Hiding a less frequent axis
   reduces clutter appropriately. The image alone does not reveal that the
   ellipsis opens Roll controls. The existing Roll controls tooltip is useful;
   observe whether touch users find it before adding another visible label.

5. **Unverified — inactive Stop state.** Stop is grey in both new captures.
   With the thumb centered and no movement shown, that is a plausible idle
   state. These images cannot establish whether Stop stays reachable during a
   delayed action, lost status or a failed Stop. Use the action/state tests and
   bench logs for those claims.

6. **Unverified — available space and readability.** The new panel is compact
   at this desktop size and its controls are separated clearly. Narrow windows,
   touch targets at the operator's actual display scale, sunlight, expanded Roll
   controls and long error messages are not shown here. Do not infer their
   usability from this desktop capture.

## Review disposition

The new floating panel addresses the user's concern about a modal, text-heavy
camera controller. Keep this direction for the operator checkpoint. The main
remaining questions are interaction semantics and the transition back to
targeting; the screenshots do not justify another broad visual redesign.
Hardware behavior, Stop reliability and operator acceptance remain separate
gates.
