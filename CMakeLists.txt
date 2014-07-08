cmake_minimum_required (VERSION 2.8)
project (libmemsinjection)

set (LIBMEMSINJECTION_VER_MAJOR 0)
set (LIBMEMSINJECTION_VER_MINOR 1)
set (LIBMEMSINJECTION_VER_PATCH 1)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "bin")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin")
set (SOURCE_SUBDIR "src")

set (CPACK_SOURCE_GENERATOR "TGZ" "ZIP")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}-src")
set (CPACK_SOURCE_IGNORE_FILES
      ".*deb$"
      ".*zip$"
      ".*gz$"
      ".*~$"
      ".*.dll.a$"
      "README.TXT"
      "LICENSE.TXT"
      ".git"
      "^CPack.*"
      "_CPack_Packages"
      "CPackConfig.cmake"
      "CPackSourceConfig.cmake"
      "CMakeFiles"
      "CMakeCache.txt"
      "cmake_install.cmake"
      "install_manifest.txt"
      "Makefile"
      "bin/lib.*"
      "src/mems_version.h$")

configure_file (
  "${SOURCE_SUBDIR}/mems_version.h.in"
  "${SOURCE_SUBDIR}/mems_version.h"
  )

add_library (memsinjection SHARED ${SOURCE_SUBDIR}/setup.c
                                  ${SOURCE_SUBDIR}/protocol.c)
add_executable (readmems ${SOURCE_SUBDIR}/readmems.c)

if (MINGW)

  message (STATUS "Found Windows/MinGW platform.")

  # statically link against the C MinGW lib to avoid incurring an additional DLL dependency
  set (CMAKE_SHARED_LINKER_FLAGS "-static-libgcc")
  set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc")

  # convert Unix-style newline characters into Windows-style
  file (READ "README" README_CONTENTS)
  file (READ "LICENSE" LICENSE_CONTENTS)
  file (WRITE "README.TXT" ${README_CONTENTS})
  file (WRITE "LICENSE.TXT" ${LICENSE_CONTENTS})

  message (STATUS "Defaulting installation path to: $ENV{SystemDrive}/memsinjection")
  set (CMAKE_INSTALL_PREFIX "$ENV{SystemDrive}/memsinjection")

  set_target_properties (
    memsinjection
      PROPERTIES
        SOVERSION ${LIBMEMSINJECTION_VER_MAJOR}
        VERSION ${LIBMEMSINJECTION_VER_MAJOR}
  )

  target_link_libraries (readmems memsinjection)

  install (FILES ${SOURCE_SUBDIR}/memsinjection.h
           DESTINATION "include")
  install (FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.dll
           DESTINATION ".")
  install (FILES "README.txt"
                 "LICENSE.txt"
           DESTINATION "doc")
  install (PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/readmems.exe
           DESTINATION ".")

  set (CPACK_GENERATOR "ZIP")
  set (CPACK_PACKAGE_NAME ${PROJECT_NAME})
  set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}-${CMAKE_SYSTEM_NAME}")
  include (CPack)

