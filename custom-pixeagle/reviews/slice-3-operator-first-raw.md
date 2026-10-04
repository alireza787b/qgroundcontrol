Simulated operator feedback from five static screenshots only. I did not inspect source or prior reviews, interact with controls, or run the application or tests. These preliminary captures do not establish actual connection state, control authority, command results, or readiness for operation.

- **`02-settings.png`**
  - First interpretation: PixEagle is switched off. I need to enable it before connecting a companion.
  - Clear: The PixEagle settings entry is easy to spot, and the explanation describes what the enable switch does.
  - Confusing: I cannot see where tracker settings will appear. This screen offers only an enable switch.
  - Next action: Enable PixEagle and look for connection and tracker configuration.

- **`04-connected.png`**
  - First interpretation: The interface reports a PixEagle connection, signed in as `qgc-demo`, with no aircraft connected to QGC. This session appears limited to viewing video and status.
  - Clear: “No aircraft in QGC” and “Aircraft controls remain unavailable” are explicit. The separate explanation that target selection and following are unavailable is useful.
  - Confusing: The earlier sentence says permitted target tracking can work without PX4, while the lower sentence says this session is read-only. I can reconcile those statements, but I have to read both to understand my current capability. `qgc-demo` looks like an account name; `http://127.0.0.1:8093` does not tell me which physical companion I am looking at.
  - Next action: Open Fly View. If I expected to select targets, I would investigate why this session is read-only. I still could not identify a particular aircraft or physical companion from this screen.

- **`05-video.png`**
  - First interpretation: “Disconnected” initially makes me think the system is disconnected. The central “PixEagle · No aircraft” and “Live video” labels then suggest that the aircraft connection and video connection are separate.
  - Clear: “No target selected,” the inactive tracker/follower text, and the disabled-looking “Cancel tracking” button suggest tracking is not active. “Select target” and “Tracking setup” are discoverable.
  - Confusing: A red “TRACKING” label appears over a vehicle even though other text says no target is selected and the tracker is inactive. I do not know which indicator to trust. “Select target” also appears available after the preceding screenshot described selection as unavailable; the screenshots do not explain whether permissions changed.
  - Noise: Numerous `N/A` readings, the blank white inset, and several layers of overlays compete with the actual video. The sentence about following controls is very small.
  - Next action: Try “Select target” and look for selection instructions. I cannot tell from this capture whether selection means clicking an object or drawing a box.

- **`07-selection-result.png`**
  - First interpretation: Despite the filename, I cannot conclude that selection succeeded. The interface still says “No target selected,” and “Cancel tracking” looks disabled.
  - Clear: The screen communicates that QGC is not presenting an active selected target.
  - Confusing: The red “TRACKING” label remains visible while “Tracker: Not Active” is shown. If this were the screen immediately after my selection attempt, I would not know whether the attempt was rejected, missed, still pending, or never submitted. No explanation is visible.
  - Next action: Look for an error or pending indication, then try “Select target” again. Repeating the action would be guesswork from this screenshot.

- **`08-tracking.png`**
  - First interpretation: The display reports active target tracking and inactive following.
  - Clear: “Tracking target,” “Tracker: Active,” the target markings, and the change to “Retarget” support the same interpretation. “Follower: Not Active” and the footer distinguish tracking from following. “Retarget,” “Cancel tracking,” and “Tracking setup” give me plausible routes to replace the target, stop tracking, and find tracker settings.
  - Confusing: The prominent “Disconnected” label still competes with the active tracking display. No unique companion identity is visible. Several overlapping target marks make it harder to distinguish the selected object boundary from the aiming reticle.
  - Next action: Use “Retarget” to replace the target. To stop, use “Cancel tracking,” then look for an explicit inactive state and changed controls. I cannot verify either action from these images.

Across the captures, I can identify the intended controls most confidently in `08-tracking.png`. I cannot identify a specific aircraft or physical companion in control. My largest uncertainty is how to reconcile the disconnected banner, the central status, and tracking text inside the video when they appear to disagree.
