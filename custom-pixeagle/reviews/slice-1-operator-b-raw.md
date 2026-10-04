Simulated operator review B — not a human operator study.

This review covers screenshots 06–16 only. I previously reviewed the implementation, so I am not fully blinded. I have not read operator A's feedback or the disposition. Below, I distinguish visible evidence from interpretations and missing evidence. Screenshots do not prove interaction success, authentication enforcement, vehicle routing, traffic isolation or freshness.

Severity: Medium means likely hesitation or misunderstanding during setup; Low means avoidable friction. I found no screenshot-supported High issue in this connection-only slice.

06-pixeagle-disabled.png

My first reading is that PixEagle is off and ticking Enable PixEagle begins setup. The unchecked control and absence of the connection form communicate this with little clutter. The text says there are no connections while off; the screenshot cannot verify that network claim. No aircraft is identified here, which is acceptable for an inactive setup page.

07-sign-in.png

Vehicle 1 is selected. No companion is configured and no authenticated user is shown. The next action is evident: enter the address, username and password, then sign in. The disabled Sign in button is consistent with the empty form.

B-01 — Medium: the selector's operational scope is unclear. From this page alone I would interpret Vehicle as the aircraft whose companion settings I am editing. Nothing states whether changing it also changes the active aircraft elsewhere in QGC. My earlier code review supplies that knowledge, but the screenshot does not. Use a short label that communicates the actual scope; do not add a paragraph of implementation explanation.

08-rejected-credentials.png

Vehicle 1 and the address ending in 8091 remain visible, and operator remains in the username field. The password is empty. The bold rejection message gives a direct next action: correct the credentials and retry. It does not imply a successful association. This is readable and actionable at the captured size; I cannot tell whether keyboard focus returns to the password field.

09-signed-in.png

The page now says Signed in as operator for Vehicle 1 at the address ending in 8091. The bold status and Verify vehicle button distinguish authentication from aircraft verification. My next action is clear. The address has become grey and appears unavailable for editing.

B-02 — Low: the persistent sentence about video, targeting and following being unavailable in this build competes with the current connection task. Its limitation is useful, but the release-roadmap wording and length add noise on every signed-in screen. A shorter availability statement would communicate the same boundary.

10-verified-vehicle-one.png

Vehicle 1, the address ending in 8091, the signed-in account and Aircraft association verified are visible together. I would regard connection setup as complete. Verify again and Sign out are available; no targeting or following action is offered. The evidence here is the application's displayed assertion, not an independent demonstration that the aircraft match is correct.

11-verified-details.png

The three aircraft IDs visibly match, and the instance is fixture-1. This supplies inspectable evidence behind the summary without showing it on the normal page. The long decimal identifiers are readable at 1440×900, although manually comparing them is slow. Runtime looks diagnostic rather than operator-actionable; keeping these values behind Show connection details is appropriate. There is no reason to place all of them on the default view.

12-vehicle-two.png

Vehicle 2 is selected, and the companion address and credential fields are empty. My reading is that this vehicle needs its own setup. The image does not show whether Vehicle 1 remains signed in in the background; I would not infer that merely from the switch. The scope ambiguity in B-01 remains.

13-wrong-endpoint.png

Vehicle 2 is selected with the same address ending in 8091 previously shown for Vehicle 1. Authentication succeeded as operator, but verification is unavailable. The bold conflict message makes it clear that setup is not complete.

B-03 — Medium: recovery requires guessing. Resolve the duplicate connection does not identify the other vehicle or explain what to change. The address appears locked, Verify vehicle is disabled, and Sign out is the only obvious action besides changing the vehicle selection. I would try Sign out to unlock the address, but that is an inference. For this shown condition, identify the duplicate vehicle and give the immediate corrective action in the status message. Do not make the operator search through raw IDs to discover the normal recovery path.

14-verified-vehicle-two.png

Vehicle 2 is now associated with the address ending in 8092 and displays Aircraft association verified. The resulting state is clear. The difference between the two companion addresses is only the final port digit; that is a fixture characteristic, not evidence that real companion names will be equally difficult to distinguish. This capture does not prove what interactions resolved the preceding conflict.

15-switch-back.png

Vehicle 1 returns with its earlier address and signed-in account. It now asks for verification, despite appearing verified in screenshot 10. The Verify vehicle action is easy to find.

B-04 — Medium: the reason for lost verification is invisible. The screenshots do not tell me whether switching vehicles invalidates verification, the earlier duplicate connection did so, or the connection became stale. I would verify again, but I could form the wrong expectation that every switch requires it. Preserve the concise next action while naming the actual reason when an established association is invalidated. This is an explanation issue; the images do not show that invalidation itself is incorrect.

16-narrow-verified.png

At 800×600, Vehicle 1, the companion address, the account, verified status, Verify again, Sign out and Show connection details remain fully visible. I see no clipping of the connection task's essential controls. Text remains readable at the supplied image size; this does not establish readability outdoors, on a high-DPI device or at a user's chosen scale. The left sidebar extends beyond the bottom around Palette Test, a debug navigation entry rather than an essential PixEagle action.

B-05 — Low, unverified scenario: this is only the collapsed-details verified state at the smaller size. Expanded diagnostics, a long real endpoint, wrapped error messages, sign-in with an onscreen keyboard and narrower widths are not shown. Their usability remains untested; I am not claiming they are broken.

Overall task reading

At 1440×900 the compact form is readable, with enough empty space to avoid crowding. It does not need more fields or a dashboard to fill that space. At 800×600 the supplied verified state still supports the immediate setup actions without scrolling the main form.

The main ambiguity is whether the Vehicle selector edits a companion profile or changes the active QGC aircraft. A second, smaller scope question is whether Enable PixEagle applies to all vehicles; its placement suggests a global setting but does not explicitly say so. Short, accurate labels should resolve these questions without expanding the page.

The priority improvements are an explicit selector scope, direct duplicate-endpoint recovery and a reason when verification is invalidated. Keep the existing separation between signed-in and verified states, the short primary action area, and optional diagnostics. No broader redesign is justified by these screenshots.

Not established by this review: behavior during an in-flight request, session expiry, a removed vehicle, TLS failure, touch interaction, successful navigation back to flight controls, or the actual destination of any network request. Those require their own evidence rather than favorable interpretation of the screenshots.
