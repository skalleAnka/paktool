set(VER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cli/paktoolver.h")
if (EXISTS ${VER_FILE})
	file(STRINGS ${VER_FILE} PAKTOOL_VERSION_MAJOR_LINE REGEX ".+PAKTOOL_MAJOR.+$")
	file(STRINGS ${VER_FILE} PAKTOOL_VERSION_MINOR_LINE REGEX ".+PAKTOOL_MINOR.+$")
	file(STRINGS ${VER_FILE} PAKTOOL_VERSION_PATCH_LINE REGEX ".+PAKTOOL_PATCH.+$")
    string(REGEX MATCH "([0-9]+)" _ ${PAKTOOL_VERSION_MAJOR_LINE})
    set(PAKTOOL_VERSION_MAJOR ${CMAKE_MATCH_1})
    string(REGEX MATCH "([0-9]+)" _ ${PAKTOOL_VERSION_MINOR_LINE})
    set(PAKTOOL_VERSION_MINOR ${CMAKE_MATCH_1})
    string(REGEX MATCH "([0-9]+)" _ ${PAKTOOL_VERSION_PATCH_LINE})
    set(PAKTOOL_VERSION_PATCH ${CMAKE_MATCH_1})
	set(PAKTOOL_VERSION ${PAKTOOL_VERSION_MAJOR}.${PAKTOOL_VERSION_MINOR}.${PAKTOOL_VERSION_PATCH})
	unset(PAKTOOL_VERSION_MAJOR_LINE)
	unset(PAKTOOL_VERSION_MINOR_LINE)
	unset(PAKTOOL_VERSION_PATCH_LINE)
	unset(PAKTOOL_VERSION_MAJOR)
	unset(PAKTOOL_VERSION_MINOR)
	unset(PAKTOOL_VERSION_PATCH)
endif()
unset(VER_FILE)
