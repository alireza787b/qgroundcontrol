Simulated operator review only. I inspected the nine screenshots without reading code or implementation discussions. These are hypothetical reactions, not participant findings. I did not interact with the application. “Live” below means the interface says live; screenshots cannot establish actual freshness, successful switching, recovery, or fullscreen behavior. I treated the synthetic imagery and blank offline map as fixtures.

1. **`first-ui/ui/02-disabled.png`**

   **First interpretation:** PixEagle is off. The explanation makes it clear that QGC is not attempting a PixEagle connection.

   **Next action:** Check “Enable PixEagle.”

   **Confusion or noise:** Nothing blocks this step. I do not yet know whether I need an aircraft, but enabling the feature is the obvious next action. The numerous unrelated settings entries are background noise.

   **Severity/task impact:** Low. Clear entry point.

2. **`first-ui/ui/03-companion-setup.png`**

   **First interpretation:** I can connect directly to a companion and view video without an aircraft. I need its address, username, and password. Credentials will not be retained.

   **Next action:** Enter the companion address and credentials, then sign in.

   **Confusion or noise:** The disabled button and “Enter the PixEagle address” explain the immediate requirement. If I do not already have the address and credentials, this screen gives me no clue where to obtain them. The paragraph about a future aircraft connection is understandable but interrupts the immediate sign-in task.

   **Severity/task impact:** Low when connection details are known; medium if this is the operator’s first setup and those details were not provided.

3. **`first-ui/ui/04-companion-signed-in.png`**

   **First interpretation:** I am signed in as `operator` to the displayed address. The companion is connected independently of an aircraft. I can watch video and status, but cannot select targets or use following controls.

   **Next action:** Go to Fly View to see the video. If something fails, “Retry connection” and “Show connection details” look like useful places to start.

   **Confusion or noise:** “Retry connection” is prominent even though the status says connected; I would wonder briefly whether another connection step is expected. “No aircraft association” sounds more technical than the earlier “no aircraft in QGC.” The persistent sign-in instructions above the connected state make me reread to confirm that sign-in is already complete. The sidebar’s “Fly View” entry is inside Application Settings, so I cannot tell from this screenshot whether it takes me to the actual flight view or its settings.

   **Severity/task impact:** Low to medium. Connection success and restrictions are clear; getting from setup to viewing takes some interface familiarity.

4. **`first-ui/ui/06-restart-sign-in.png`**

   **First interpretation:** PixEagle remains enabled and its address is remembered, but I am signed out and must enter credentials again.

   **Next action:** Enter username and password, then sign in.

   **Confusion or noise:** Without knowing this followed a restart, I could mistake this for an unexpected lost session. “Sign-in credentials are not saved” makes the empty fields understandable. “Show PixEagle video in Fly View” remains checked, which I read as a preference rather than evidence of a current connection.

   **Severity/task impact:** Low. Reauthentication is clear, although its immediate cause is not stated.

5. **`synthetic-companion/ui/01-fly-video.png`**

   **First interpretation:** The large view is PixEagle companion video, and the status panel calls it live. The top bar says disconnected, so I initially hesitate: is the companion connected or not? “Companion only” makes me infer that the top bar concerns the aircraft.

   **Next action:** Continue watching; to bring up the map, I would try the lower-left inset or its overlapping-rectangles icon.

   **Confusion or noise:** The two connection messages require interpretation. “PixEagle” identifies the feature but does not identify the companion address or camera. “Tracking inactive. Following inactive.” tells me current activity but does not tell me whether I can activate those functions. Disabled flight buttons, zero-valued telemetry, and the compass add visual noise when I have no aircraft.

   **Severity/task impact:** Medium. I can find video and a plausible map-switch control, but connection scope and available actions are less clear than in settings.

6. **`synthetic-companion/ui/02-frozen.png`**

   **First interpretation:** The retained image is no longer reliably current. The amber “Video delayed or paused” is noticeable and prevents me from casually treating it as live.

   **Next action:** Briefly check whether it resumes, then return to PixEagle settings and try “Retry connection.”

   **Confusion or noise:** I cannot tell how old the image is or whether it was deliberately paused. I see no local resume/retry control or instruction. The existing “Disconnected” header gives me another possible explanation, but still does not identify which connection it means. The image remaining visible is useful, but I must keep looking at the small status panel to remember it is stale.

   **Severity/task impact:** Medium. The warning communicates the problem; deciding whether to resume, wait, or reconnect requires guessing.

7. **`synthetic-companion/ui/04-source-resolution-swap.png`**

   **First interpretation:** I see a narrow vertical video area centered in a wide window, with unused space on either side. It is labelled live. The fixture’s `camera-b` text suggests a source identity, but the application status itself still only says PixEagle.

   **Next action:** Continue viewing and, if I need to verify the source, look for connection details.

   **Confusion or noise:** The top toolbar and status panel overlap visible image content. The application does not visibly tell me whether the source changed or just its shape. The lower-left map inset has no clearly visible controls in this capture, making the switching action less discoverable than in the earlier landscape screenshot.

   **Severity/task impact:** Medium. The image is visible, but independent source identification and access to inset controls are uncertain.

8. **`synthetic-companion/ui/05-portrait.png`**

   **First interpretation:** I am viewing companion video in a portrait window. The status panel remains readable and says live.

   **Next action:** Try tapping the small lower-left map inset if I want the map.

   **Confusion or noise:** The map inset is small and has no obvious visible switching icon here. The header and status panel cover the upper image; the inset, telemetry, and compass occupy much of its lower area. The image’s lower-left source text is partly obscured. The aircraft-disconnected header still competes with the live companion status.

   **Severity/task impact:** Medium. Basic video status remains understandable, but image obstruction and uncertain switching affordances are more noticeable at this size.

9. **`synthetic-companion/ui/06-portrait-pip.png`**

   **First interpretation:** The map now occupies the main view and video appears to be in the tiny lower-left inset. I can make out part of “Companion only,” but cannot read a complete status.

   **Next action:** Try the inset or overlapping-rectangles icon to enlarge the video and inspect its state.

   **Confusion or noise:** The inset’s controls and label consume much of the visible video area. I cannot determine from the visible inset whether the video is live, delayed, or paused. With its status unreadable, the prominent “Disconnected” header becomes my clearest connection message. Zero telemetry and a large compass remain prominent despite no aircraft being connected.

   **Severity/task impact:** High for checking video freshness while keeping the map primary. I would need to enlarge the video to regain confidence. This screenshot shows that arrangement, but does not demonstrate that switching into or out of it worked.
