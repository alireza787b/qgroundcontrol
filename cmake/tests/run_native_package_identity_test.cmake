cmake_minimum_required(VERSION 3.25)

foreach(qgc_required IN ITEMS QGC_MODULE_DIR TEST_BINARY_DIR CMAKE_EXECUTABLE)
    if(NOT DEFINED ${qgc_required} OR "${${qgc_required}}" STREQUAL "")
        message(FATAL_ERROR "${qgc_required} is required")
    endif()
endforeach()

# Require a staged public metadata file.
function(qgc_require_file path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required package payload missing: ${path}")
    endif()
endfunction()

# Exercise generated CPack identity and private runtime staging together.
function(qgc_test_native_identity app_name package_id generator)
    set(qgc_fixture "${TEST_BINARY_DIR}/${app_name}-${generator}")
    file(REMOVE_RECURSE "${qgc_fixture}")
    file(MAKE_DIRECTORY "${qgc_fixture}/source/.github")
    file(WRITE "${qgc_fixture}/source/.github/COPYING.md" "Test license")
    file(WRITE "${qgc_fixture}/source/README.md" "Test readme")
    file(
        WRITE "${qgc_fixture}/source/CMakeLists.txt"
        "
cmake_minimum_required(VERSION 3.25)
project(${app_name} VERSION 1.2.3 DESCRIPTION \"Package fixture\" LANGUAGES NONE)
set(QGC_PACKAGE_NAME \"${package_id}\")
set(QGC_APP_VERSION_DEV 1)
set(QGC_GIT_HASH abcdef)
list(APPEND CMAKE_MODULE_PATH \"${QGC_MODULE_DIR}/install/CPack\")
include(CreateCPack${generator})
"
    )
    execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" -S "${qgc_fixture}/source" -B "${qgc_fixture}/build"
        RESULT_VARIABLE qgc_configure_result
        OUTPUT_VARIABLE qgc_configure_output
        ERROR_VARIABLE qgc_configure_error
    )
    if(NOT qgc_configure_result EQUAL 0)
        message(FATAL_ERROR "Package fixture configure failed: ${qgc_configure_output}\n${qgc_configure_error}")
    endif()

    include("${qgc_fixture}/build/CPackConfig.cmake")
    string(TOLOWER "${app_name}" qgc_native_name)
    if(NOT CPACK_QGC_APP_NAME STREQUAL app_name
       OR NOT CPACK_QGC_PACKAGE_ID STREQUAL package_id
       OR NOT CPACK_PACKAGING_INSTALL_PREFIX STREQUAL "/opt/${app_name}"
    )
        message(FATAL_ERROR "CPack did not retain the application identity")
    endif()
    if(generator STREQUAL "Deb")
        if(NOT CPACK_DEBIAN_PACKAGE_NAME STREQUAL qgc_native_name OR NOT CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
                                                                     STREQUAL "/opt/${app_name}/lib"
        )
            message(FATAL_ERROR "DEB identity or private dependency path changed")
        endif()
    elseif(NOT CPACK_RPM_PACKAGE_NAME STREQUAL qgc_native_name)
        message(FATAL_ERROR "RPM identity changed")
    endif()

    set(qgc_stage "${qgc_fixture}/stage/Runtime")
    set(qgc_private "${qgc_stage}/opt/${app_name}")
    foreach(
        qgc_payload IN
        ITEMS "bin/${app_name}" "lib/libQt6Core.so.6" "share/applications/${package_id}.desktop"
              "share/metainfo/${package_id}.appdata.xml" "share/icons/hicolor/256x256/apps/${app_name}.png"
              "share/icons/hicolor/scalable/apps/${app_name}.svg"
    )
        get_filename_component(qgc_parent "${qgc_private}/${qgc_payload}" DIRECTORY)
        file(MAKE_DIRECTORY "${qgc_parent}")
        file(WRITE "${qgc_private}/${qgc_payload}" "${app_name} fixture")
    endforeach()
    # A stock installation beside the custom payload must not be touched.
    if(NOT app_name STREQUAL "QGroundControl")
        file(MAKE_DIRECTORY "${qgc_stage}/usr/bin" "${qgc_stage}/opt/QGroundControl/bin")
        file(WRITE "${qgc_stage}/usr/bin/QGroundControl" "stock launcher")
        file(WRITE "${qgc_stage}/opt/QGroundControl/bin/QGroundControl" "stock runtime")
    endif()
    # cmake-lint: disable=C0103
    set(CPACK_TEMPORARY_DIRECTORY "${qgc_fixture}/stage")
    include("${QGC_MODULE_DIR}/install/FinalizeNativePackage.cmake")
    foreach(qgc_payload IN
            ITEMS "applications/${package_id}.desktop" "metainfo/${package_id}.appdata.xml"
                  "icons/hicolor/256x256/apps/${app_name}.png" "icons/hicolor/scalable/apps/${app_name}.svg"
    )
        qgc_require_file("${qgc_stage}/usr/share/${qgc_payload}")
    endforeach()
    file(READ_SYMLINK "${qgc_stage}/usr/bin/${app_name}" qgc_link)
    if(NOT qgc_link STREQUAL "../../opt/${app_name}/bin/${app_name}")
        message(FATAL_ERROR "Launcher does not resolve to its private runtime: ${qgc_link}")
    endif()
    if(NOT app_name STREQUAL "QGroundControl")
        file(READ "${qgc_stage}/usr/bin/QGroundControl" qgc_stock_launcher)
        file(READ "${qgc_stage}/opt/QGroundControl/bin/QGroundControl" qgc_stock_runtime)
        if(NOT qgc_stock_launcher STREQUAL "stock launcher" OR NOT qgc_stock_runtime STREQUAL "stock runtime")
            message(FATAL_ERROR "Custom staging modified the stock application")
        endif()
    endif()
endfunction()

foreach(qgc_generator IN ITEMS Deb RPM)
    qgc_test_native_identity("QGroundControl" "org.mavlink.qgroundcontrol" "${qgc_generator}")
    qgc_test_native_identity("PixEagle-QGroundControl" "io.github.alireza787b.pixeagle.qgroundcontrol"
                             "${qgc_generator}"
    )
endforeach()

foreach(qgc_identity IN ITEMS CPACK_QGC_APP_NAME CPACK_QGC_PACKAGE_ID)
    execute_process(
        COMMAND
            "${CMAKE_EXECUTABLE}" "-D${qgc_identity}=../../stock" "-DQGC_NATIVE_PACKAGE_ROOT=${TEST_BINARY_DIR}/invalid"
            -P "${QGC_MODULE_DIR}/install/FinalizeNativePackage.cmake"
        RESULT_VARIABLE qgc_invalid_result
        ERROR_VARIABLE qgc_invalid_error
    )
    if(qgc_invalid_result EQUAL 0 OR NOT qgc_invalid_error MATCHES "invalid native package identity")
        message(FATAL_ERROR "Unsafe identity was not rejected: ${qgc_invalid_error}")
    endif()
endforeach()

message(STATUS "Stock and custom native package identities are isolated")
