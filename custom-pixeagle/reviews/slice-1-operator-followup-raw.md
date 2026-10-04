Simulated operator follow-up review — not a human operator study.

I previously reviewed both source and earlier screenshots, so this review is not fully blinded. For this follow-up I inspected only the nine requested images and their dimensions. I did not read current source, operator A's feedback or any disposition. The comments below report what the images communicate; they do not prove that interactions, monitoring, authentication or vehicle routing work.

Image root: /home/alireza/.cache/pixeagle-qgc-baseline/slice-1-2026-09-21/

The followup/ui images represent the requested follow-up set. The ui/17–19 images are earlier captures and must not be treated as proof of the revised interface's behavior.

followup/ui/22-fly.png — 1440×900

Vehicle 1 is the active aircraft according to both the top-right label and its green card. Vehicle 2 remains visible; both cards say Disarmed. The image shows no PixEagle controls or PixEagle-specific reserved area. I would use QGC's normal navigation to reach companion setup, rather than look for a connection action here. This does not establish whether the integration is enabled or whether background traffic exists.

The existing “Vehicles Selected: -” group-action panel remains distinct from the visibly active Vehicle 1, without explaining the distinction. This is the baseline ambiguity already recorded, not evidence of a newly introduced regression. The blank map and nearly overlapping markers limit geographic assessment and are consistent with the supplied mock/offline context.

followup/ui/23-rejected-credentials.png — 1440×900

“Active vehicle in QGC” now explicitly identifies the selector's scope. Vehicle 1 is active, and the companion address ends in 8091. The page reports rejected sign-in, retains the username and shows an empty password field. My next action is to check the username and enter the password again, exactly as the status directs. The disabled Sign in button is consistent with the empty password.

This image addresses the earlier profile-versus-active-aircraft ambiguity through a short label. It does not demonstrate the actual effect of changing the selection or where keyboard focus moves after rejection.

followup/ui/24-verified.png — 1440×900

Vehicle 1 is active, the companion address ends in 8091, and the account is operator. “Aircraft association verified. Connection is monitored.” communicates that setup succeeded and that I need not continually press Verify again merely to obtain status. That is my interpretation of the wording; actual monitoring remains unproven by the screenshot.

The shorter “Connection verification only” availability note states the limitation without listing a release roadmap. Verify again, Sign out and optional details remain easy to identify. I would regard connection setup as complete and return to my other task.

followup/ui/25-duplicate-recovery.png — 1440×900

Vehicle 2 is active and signed in to the address ending in 8091. The message names Vehicle 1 as the conflicting assignment and tells me to sign out and check the PixEagle address. Verify vehicle is disabled; Sign out is available.

The screenshot now supplies both the conflicting vehicle and the immediate recovery action. I would sign out from the displayed Vehicle 2 connection, correct its address and sign in again. The image does not show completion of those steps or prove that Vehicle 1's session is unaffected.

followup/ui/26-reverification-reason.png — 1440×900

Vehicle 1 is active and remains signed in. The status explicitly says that a duplicate connection invalidated verification and asks me to verify again. My next action is Verify vehicle.

This resolves the earlier unexplained loss of verified status for the shown duplicate scenario. I would no longer infer from this capture that switching vehicles itself necessarily invalidates verification. Other causes of invalidation are not shown here.

followup/ui/27-compact-details.png — 480×720

The active vehicle, companion address, account, verified/monitored status, both actions and expanded diagnostics all fit in the captured window. The three aircraft IDs are fully visible and visibly match. Instance and runtime values are also displayed without visible truncation. The ordinary task remains above the diagnostic text, so inspecting details does not hide Sign out or Verify again in this state.

The left navigation takes a substantial fraction of the narrow screen, but the actual connection form remains readable. I see no overlap or horizontal clipping. This is evidence for this particular compact layout, not for touch target size, outdoor readability, long production endpoints, translated text or an onscreen keyboard.

ui/17-compact-verified.png — 480×720, earlier capture

Vehicle 1 is shown as signed in and verified, and the main actions fit. Its label is still the older “Vehicle,” so this image alone leaves selector scope ambiguous. The follow-up images provide direct evidence of the revised label; I would not treat this earlier capture as a current defect.

ui/18-disconnected.png — 480×720, earlier capture

Vehicle 1 and its address remain visible. The page retains “Signed in as operator” but explicitly says that PixEagle could not be reached. I would understand that the app remembers my account but does not currently establish a usable companion connection. I would first check whether the companion is running.

F-01 — Medium in this earlier capture: address-recovery instructions are incomplete. The error tells me to check the address, but the address appears locked. Sign out may unlock it, yet this screen does not say so. Verify vehicle is the likely retry action, although its wording does not directly say retry connection. The follow-up set does not contain the revised disconnected state, so I cannot conclude whether this ambiguity remains in the current interface.

ui/19-expired-session.png — 480×720, earlier capture

Vehicle 1 and the companion address remain, the credentials are empty, and “Your session expired. Sign in again.” gives a clear next action. The form fits without visible clipping.

F-02 — Low: the introductory claim “Your sign-in lasts for this app session” can imply that authentication will not expire while QGC remains open. This earlier expired-session capture shows why that expectation is misleading, and the same introductory wording remains visible in follow-up images. A short statement about credentials not being saved would express the persistence policy more precisely. This is a wording issue, not evidence of incorrect session enforcement.

Remaining assessment

The follow-up screenshots directly address the three main earlier clarity concerns: the selector names the active QGC vehicle, duplicate recovery identifies the other vehicle and a next action, and re-verification names its cause. The compact expanded-details capture also supplies evidence missing from the previous review. These conclusions concern visible communication, not successful interaction or implementation correctness.

The remaining actionable wording issues are the session-lifetime implication and, if still present in the revised disconnected state, how to edit an unreachable endpoint. The scope of Enable PixEagle is still implicit rather than expressly labelled as applying across vehicles; its placement above the vehicle selector suggests that scope, so I regard this as minor residual ambiguity rather than a demonstrated failure.

No broader redesign or additional diagnostic fields are justified by these images. I did not find a screenshot-supported new High issue. Actual recovery execution, changing active vehicles, network isolation, session expiry enforcement and touch use remain outside this screenshot-only review.
