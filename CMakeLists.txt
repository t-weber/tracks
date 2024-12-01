#
# tracks
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date Nov-2024
# @license see 'LICENSE' file
#

cmake_minimum_required(VERSION 3.5)
project(tracks)


enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_VERBOSE_MAKEFILE True)
#set(CMAKE_POSITION_INDEPENDENT_CODE True)

add_compile_options(-std=c++20)
add_compile_options(-Wall -Wextra -Weffc++)


# see: https://cmake.org/cmake/help/latest/module/FindBoost.html
find_package(Boost REQUIRED COMPONENTS system filesystem)
add_compile_options(${Boost_CXX_FLAGS})


find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5PrintSupport REQUIRED)

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
	${Boost_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}/..
	${Qt5Svg_INCLUDE_DIRS}
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
	src/gui/main.cpp
	src/gui/gui.cpp src/gui/gui.h
	src/gui/settings.cpp src/gui/settings.h
	src/gui/globals.cpp src/gui/globals.h
	src/gui/about.cpp src/gui/about.h
	src/gui/resources.cpp src/gui/resources.h
	src/gui/recent.h src/gui/helpers.h
	src/gui/track_browser.cpp src/gui/track_browser.h
	src/gui/track_infos.cpp src/gui/track_infos.h

	src/common/types.h

	src/lib/calc.h
	src/lib/track.h src/lib/trackdb.h

	ext/qcustomplot.cpp ext/qcustomplot.h
)


target_link_libraries(tracks
	${QtTargets}
	Boost::system Boost::filesystem
)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# cli
# -----------------------------------------------------------------------------
add_executable(tracks_cli
	src/cli/tracks.cpp
	src/common/types.h

	src/lib/calc.h
	src/lib/track.h src/lib/trackdb.h
)


target_link_libraries(tracks_cli)
# -----------------------------------------------------------------------------
