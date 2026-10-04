if(NOT QGC_ENABLE_GST_VIDEOSTREAMING)
    message(FATAL_ERROR "The PixEagle overlay requires QGC_ENABLE_GST_VIDEOSTREAMING for authenticated JPEG video.")
endif()

# Keep credential storage isolated from QGC settings and pin the cross-platform
# secure-store implementation used by Linux, Windows, and Android builds.
include(FetchContent)
set(BUILD_WITH_QT6 ON)
set(BUILD_TRANSLATIONS OFF)
set(BUILD_SHARED_LIBS OFF)
set(LIBSECRET_SUPPORT OFF)
FetchContent_Declare(
    PixEagleQtKeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG aa6da344e1a20b9194e12bace3665caeea6b6304
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(PixEagleQtKeychain)

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEaglePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEaglePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleClient.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleCameraClient.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleConfigClient.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleSafetyClient.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleManager.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleVideoItem.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleVideoItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleVideoController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleVideoController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleTargetController.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PixEagleTargetController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/PixEagleSettings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/PixEagleSettings.h
    PARENT_SCOPE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings
    PARENT_SCOPE
)

qt_add_library(PixEagleModule STATIC)
qgc_configure_target(PixEagleModule)
set_source_files_properties(src/PixEagleSettings.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleSettings.qml)
set_source_files_properties(src/PixEagleConfigSection.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleConfigSection.qml)
set_source_files_properties(src/PixEagleVideoView.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleVideoView.qml)
set_source_files_properties(src/PixEagleModelDialog.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleModelDialog.qml)
set_source_files_properties(src/PixEagleIndicator.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleIndicator.qml)
set_source_files_properties(src/PixEagleOptionsDialog.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleOptionsDialog.qml)
set_source_files_properties(src/PixEagleMovablePanel.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleMovablePanel.qml)
set_source_files_properties(src/PixEagleCameraControls.qml PROPERTIES QT_RESOURCE_ALIAS PixEagleCameraControls.qml)
qt_add_qml_module(
    PixEagleModule
    URI PixEagle
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES src/PixEagleSettings.qml
              src/PixEagleVideoView.qml
              src/PixEagleModelDialog.qml
              src/PixEagleIndicator.qml
              src/PixEagleOptionsDialog.qml
              src/PixEagleMovablePanel.qml
              src/PixEagleConfigSection.qml
              src/PixEagleCameraControls.qml
    NO_PLUGIN
)
set(CUSTOM_LIBRARIES
    PixEagleModule qt6keychain
    PARENT_SCOPE
)

set(PIXEAGLE_SETTINGS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/PixEagle.SettingsGroup.json")
qgc_set_qt_resource_alias("${PIXEAGLE_SETTINGS_JSON}")
qt_add_resources(
    ${CMAKE_PROJECT_NAME} pixeagle_settings_json
    PREFIX "/json"
    FILES "${PIXEAGLE_SETTINGS_JSON}"
)

if(QGC_BUILD_TESTING)
    include(QGCTest)
    add_subdirectory("${CMAKE_SOURCE_DIR}/test/PixEagle" "${CMAKE_BINARY_DIR}/test/PixEagle")
endif()

set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD CUSTOMHEADER="PixEaglePlugin.h" CUSTOMCLASS=PixEaglePlugin
    PARENT_SCOPE
)
