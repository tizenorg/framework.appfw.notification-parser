CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET (this_target notifications)

INCLUDE(FindPkgConfig)
pkg_check_modules(bin_pkgs REQUIRED libxml-2.0 dlog sqlite3 db-util pkgmgr-info)

FOREACH(flag ${bin_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)


SET (${this_target}_SOURCE_FILES
	src/notification_register.c
	)

SET(EXTRA_CFLAGS  "${EXTRA_CFLAGS} -Wall" )

## SET C COMPILER FLAGS
SET(CMAKE_C_FLAGS "${OSP_DEBUG_FLAGS} ${OSP_OPT_FLAGS} ${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS}")

## SET CPP COMPILER FLAGS
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
SET(CMAKE_CXX_FLAGS "${OSP_DEBUG_FLAGS} ${OSP_OPT_FLAGS} ${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS}")

## Create Library
ADD_LIBRARY (${this_target} SHARED ${${this_target}_SOURCE_FILES})

## SET LINKER FLAGS
SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined -Wl,--as-needed")

TARGET_LINK_LIBRARIES(${this_target} ${bin_pkgs_LDFLAGS})

CONFIGURE_FILE(notification-parser.pc.in ${CMAKE_SOURCE_DIR}/notification-parser.pc @ONLY)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/notification-parser.pc DESTINATION lib/pkgconfig)
INSTALL(TARGETS ${this_target} DESTINATION "etc/package-manager/parserlib")
