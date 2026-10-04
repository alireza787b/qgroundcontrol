# Simulated operator feedback A — native PixEagle connection screens

Date: 2026-09-21.

This is simulated agent feedback from viewing eight supplied screenshots. I
worked on the backend contract and therefore have relevant project context;
I did not design the QGC interface. This is not a real operator study and does
not guarantee an unbiased assessment. For this review I read no implementation
code and performed no interactions. The observations below describe visible
UI evidence and my interpretation, not verified interaction, network, or
aircraft behavior. This file preserves my first review response without a
later implementer rewrite.

## First interpretation of each screen

### 06-pixeagle-disabled.png

My first interpretation is that the PixEagle feature is off for QGC. The empty
Enable PixEagle checkbox and the sentence saying QGC makes no PixEagle
connections while off support that reading. No aircraft, address, or sign-in
state is shown. My next action would be to enable the checkbox. The page is
quiet and the one available action is easy to find. I cannot infer from an
image that network traffic actually stops.

Task impact: clear starting state; no usability blocker visible.

### 07-sign-in.png

Vehicle 1 is selected. I read the empty PixEagle address, Username, and Password
fields as an unsigned-in profile for that vehicle. The disabled Sign in button
and bold instruction to enter the address make the first missing step clear.
My next action would be to enter the companion address and credentials, then
sign in. I would obtain that address from the companion setup; nothing on this
screen discovers it for me.

The statement that sign-in lasts for this app session sets a useful persistence
expectation. The address is the only visible companion identifier at this
point. I cannot tell how to distinguish two physical aircraft both labelled
Vehicle 1 from this screen alone.

Task impact: understandable normal path. Generic vehicle labels are a medium
concern for a larger multi-aircraft setup, although these screenshots only show
Vehicle 1 and Vehicle 2 and do not establish a duplicate-label case.

### 08-rejected-credentials.png

Vehicle 1 is still selected, the address is http://127.0.0.1:8091, and the
username is operator. The bold message explicitly says that username or
password was not accepted. The password field is empty and Sign in is
disabled. My next action would be to re-enter the password, check the username,
and retry. I would not interpret this as successful sign-in.

The message does not say that the password was cleared. An operator who did
not notice the empty field could briefly wonder why Try again is accompanied
by a disabled button. Error text also uses the same white bold appearance as
the later success text, so the actual words carry all of the distinction.

Task impact: low recovery friction. The reason is readable and the editable
fields are present; this is not a dead end.

### 09-signed-in.png

I understand this as successful sign-in as operator to the address shown for
Vehicle 1, with aircraft verification still outstanding. Signed in as operator
and Signed in. Verify the vehicle to check PixEagle's aircraft connection are
direct evidence. My next action would be Verify vehicle.

The address appears disabled now. I assume I must Sign out before changing it,
but the screen does not explicitly tell me that. The text below the buttons is
particularly helpful: this is connection verification only, and PixEagle video,
targeting, and following controls are not available here yet. I would not look
for a Start Following button on this page.

Task impact: the difference between authentication and aircraft verification
is clear. Address-edit recovery is discoverable by inference, rather than
stated directly.

### 10-verified-vehicle-one.png

Vehicle 1, http://127.0.0.1:8091, and operator remain visible. Aircraft
association verified is the displayed success evidence. The button has become
Verify again. I would consider the connection setup step complete and either
close settings or inspect Show connection details if I wanted more confidence.

I cannot tell whether verification is continuously checked, is a single past
check, or will expire. There is no visible last-check time or current freshness
description. Verify again reinforces a possible one-time-check interpretation.
The screenshot proves only that the interface displays verified, not that the
aircraft association or live state is actually correct.

Task impact: medium uncertainty about how long the displayed success can be
trusted. A short indication that the connection is currently monitored, or a
last-check/age indicator, would resolve this if supported by actual behavior.

### 11-verified-details.png

