# Raw simulated expert review — whole PixEagle settings page

2026-09-26. User expanded the request to whole-page wording and hierarchy.
Independent AI UX review, not human operator acceptance. Read unauthenticated and
authenticated QML paths, client status messages, native QGC SectionHeader and
SettingsGroupLayout, and private screenshots 04–14. Authenticated recommendations
are source-based; no authenticated final screenshot was supplied for this review.
Earlier raw reviews are preserved unchanged. No product changes performed here.

## Decisions

The page should answer three questions in order: am I connected to the intended
PixEagle/vehicle, how do I use its video/targets, and where are secondary tools/help?
Preserve the native QGC control vocabulary. Use plain inline labels for required
connection steps, actual checkboxes for boolean preferences, buttons for actions,
and collapsed SectionHeader disclosures for secondary information/settings.

1. Keep Enable PixEagle first, with Dashboard beside the PixEagle page title.
2. Keep the connection form and authoritative status visible while enabled.
   Vehicle choice, sign-in, association verification and failure reasons are core
   tasks and must not disappear into an Advanced menu or collapsed group.
3. After connection actions, group the two boolean preferences under one small
   bold QGCLabel: **Video and targeting**. A full bordered card is unnecessary.
4. Put **Web dashboard**, **Connection details**, then **Help** below normal tasks.
   These headings use native SectionHeader, collapsed initially. Help stays
   available while PixEagle is disabled. Connection details remains conditional
   on its useful authenticated content.

## Exact wording recommendations

| Current | Recommended | Reason |
| --- | --- | --- |
| PixEagle backend address | Backend address | Page title already establishes PixEagle; retain backend distinction from browser. |
| Active vehicle in QGC | QGC vehicle | Shorter without losing which application's selection is being changed. |
| Enable PixEagle | Keep | Clear persistent enable preference. |
| Dashboard | Keep | Clear action label; browser tooltip describes destination. Icon alone is ambiguous. |
| Dashboard address (disclosure) | Web dashboard | Names the user-facing destination; the nested field can name the address. |
| Help and links | Help | Users seek help, not an inventory of UI control types. |
| Documentation | PixEagle documentation | Makes the external destination explicit. |
| Repository | GitHub repository | More recognizable destination than a bare technical noun. |
| Show connection details / Connection details | Connection details | Accurate optional diagnostic disclosure; omit the redundant verb Show. |
| Show PixEagle video in Fly View | Keep | Clearly distinguishes companion video from stock QGC video. |
| Tap video to select targets | Keep | Describes persistent direct-selection behavior in operator terms. |
| Verify vehicle / Verify again | Keep | Concise association action; explanatory status supplies details. |
| Open Fly View | Keep | Matches QGC terminology and tells users where operational controls are. |
| Retry connection | Keep | Describes the recovery action. |
| Save / Use default | Keep | Local context identifies the address; these actions are already clear. |

Inside Web dashboard, label the editable override **Custom address (optional)**.
The automatic destination may remain placeholder text, provided no instruction
asks users to copy it. Use explanatory text such as:
“Uses the backend's host and protocol on port 3040. Set a custom address for another
port or reverse proxy. Your browser signs in separately.”
This preserves default behavior without implying automatic port discovery.

## Reduce repeated text

- Disabled introduction: “Connect PixEagle for video and target tracking.” The
  unchecked Enable control communicates state. If retaining the no-network
  explanation, keep it a short second sentence rather than a warning block.
- Enabled, signed-out introduction: “Sign in with your PixEagle account. Your
  password is not saved.” This is one task-oriented instruction, not another
  paragraph explaining no aircraft and unavailable flight controls.
- Remove the separate repeated static paragraph beginning “No aircraft in QGC.”
  The authoritative connected status already identifies that state. Put the
  useful no-aircraft capability explanation once in Help, rather than removing
  identity/error information from the client status.
- Backend hint: “Use the backend URL and port, without /api/v1.” Keep the browser
  port distinction in Web dashboard; avoid repeating it in several paragraphs.
- Tap preference hint: “Tap another target to replace it. Turn off to use Select
  target or Retarget before each selection.” Keep this immediately adjacent to
  the preference; its effect changes interaction semantics and merits explanation.
- Replace the authenticated static instruction with “Use Fly View for video and
  target selection.” If the current release limitation needs stating, a separate
  short Help sentence “Following controls are not available yet” is enough. Do
  not imply that following in the backend has been disabled or stopped.

## Help content hierarchy

Use short paragraphs rather than one long mixed-purpose paragraph:

“Enter the backend address and sign in with your PixEagle account.”

“A local backend usually uses http://127.0.0.1:5077. Remote connections require HTTPS.”

“With an aircraft connected, verify that PixEagle controls the selected QGC vehicle.
Video and permitted target tracking can also work without an aircraft.”

Follow with the two external-link buttons. Following's current native availability
can be stated here if retained. These are operational explanations, not a new setup
wizard, provider manual or infrastructure questionnaire.

## Preserve important state and accessibility

- Keep client.statusText visible and authoritative. It covers wrong aircraft,
  duplicate identities, expired sessions, TLS, sign-in, stale context and verification.
  Do not shorten these into a generic green Connected badge.
- Keep Signed in as near Sign out, and keep the endpoint visible but protected
  while signed in. Vehicle selection must continue to invalidate unfinished edits.
- QGC native SectionHeader with StrongFocus and explicit Expand/Collapse accessible
  naming is appropriate. The black focus outline seen in capture14 needs the
  parent-identified palette-ID shadow correction; do not accept imperceptible focus.
- ScreenTools/palette/localized strings and existing field widths should remain.
  Avoid adding bespoke cards, new typography or a menu for every field.
- Check the resulting page in both companion-only and vehicle-associated states,
  enabled/disabled, collapsed/expanded, and the existing compact private window.
  An uncluttered screenshot alone does not prove keyboard or screen-reader behavior.

This is a bounded copy/hierarchy improvement to the accepted settings workflow.
No new backend contract, network discovery, provisioning flow or project slice
is required. The live connection/target behavior should remain unchanged.
