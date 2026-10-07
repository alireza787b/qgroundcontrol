# Explicit selection through QGC_CUSTOM_DIR keeps the default build stock.
# Qualification tags must not replace the shared upstream package-version anchor.
set(QGC_GIT_TAG_PATTERN "v5.2.0-dev*")
set(QGC_APP_NAME
    "PixEagle-QGroundControl"
    CACHE STRING "App Name" FORCE
)
set(QGC_ORG_NAME
    "PixEagle"
    CACHE STRING "Organization name" FORCE
)
set(QGC_ORG_DOMAIN
    "github.com"
    CACHE STRING "Organization domain" FORCE
)
set(QGC_APP_HOMEPAGE_URL
    "https://github.com/alireza787b/PixEagle"
    CACHE STRING "Application homepage URL" FORCE
)
set(QGC_PACKAGE_NAME
    "io.github.alireza787b.pixeagle.qgroundcontrol"
    CACHE STRING "Package identifier" FORCE
)
set(QGC_ANDROID_PACKAGE_NAME
    "io.github.alireza787b.pixeagle.qgroundcontrol"
    CACHE STRING "Android package identifier" FORCE
)