The expanded details show matching QGC, PixEagle, and telemetry aircraft IDs,
all displayed as 18446744073709551001. Instance is fixture-1 and a runtime UUID
is visible. Together with the selected Vehicle 1 and address, this provides
specific evidence behind the displayed association result.

I would compare the three aircraft-ID lines first. The full long numbers are
legible but slow to compare visually. Instance and Runtime are useful to a
technical troubleshooter, although their distinction is not explained in the
pane. I cannot tell whether the text can be selected/copied. Keeping these
details behind an explicit checkbox avoids clutter during ordinary setup.

Task impact: low diagnostic friction; good separation of normal setup from
technical detail. A copy-details affordance would help a support handoff, but
its absence does not block the shown task.

### 12-vehicle-two.png

Vehicle 2 is selected and its address and credential fields are empty. I read
this as a separate unsigned-in profile requiring its own companion details.
That matches the wording Connect the PixEagle companion for each vehicle.
My next action would be to enter Vehicle 2's address and sign in.

I cannot tell whether Vehicle 1 remains connected in the background, whether
switching this dropdown changes the aircraft in Fly View, or whether it only
chooses a settings profile. There is no visible summary of the other vehicle's
connection state. The selected vehicle itself is easy to see.

Task impact: medium uncertainty for multi-aircraft operation. A label such as
Vehicle profile, if that is the actual behavior, or an explicit active-aircraft
indicator would clarify what selection changes. The screenshot alone cannot
settle which description is correct.

### 13-wrong-endpoint.png

Vehicle 2 is selected, but its address is http://127.0.0.1:8091, the same address
previously shown for Vehicle 1. I am signed in as operator. The bold conflict
message and disabled Verify vehicle button tell me that I cannot continue with
this association. Sign out remains available.

Because I saw the earlier images, my first recovery hypothesis is that I entered
Vehicle 1's companion address for Vehicle 2. I would Sign out, replace the
address with Vehicle 2's companion address, sign in again, and verify. In this
single screenshot the error instead says Conflicting aircraft or companion
identities. Resolve the duplicate connection before continuing. It does not
identify the conflicting vehicle/profile or explain the Sign out then edit
address recovery. The address is disabled, so a first-time operator may look
for a nonexistent disconnect/remove-profile action.

Task impact: medium recovery problem. The block itself is visible and
unambiguous, but the instruction is too general to confidently fix the cause.
A message that identifies the conflicting profile where known and tells the
operator how to change the address would reduce trial and error. I cannot infer
from these images whether that suggested sequence succeeds.

## Overall task assessment

I can identify the selected vehicle, companion address, and signed-in username
in the authenticated screens. The normal sequence is easy to infer: enable,
select vehicle, enter address and credentials, sign in, verify. The visible
controls permit setup, sign-out, re-verification, and viewing details. No
PixEagle video, target-selection, or following control is offered, and the page
says so directly. These images do not establish what standard QGC flight
controls elsewhere in the application can do.

The content is visually restrained. The large blank area does not obstruct the
task, and the narrow single-column form is easy to scan at the supplied desktop
size. I do not see overlap or clipping in these eight images. I cannot assess
phone-width layout, keyboard navigation, focus order, hover hints, screen-reader
output, touch targets, or dynamic transitions from these screenshots.

My highest-priority findings are:

1. Medium: conflict recovery should identify the conflicting profile when known
   and explain that signing out permits address correction.
2. Medium: verified status does not visibly communicate recency or whether
   monitoring continues after the check.
3. Medium: Vehicle selection does not explain whether it selects only a stored
   companion profile or also changes the active QGC aircraft; other profiles'
   connection states are not visible.
4. Low: rejected credentials leave an empty password field and disabled retry
   button without explicitly saying the password must be re-entered.

I found no screenshot-level blocker to the ordinary single-vehicle sign-in and
verification task. The observations above are usability hypotheses that need
interaction checks and, eventually, testing with actual operators.
