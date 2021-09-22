cmake_minimum_required (VERSION 2.8)
project (librosco)
include (GNUInstallDirs)

set(PROJECT_NAME "librosco")
set(PROJECT_DESCRIPTION "Library supporting serial communication with the Rover MEMS 1.6 engine control module via the ROSCO protocol")
set(PROJECT_AUTHOR "Colin Bourassa <colin.bourassa@gmail.com>")
set(PROJECT_URL "https://github.com/colinbourassa/librosco")

set (LIBROSCO_VER_MAJOR 1)
set (LIBROSCO_VER_MINOR 0)
set (LIBROSCO_VER_PATCH 0)
set (LIBROSCO_VERSION "${LIBROSCO_VER_MAJOR}.${LIBROSCO_VER_MINOR}.${LIBROSCO_VER_PATCH}")

set (SOURCE_SUBDIR "${CMAKE_SOURCE_DIR}/src")

set (CPACK_SOURCE_GENERATOR "TGZ")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBROSCO_VERSION}")
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
      "src/rosco_version.h$"
      "build/"
      "build-win32")

option (ENABLE_DOC_INSTALL "Enables installation of documentation (README, LICENSE, manpage) to the appropriate locations" OFF)
option (ENABLE_TESTAPP_INSTALL "Enables installation of the readmems utility" OFF)
option (ENABLE_PKGCONFIG_INSTALL "Enables installation of a pkgconfig configuration file" ON)

configure_file (
  "${SOURCE_SUBDIR}/rosco_version.h.in"
  "${CMAKE_BINARY_DIR}/rosco_version.h"
  )

include_directories ("${CMAKE_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}")

if (BUILD_STATIC STREQUAL "ON")
  add_library (rosco STATIC ${SOURCE_SUBDIR}/setup.c
                            ${SOURCE_SUBDIR}/protocol.c)
else()
  add_library (rosco SHARED ${SOURCE_SUBDIR}/setup.c
                            ${SOURCE_SUBDIR}/protocol.c)
endif()

add_executable (readmems ${SOURCE_SUBDIR}/readmems.c)

if (MINGW OR WIN32)
  message (STATUS "Found Win32/MinGW platform.")

  # statically link against the C MinGW lib to avoid incurring an additional DLL dependency
  set (CMAKE_SHARED_LINKER_FLAGS "-static-libgcc")
  set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc")

  # convert proper Unix-style newline characters into Windows-style
  configure_file ("${CMAKE_SOURCE_DIR}/README" "${CMAKE_BINARY_DIR}/README.TXT" NEWLINE_STYLE WIN32)
  configure_file ("${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/LICENSE.TXT" NEWLINE_STYLE WIN32)

  set_target_properties (
    rosco
      PROPERTIES
        SOVERSION ${LIBROSCO_VER_MAJOR}
        VERSION ${LIBROSCO_VER_MAJOR}
  )

  target_link_libraries (readmems rosco)

  # MXE requires that documentation and test applications not be installed
  # by default, so we provide a couple env vars to control this
  if (ENABLE_DOC_INSTALL)
    message (STATUS "Install will include documentation.")
    install (FILES "${CMAKE_BINARY_DIR}/README.TXT"
                   "${CMAKE_BINARY_DIR}/LICENSE.TXT"
             DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  else()
    message (STATUS "Skipping installation of documentation.")
  endif()

  set (CPACK_GENERATOR "ZIP")
  set (CPACK_PACKAGE_NAME ${PROJECT_NAME})
  set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "")
    set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBROSCO_VERSION}-${CMAKE_SYSTEM_NAME}")
  else()
    set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBROSCO_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  include (CPack)

else()

  message (STATUS "Defaulting to UNIX/Linux/BSD build configuration.")

  find_program (GZIP_TOOL
                NAMES gzip
                PATHS /bin
                      /usr/bin
                      /usr/local/bin)

  if (GZIP_TOOL)
    set (MANNAME "${PROJECT_NAME}.3")
    set (MAN "${CMAKE_SOURCE_DIR}/${MANNAME}")
    set (MAN-GZ "${CMAKE_BINARY_DIR}/${MANNAME}.gz")
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

  set_target_properties (
    rosco
      PROPERTIES
        SOVERSION ${LIBROSCO_VER_MAJOR}
        VERSION   ${LIBROSCO_VERSION}
  )

  target_link_libraries (readmems rosco pthread)

  if (ENABLE_DOC_INSTALL)
    install (DIRECTORY DESTINATION "${CMAKE_INSTALL_DOCDIR}" DIRECTORY_PERMISSIONS
              OWNER_READ OWNER_EXECUTE OWNER_WRITE
              GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE)

    if (GZIP_TOOL)
      install (FILES "${MAN-GZ}"
               DESTINATION "${CMAKE_INSTALL_MANDIR}/man3"
               PERMISSIONS
                OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif (GZIP_TOOL)

    install (FILES "${CMAKE_SOURCE_DIR}/README"
                   "${CMAKE_SOURCE_DIR}/LICENSE"
             DESTINATION "${CMAKE_INSTALL_DOCDIR}"
             PERMISSIONS
              OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  else()
    message (STATUS "Skipping installation of documentation.")
  endif()

  #
  # If building on Linux, set up to create a Debian package
  #
  if (CMAKE_SYSTEM_NAME MATCHES "Linux")

    find_program (DPKG_CMD dpkg)
    if (NOT DPKG_CMD)
      message (STATUS "'dpkg' not found in path; defaulting to i386.")
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
    endif()
    execute_process (COMMAND "${DPKG_CMD}" --print-architecture
                     OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
                     OUTPUT_STRIP_TRAILING_WHITESPACE)
    message (STATUS "CPack architecture for dpkg: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

    set (CPACK_GENERATOR "DEB")
    set (CPACK_PACKAGE_NAME ${PROJECT_NAME})
    set (CPACK_PACKAGE_VERSION_MAJOR ${LIBROSCO_VER_MAJOR})
    set (CPACK_PACKAGE_VERSION_MINOR ${LIBROSCO_VER_MINOR})
    set (CPACK_PACKAGE_VERSION_PATCH ${LIBROSCO_VER_PATCH})
    set (CPACK_DEBIAN_PACKAGE_NAME ${CPACK_PACKAGE_NAME})
    set (CPACK_DEBIAN_PACKAGE_MAINTAINER "${PROJECT_AUTHOR}")
    set (CPACK_DEBIAN_PACKAGE_SECTION "Libraries")
    set (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.13)")
    set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set (CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${LIBROSCO_VERSION}-${CMAKE_SYSTEM_NAME}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
    set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set (CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
    include(CPack)
  endif()

endif()

#
# install the public headers
#
install (FILES "${SOURCE_SUBDIR}/rosco.h"
               "${CMAKE_BINARY_DIR}/rosco_version.h"
         DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
         PERMISSIONS
          OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

#
# install the library
#
install (TARGETS rosco
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

#
# install the pkgconfig file
#
if (ENABLE_PKGCONFIG_INSTALL)
  set (LIBROSCO_PC "${CMAKE_BINARY_DIR}/librosco.pc")
  configure_file ("${CMAKE_SOURCE_DIR}/librosco.pc.cmakein" ${LIBROSCO_PC} @ONLY)

  install (FILES "${LIBROSCO_PC}"
           DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
           PERMISSIONS
            OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
else()
  message (STATUS "Skipping installation of pkgconfig file.")
endif()

#
# install the test applications
#
if (ENABLE_TESTAPP_INSTALL)
  install (TARGETS readmems
           RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
  message (STATUS "Skipping installation of test application (readmems).")
endif()