else()

  message (STATUS "Defaulting to UNIX/Linux/BSD build configuration.")

  find_program (GZIP_TOOL
                NAMES gzip
                PATHS /bin
                      /usr/bin
                      /usr/local/bin)

  if (GZIP_TOOL)
    set (MAN "libmemsinjection.3")
    set (MAN-GZ "${MAN}.gz")
    add_custom_command (OUTPUT ${MAN-GZ}
                        COMMAND ${GZIP_TOOL} -c ${MAN} > ${MAN-GZ}
                        DEPENDS ${MAN}
                        COMMENT "Building ${MAN-GZ}..."
                        VERBATIM)
    add_custom_target (man ALL
                       DEPENDS ${MAN-GZ})
  else ()
    message (WARNING "gzip not found! Manual page will not be installed.")
  endif (GZIP_TOOL)

  set (CMAKE_SKIP_RPATH TRUE)

  if ((CMAKE_SYSTEM_NAME MATCHES "OpenBSD") OR
      (CMAKE_SYSTEM_NAME MATCHES "FreeBSD") OR
      (CMAKE_SYSTEM_NAME MATCHES "Darwin"))
    set (CMAKE_INSTALL_PREFIX "/usr/local")
    message (STATUS "System is OpenBSD/FreeBSD/OS X; install path prefix is set to ${CMAKE_INSTALL_PREFIX}.")
  elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    set (CMAKE_INSTALL_PREFIX "/usr/pkg")
    message (STATUS "System is NetBSD; install path prefix is set to ${CMAKE_INSTALL_PREFIX}.")
  else()
    set (CMAKE_INSTALL_PREFIX "/usr")
    message (STATUS "Install path prefix is set to ${CMAKE_INSTALL_PREFIX}.")
  endif()

  set_target_properties (
    memsinjection
      PROPERTIES
        SOVERSION ${LIBMEMSINJECTION_VER_MAJOR}
        VERSION   ${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}
  )

  target_link_libraries (readmems memsinjection pthread)

  # set the installation destinations for the header files,
  # shared library binaries, and reference utility
  install (FILES ${SOURCE_SUBDIR}/memsinjection.h
           DESTINATION "include"
           PERMISSIONS
            OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

  if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    message (STATUS "Using OpenBSD-style shared library naming.")
    install (FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.so
                   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.so.${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}
             DESTINATION "lib"
             PERMISSIONS
              OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    message (STATUS "Using Mac OS X-style shared library naming.")
    install (FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.dylib
                   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.${LIBMEMSINJECTION_VER_MAJOR}.dylib
                   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}.dylib
             DESTINATION "lib"
             PERMISSIONS
              OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  else()
    message (STATUS "Using Linux/FreeBSD-style shared library naming.")
    install (FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.so
                   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.so.${LIBMEMSINJECTION_VER_MAJOR}
                   ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmemsinjection.so.${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}
             DESTINATION "lib"
             PERMISSIONS
              OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endif()

  install (DIRECTORY DESTINATION "share/doc/memsinjection" DIRECTORY_PERMISSIONS
            OWNER_READ OWNER_EXECUTE OWNER_WRITE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)

  if (GZIP_TOOL)
    install (FILES "${MAN-GZ}"
             DESTINATION "share/man/man3"
             PERMISSIONS
              OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endif (GZIP_TOOL)

  install (FILES "README"
                 "LICENSE"
           DESTINATION "share/doc/memsinjection"
           PERMISSIONS
            OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

  install (PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/readmems
           DESTINATION "bin"
           PERMISSIONS
            OWNER_READ OWNER_EXECUTE OWNER_WRITE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)

  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    # set the architecture flag that will be used by the Debian package builder
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
    elseif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
    endif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")

    set (CPACK_GENERATOR "DEB")
    set (CPACK_PACKAGE_NAME ${PROJECT_NAME})
    set (CPACK_PACKAGE_VERSION_MAJOR ${LIBMEMSINJECTION_VER_MAJOR})
    set (CPACK_PACKAGE_VERSION_MINOR ${LIBMEMSINJECTION_VER_MINOR})
    set (CPACK_PACKAGE_VERSION_PATCH ${LIBMEMSINJECTION_VER_PATCH})
    set (CPACK_DEBIAN_PACKAGE_NAME ${CPACK_PACKAGE_NAME})
    set (CPACK_DEBIAN_PACKAGE_MAINTAINER "Colin Bourassa <colin.bourassa@gmail.com>")
    set (CPACK_DEBIAN_PACKAGE_SECTION "Libraries")
    set (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.15)")
    set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Library supporting serial communication with the Rover MEMS 1.6 engine control module")
    set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBMEMSINJECTION_VER_MAJOR}.${LIBMEMSINJECTION_VER_MINOR}.${LIBMEMSINJECTION_VER_PATCH}-${CMAKE_SYSTEM_NAME}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    set (CPACK_RESOURCE_FILE_LICENSE "LICENSE")
    set (CPACK_RESOURCE_FILE_README "README")
    include(CPack)
  endif()

endif()

