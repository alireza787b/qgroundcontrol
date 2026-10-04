# Keep the bundled application runtime private while exposing freedesktop
# metadata and a launcher in their standard system locations.

if(NOT DEFINED CPACK_QGC_APP_NAME)
    set(CPACK_QGC_APP_NAME "QGroundControl")
endif()
if(NOT DEFINED CPACK_QGC_PACKAGE_ID)
    set(CPACK_QGC_PACKAGE_ID "org.mavlink.qgroundcontrol")
endif()
foreach(qgc_identity IN ITEMS CPACK_QGC_APP_NAME CPACK_QGC_PACKAGE_ID)
    if(NOT "${${qgc_identity}}" MATCHES "^[A-Za-z0-9][A-Za-z0-9_.+-]*$")
        message(FATAL_ERROR "QGC: invalid native package identity: ${qgc_identity}")
    endif()
endforeach()
set(_qgc_app_prefix "opt/${CPACK_QGC_APP_NAME}")
set(_qgc_executable_suffix "/${_qgc_app_prefix}/bin/${CPACK_QGC_APP_NAME}")

if(DEFINED QGC_NATIVE_PACKAGE_ROOT AND NOT QGC_NATIVE_PACKAGE_ROOT STREQUAL "")
    set(_qgc_package_roots "${QGC_NATIVE_PACKAGE_ROOT}")
elseif(DEFINED CPACK_TEMPORARY_DIRECTORY AND IS_DIRECTORY "${CPACK_TEMPORARY_DIRECTORY}")
    file(
        GLOB_RECURSE _qgc_staged_entries
        LIST_DIRECTORIES false
        "${CPACK_TEMPORARY_DIRECTORY}/*"
    )
    set(_qgc_package_roots "")
    foreach(qgc_entry IN LISTS _qgc_staged_entries)
        string(LENGTH "${qgc_entry}" _qgc_entry_length)
        string(LENGTH "${_qgc_executable_suffix}" _qgc_suffix_length)
        if(_qgc_entry_length LESS _qgc_suffix_length)
            continue()
        endif()
        math(EXPR _qgc_root_length "${_qgc_entry_length} - ${_qgc_suffix_length}")
        string(SUBSTRING "${qgc_entry}" ${_qgc_root_length} -1 _qgc_entry_suffix)
        if(_qgc_entry_suffix STREQUAL _qgc_executable_suffix)
            string(SUBSTRING "${qgc_entry}" 0 ${_qgc_root_length} _qgc_root)
            list(APPEND _qgc_package_roots "${_qgc_root}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _qgc_package_roots)
else()
    message(FATAL_ERROR "QGC: native package root is unavailable")
endif()

if(NOT _qgc_package_roots)
    message(FATAL_ERROR "QGC: no staged ${_qgc_executable_suffix} executable found")
endif()

foreach(qgc_root IN LISTS _qgc_package_roots)
    set(_qgc_private_root "${qgc_root}/${_qgc_app_prefix}")
    if(NOT EXISTS "${_qgc_private_root}/bin/${CPACK_QGC_APP_NAME}")
        message(FATAL_ERROR "QGC: incomplete private runtime at ${_qgc_private_root}")
    endif()

    file(GLOB _qgc_qt_core "${_qgc_private_root}/lib/libQt6Core.so*" "${_qgc_private_root}/lib64/libQt6Core.so*")
    if(NOT _qgc_qt_core)
        message(
            FATAL_ERROR "QGC: bundled Qt Core is missing under ${_qgc_private_root}/lib or ${_qgc_private_root}/lib64"
        )
    endif()

    set(_qgc_launcher "${qgc_root}/usr/bin/${CPACK_QGC_APP_NAME}")
    if(EXISTS "${_qgc_launcher}" OR IS_SYMLINK "${_qgc_launcher}")
        message(FATAL_ERROR "QGC: native package launcher already exists: ${_qgc_launcher}")
    endif()

    file(MAKE_DIRECTORY "${qgc_root}/usr/share")
    foreach(qgc_data_dir IN ITEMS applications icons metainfo)
        set(_qgc_source "${_qgc_private_root}/share/${qgc_data_dir}")
        set(_qgc_destination "${qgc_root}/usr/share/${qgc_data_dir}")
        if(NOT EXISTS "${_qgc_source}")
            message(FATAL_ERROR "QGC: required native package metadata is missing: ${_qgc_source}")
        endif()
        if(EXISTS "${_qgc_destination}")
            message(FATAL_ERROR "QGC: native package destination already exists: ${_qgc_destination}")
        endif()
        file(RENAME "${_qgc_source}" "${_qgc_destination}")
    endforeach()

    foreach(
        qgc_required_path IN
        ITEMS "applications/${CPACK_QGC_PACKAGE_ID}.desktop" "icons/hicolor/256x256/apps/${CPACK_QGC_APP_NAME}.png"
              "icons/hicolor/scalable/apps/${CPACK_QGC_APP_NAME}.svg" "metainfo/${CPACK_QGC_PACKAGE_ID}.appdata.xml"
    )
        if(NOT EXISTS "${qgc_root}/usr/share/${qgc_required_path}")
            message(FATAL_ERROR "QGC: required native package payload is missing: /usr/share/${qgc_required_path}")
        endif()
    endforeach()

    file(
        GLOB _qgc_remaining_share_entries
        LIST_DIRECTORIES true
        "${_qgc_private_root}/share/*"
    )
    if(NOT _qgc_remaining_share_entries)
        file(REMOVE_RECURSE "${_qgc_private_root}/share")
    endif()

    file(MAKE_DIRECTORY "${qgc_root}/usr/bin")
    file(CREATE_LINK "../..${_qgc_executable_suffix}" "${_qgc_launcher}" SYMBOLIC RESULT _qgc_link_result)
    if(NOT _qgc_link_result STREQUAL "0")
        message(FATAL_ERROR "QGC: failed to create native package launcher: ${_qgc_link_result}")
    endif()

    message(STATUS "QGC: finalized private native runtime under /${_qgc_app_prefix}")
endforeach()
