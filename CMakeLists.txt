#
# tracks
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date Nov-2024
# @license see 'LICENSE' file
#

cmake_minimum_required(VERSION 3.5)
project(tracks)


option(USE_OSMIUM "use the osmium library" TRUE)


enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_VERBOSE_MAKEFILE True)
#set(CMAKE_POSITION_INDEPENDENT_CODE True)

add_compile_options(-std=c++20)
add_compile_options(-Wall -Wextra -Weffc++)


# see: https://cmake.org/cmake/help/latest/module/FindBoost.html
find_package(Boost REQUIRED COMPONENTS system filesystem)
add_compile_options(${Boost_CXX_FLAGS})


if(USE_OSMIUM)
	find_package(ZLIB)
	find_package(BZip2)
	find_package(EXPAT)

	set(OSMIUM_INCLUDE_DIRS ${BZIP2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${EXPAT_INCLUDE_DIRS})
	set(OSMIUM_LIBRARIES ${BZIP2_LIBRARIES} ${ZLIB_LIBRARIES} ${EXPAT_LIBRARIES})

	add_definitions(-D_TRACKS_CFG_USE_OSMIUM_=1)
	message("Using osmium library.")
else()
	set(OSMIUM_INCLUDE_DIRS "")
	set(OSMIUM_LIBRARIES "")
endif()


find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5PrintSupport REQUIRED)

#find_package(Qt6Core REQUIRED)
#find_package(Qt6Gui REQUIRED)
#find_package(Qt6Widgets REQUIRED)
#find_package(Qt6Svg REQUIRED)
#find_package(Qt6PrintSupport REQUIRED)


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

if(TARGET Qt6::Core)
	message(NOTICE "Using Qt 6.")
	list(APPEND QtTargets Qt6::Core Qt6::Gui
		Qt6::Widgets Qt6::Svg Qt6::PrintSupport)
elseif(TARGET Qt5::Core)
	message(NOTICE "Using Qt 5.")
	list(APPEND QtTargets Qt5::Core Qt5::Gui
		Qt5::Widgets Qt5::Svg Qt5::PrintSupport)
endif()


include_directories(
	${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src
	${OSMIUM_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}/..
	${Qt5Svg_INCLUDE_DIRS} ${Qt6Svg_INCLUDE_DIRS}
	ext/
)


# -----------------------------------------------------------------------------
# gui
# -----------------------------------------------------------------------------
set_source_files_properties(
	ext/qcustomplot.cpp ext/qcustomplot.h
	${PROJECT_BINARY_DIR}/tracks_autogen/mocs_compilation.cpp
	PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-extra -Wno-effc++ -w"
)


add_executable(tracks
	# main gui
	src/gui/main.cpp
	src/gui/gui.cpp src/gui/gui.h
	src/gui/globals.cpp src/gui/globals.h
	src/gui/resources.cpp src/gui/resources.h
	src/gui/recent.h src/gui/helpers.h

	# dock widgets
	src/gui/docks/track_browser.cpp src/gui/docks/track_browser.h
	src/gui/docks/track_infos.cpp src/gui/docks/track_infos.h
	src/gui/docks/map.cpp src/gui/docks/map.h

	# dialogs
	src/gui/dialogs/conversions.cpp src/gui/dialogs/conversions.h
	src/gui/dialogs/distances.cpp src/gui/dialogs/distances.h
	src/gui/dialogs/statistics.cpp src/gui/dialogs/statistics.h
	src/gui/dialogs/reports.cpp src/gui/dialogs/reports.h
	src/gui/dialogs/summary.cpp src/gui/dialogs/summary.h
	src/gui/dialogs/about.cpp src/gui/dialogs/about.h
	src/gui/dialogs/settings.cpp src/gui/dialogs/settings.h

	# libs
	src/lib/calc.h src/lib/timepoint.h
	src/lib/track.h src/lib/trackdb.h
	src/lib/map.h
	src/common/types.h

	# external libs
	ext/qcustomplot.cpp ext/qcustomplot.h
)


target_link_libraries(tracks
	${OSMIUM_LIBRARIES}
	${QtTargets}
	Boost::system Boost::filesystem
)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# cli tools
# -----------------------------------------------------------------------------
add_executable(tracks_cli
	src/cli/tracks.cpp
	src/common/types.h

	src/lib/calc.h
	src/lib/track.h src/lib/trackdb.h
)

target_link_libraries(tracks_cli)


add_executable(maps_cli
	src/cli/maps.cpp
	src/common/types.h

	src/lib/map.h
)

target_link_libraries(maps_cli
	${OSMIUM_LIBRARIES}
)
# -----------------------------------------------------------------------------
