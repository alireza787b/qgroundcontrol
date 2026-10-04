# Operator review evidence

Each UI checkpoint includes screenshots, the task being attempted, the fixture
and window size, application logs, and independent raw feedback. An image proves
only what is visible; gestures, freshness, routing, and successful commands need
interaction evidence and tests.

The reviews in this directory are simulated agent reviews unless explicitly
identified as feedback from a participating human operator. They are not a user
study and cannot establish real-world usability. Independent review and neutral
prompts reduce leading feedback; they do not guarantee absence of bias.

For each slice:

1. Capture the disabled state and relevant success, waiting, and recovery states.
   Include the smallest supported layout and the relevant video/view variants.
2. Give the reviewer screenshots and neutral operator tasks before explaining
   implementation choices or expected answers. Preserve the first interpretation.
3. Save the raw response unchanged, with screenshot names and evidence limits.
4. Maintain a separate disposition: observed issue, task impact, action or reason
   for deferral, owning slice, and follow-up evidence.
5. Recheck affected states after changes. Retain both captures and both responses.

Assess clear aircraft/companion ownership, one understandable next action,
readable status and recovery, touch/keyboard discovery, and access to standard
flight controls. Keep technical diagnostics behind disclosure controls. Do not
count mock/offline fixture artifacts as integration regressions without evidence.

Raw feedback: [slice 0](slice-0-operator-raw.md),
[slice 1 A](slice-1-operator-a-raw.md), [slice 1 B](slice-1-operator-b-raw.md),
[slice 1 follow-up](slice-1-operator-followup-raw.md), and
[slice 1 final Release addendum](slice-1-operator-final-raw.md).
Slice 2: [first review](slice-2-operator-raw.md),
[follow-up](slice-2-operator-followup-raw.md), and
[fullscreen addendum](slice-2-operator-fullscreen-raw.md).
Decisions: [slice 0 carry-forward disposition](slice-0-disposition.md) and
[slice 1 disposition](slice-1-disposition.md).
See also the [slice 2 disposition](slice-2-disposition.md).

Slice 3b: [initial raw feedback](slice-3b-operator-initial-raw.md),
[follow-up raw feedback](slice-3b-operator-followup-raw.md), and
[disposition and remaining human checks](slice-3b-disposition.md).

Slice 3b user-feedback revision: [initial design review](slice-3b-feedback-design-raw.md),
[visual follow-up](slice-3b-feedback-followup-raw.md), and
[disposition](slice-3b-feedback-disposition.md).

September 26 page/dashboard review: [accepted session](slice-3b-accepted-session-raw.md),
[initial visual review](slice-3b-links-initial-raw.md),
[disclosure review](slice-3b-links-disclosure-raw.md),
[whole-page review](slice-3b-operator-full-page-raw.md),
[About conventions](slice-3b-operator-about-pattern-raw.md), and
[disposition](slice-3b-links-disposition.md).

[Final whole-page screenshots review](slice-3b-page-final-raw.md).

October 3 software checkpoint: [raw shared restart review](restart-recorded-raw.md).
Its disposition and software gates are separate from deferred physical acceptance.
