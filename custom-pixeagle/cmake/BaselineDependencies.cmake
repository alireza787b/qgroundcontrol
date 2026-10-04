# Load with cmake -C for both stock and custom baseline configurations.
set(QGC_UPDATE_TRACKED_DEPS
    OFF
    CACHE BOOL "Keep baseline dependency revisions fixed" FORCE
)

# The upstream declaration follows main; disabling refresh alone does not pin a fresh checkout.
set(CPM_DECLARATION_ArduPilotParams
    "NAME;ArduPilotParams;GITHUB_REPOSITORY;ArduPilot/ParameterRepository"
    "GIT_TAG;6a8165531148318d9d5975e36d1a8b5323f0030a;DOWNLOAD_ONLY;TRUE"
    CACHE STRING "Pinned ArduPilot parameter repository" FORCE
)
