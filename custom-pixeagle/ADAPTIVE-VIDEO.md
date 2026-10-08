# Adaptive JPEG delivery

The native receiver advertises `adaptive_dimensions: true` alongside the
existing latest-frame acknowledgement capability. PixEagle can then reduce
this client's delivery resolution without changing capture or analysis sizes.
No operator setting or separate endpoint is added.

The authenticated backend video context must advertise
`delivery_scaling_version: "1"` before QGC accepts a smaller frame. Both
encoded dimensions must fit within the context's base dimensions. Aspect
ratio may differ only by the half-pixel rounding of each scaled dimension.
Larger or distorted frames are refused. Older backends retain exact-size
validation; older clients do not advertise scaling and retain full dimensions.
A changed scaling contract replaces the receiver context.

Each decoded frame must still exactly match its provenance and verified
selection geometry. Selection uses the geometry/token of the actually
presented frame and retains the backend's analysis dimensions. A new delivery
size cannot borrow another frame's geometry, identity or target selection.
The frame ACK is receipt/queue admission, not proof of presentation.

Focused validation covers legacy behavior, negotiated downscaling, rounding,
oversize/distortion refusal, authenticated native capability exchange, and
normalized selection using the retained frame token. Run the existing
`PixEagleVideoItemTest`, `PixEagleVideoControllerTest`, and
`PixEagleTargetControllerTest` suites against the custom Debug build.
