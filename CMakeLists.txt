project(piserver)
cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-psabi")

find_package(Gettext)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
link_directories(${GTKMM_LIBRARY_DIRS})
include_directories(${GTKMM_INCLUDE_DIRS})

include(CheckIncludeFile)
check_include_file(ldap.h HAVE_LDAP_H)
find_library(LDAP_LIBRARY ldap)

if(NOT HAVE_LDAP_H OR NOT LDAP_LIBRARY)
    message( FATAL_ERROR "Missing libldap. Try 'sudo apt install libldap2-dev'" )
endif()

find_library(CAP_LIBRARY cap)
check_include_file(sys/capability.h HAVE_CAPABILITY_H)
if(NOT HAVE_CAPABILITY_H OR NOT CAP_LIBRARY)
    message( FATAL_ERROR "Missing libcap. Try 'sudo apt install libcap-dev'" )
endif()

# for crypt_r()
find_library(CRYPT_LIBRARY crypt)

# for resolving SRV DNS records
find_library(RESOLV_LIBRARY resolv)

# test if we need libatomic
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <atomic>
    #include <stdint.h>
    int main() {
	std::atomic<int64_t> x;
	x = 1;
	return (int) x;
    }"
    atomicbuiltin)

if (NOT atomicbuiltin)
	find_library(ATOMIC_LIBRARY NAMES atomic libatomic.so.1)
	if (NOT ATOMIC_LIBRARY)
		message( FATAL_ERROR "Missing libatomic while architecture does need it" )
	endif()
endif()

aux_source_directory(src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST} resources.c)
target_link_libraries(${PROJECT_NAME} ${GTKMM_LIBRARIES} ${LDAP_LIBRARY}
    ${CRYPT_LIBRARY} ${RESOLV_LIBRARY} ${CURL_LIBRARY} ${LibArchive_LIBRARIES} ${CAP_LIBRARY} ${ATOMIC_LIBRARY} Threads::Threads -lstdc++fs)

# Languages

if (GETTEXT_FOUND)
    file(GLOB LANGUAGES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} po/*.po)
    GETTEXT_CREATE_TRANSLATIONS(po/piserver.pot ALL ${LANGUAGES})
    add_definitions(-DLOCALEDIR="/usr/share/locale")
endif()

# Resources

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/macros)
include(GlibCompileResourcesSupport)

set(RESOURCE_LIST
    data/firstuse.glade
    data/piserver.glade
    data/addhostdialog.glade
    data/edithostdialog.glade
    data/adduserdialog.glade
    data/edituserdialog.glade
    data/adddistrodialog.glade
    data/addfolderdialog.glade
    data/clonedistrodialog.glade
    data/ldapsettingsdialog.glade
    )

compile_gresources(RESOURCE_FILE
                   XML_OUT
                   TYPE EMBED_C
                   RESOURCES ${RESOURCE_LIST})

add_custom_target(resource ALL DEPENDS ${RESOURCE_FILE})

# Installation

install(TARGETS piserver DESTINATION bin)
install(FILES data/piserver.desktop DESTINATION share/applications)
install(FILES data/org.raspberrypi.piserver.policy DESTINATION share/polkit-1/actions)
install(DIRECTORY DESTINATION /var/lib/piserver)
install(DIRECTORY DESTINATION /var/lib/piserver/os)
install(DIRECTORY DESTINATION /var/lib/piserver/os/shared)
install(DIRECTORY DESTINATION /var/lib/piserver/tftproot)
install(DIRECTORY DESTINATION /var/lib/piserver/scripts)
install(PROGRAMS scripts/postinstall.sh scripts/convert.sh scripts/convert_folder.sh scripts/chroot_image.sh DESTINATION /var/lib/piserver/scripts)
install(FILES scripts/convert_chrooted_cmds scripts/mkhomedir-piserver DESTINATION /var/lib/piserver/scripts)
install(FILES scripts/50-piserver-dhcpcd-hook DESTINATION /lib/dhcpcd/dhcpcd-hooks)
