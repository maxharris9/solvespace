cmake_minimum_required (VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ENABLE_COVERAGE OFF)

if (ENABLE_COVERAGE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

project(SolveSpace)

# can't just set this because it'll emit -std=gnu++20 with no way to stop it :grimace:
# set_property(TARGET SolveSpace PROPERTY CXX_STANDARD 20)

include(cmake/ClangFormat.cmake) # adds clang-format target

add_definitions(-g -gdwarf-4 -fpermissive -Wno-deprecated-enum-float-conversion -Wno-conversion-null -Wno-deprecated-declarations -Wno-deprecated-enum-enum-conversion -Wno-register -std=c++20)

add_subdirectory(res)
add_subdirectory(extlib/libdxfrw)

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h
)

add_library(solvespace-core OBJECT
	src/expr.cpp
	src/group.cpp
	src/request.cpp
	src/resource.cpp
	src/ttf.cpp
	src/sketch.cpp
	src/style.cpp
	src/system.cpp
	src/util.cpp

	src/entity/entity.cpp
	src/entity/bandedmatrix.cpp

	src/geometry/bsp.cpp
	src/geometry/mesh.cpp
	src/geometry/polygon.cpp
	src/geometry/polyline.cpp
	src/geometry/quaternion.cpp
	src/geometry/point2d.cpp
	src/geometry/vector4.cpp
	src/geometry/bbox.cpp

	src/constraint/constraint.cpp
	src/constraint/draw.cpp
	src/constraint/menu.cpp

	src/import/importdxf.cpp
	src/import/importidf.cpp
	src/import/importmesh.cpp

#	src/tw/view.cpp
#	src/tw/textscreens.cpp
#	src/tw/textwindow.cpp
#	src/tw/confscreen.cpp
#	src/tw/style.cpp

	src/gw/draw.cpp
	src/gw/graphicswindow.cpp
	src/gw/mouse.cpp
	src/gw/modify.cpp

	src/render/render.cpp
	src/render/render2d.cpp
	src/render/renderagg.cpp
	src/render/matrix.cpp

	src/ssui/clipboard.cpp
	src/ssui/export.cpp
	src/ssui/file.cpp
	src/ssui/generate.cpp
	src/ssui/solvespaceui.cpp
	src/ssui/undoredo.cpp
	src/ssui/menu.cpp

	src/platform/gui.cpp
	src/platform/platform.cpp

	src/srf/boolean.cpp
	src/srf/curve.cpp
	src/srf/merge.cpp
	src/srf/ratpoly.cpp
	src/srf/raycast.cpp
	src/srf/shell.cpp
	src/srf/surface.cpp
	src/srf/surfinter.cpp
	src/srf/triangulate.cpp

	src/filewriter/dxffilewriter.cpp
	src/filewriter/epsfilewriter.cpp
	src/filewriter/hpglfilewriter.cpp
	src/filewriter/pdffilewriter.cpp
	src/filewriter/step2dfilewriter.cpp
	src/filewriter/stepfilewriter.cpp
	src/filewriter/gcodefilewriter.cpp
	src/filewriter/svgfilewriter.cpp
	src/filewriter/vectorfilewriter.cpp
	src/filewriter/stipplepattern.cpp
)

target_include_directories(solvespace-core PRIVATE
	${FREETYPE_INCLUDE_DIRS}
	/boot/home/cppfront/include/
	/boot/system/develop/headers/agg2/
	src/
	extlib/libdxfrw/
	${CMAKE_CURRENT_BINARY_DIR} # adds config.h/config.in.h
)
target_compile_definitions(solvespace-core PRIVATE HAIKU_GUI=1)

add_executable(solvespace
	src/platform/haiku/App.cpp
	src/platform/haiku/MainWindow.cpp
	src/platform/haiku/Toolbar.cpp
	src/platform/haiku/PropertyBrowser.cpp
	src/platform/haiku/ListView/DragSortableListView.cpp
	src/platform/haiku/ListView/ThumbListItem.cpp
	src/platform/haiku/GroupsListView.cpp
	src/platform/haiku/HVIFUtil.cpp
	src/platform/haiku/EditorView.cpp
	src/platform/haiku/ViewParameters.cpp
	src/platform/haiku/guihaiku.cpp
	src/platform/haiku/HaikuSpaceUI.cpp
	src/platform/haiku/EventHooksHaiku.cpp
	src/platform/haiku/KeyboardShortcuts.cpp
	$<TARGET_OBJECTS:solvespace-core>
)

target_include_directories(solvespace PRIVATE
	/boot/home/cppfront/include/
	/boot/system/develop/headers/agg2/
	src/
	${CMAKE_CURRENT_BINARY_DIR} # adds config.h/config.in.h
)

target_link_libraries(solvespace be agg translation tracker dxfrw ${ZLIB_LIBRARY} ${PNG_LIBRARY} ${FREETYPE_LIBRARY})
target_compile_definitions(solvespace PRIVATE HAIKU_GUI=1)

set_target_properties(solvespace PROPERTIES OUTPUT_NAME "SolveSpace")
add_custom_command(TARGET solvespace POST_BUILD
	COMMENT "rebuilding icon resources"
	COMMAND rc -o ${CMAKE_BINARY_DIR}/Resources.rsrc ${CMAKE_SOURCE_DIR}/src/platform/haiku/Resources.rdef
	COMMAND xres -o ${CMAKE_BINARY_DIR}/SolveSpace ${CMAKE_BINARY_DIR}/Resources.rsrc
		-a VICN:1:line-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/line-tool.hvif
		-a VICN:2:rect-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/rect-tool.hvif
		-a VICN:3:circle-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/circle-tool.hvif
		-a VICN:4:arc-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/arc-tool.hvif
		-a VICN:5:tangent-arc-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/tangent-arc-tool.hvif
		-a VICN:6:cubic-spline-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/cubic-spline-tool.hvif
		-a VICN:7:datum-point-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/datum-point-tool.hvif
		-a VICN:8:construction-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/construction-tool.hvif
		-a VICN:9:split-curves-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/split-curves-tool.hvif
		-a VICN:10:text-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/text-tool.hvif
		-a VICN:11:distance-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/distance-tool.hvif
		-a VICN:12:angle-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/angle-tool.hvif
		-a VICN:13:horizontal-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/horizontal-tool.hvif
		-a VICN:14:vertical-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/vertical-tool.hvif
		-a VICN:15:parallel-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/parallel-tool.hvif
		-a VICN:16:open-eye ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/open-eye.hvif
		-a VICN:17:closed-eye ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/closed-eye.hvif
		-a VICN:18:nearest-iso-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/nearest-iso-tool.hvif
		-a VICN:19:nearest-ortho-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/nearest-ortho-tool.hvif
		-a VICN:20:constrain-perpendicular-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/constrain-perpendicular-tool.hvif
		-a VICN:21:constrain-point-on-line-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/constrain-point-on-line-tool.hvif
		-a VICN:22:constrain-symmetric-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/constrain-symmetric-tool.hvif
		-a VICN:23:constrain-equal-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/constrain-equal-tool.hvif
		-a VICN:24:constrain-parallel-normals-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/constrain-parallel-normals-tool.hvif
		-a VICN:25:other-supplementary-angle-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/other-supplementary-angle-tool.hvif
		-a VICN:26:ref-tool ${CMAKE_SOURCE_DIR}/src/platform/haiku/resources/icons/ref-tool.hvif
	COMMAND mimeset -f ${CMAKE_BINARY_DIR}/SolveSpace
)

# test suite
set(testsuite_sources
	test/unit/striangle-test.cpp
	test/unit/constraint-test.cpp
	test/analysis/contour_area/test.cpp
	test/core/expr/test.cpp
	test/core/locale/test.cpp
	test/core/path/test.cpp
	test/constraint/points_coincident/test.cpp
	test/constraint/pt_pt_distance/test.cpp
	test/constraint/pt_plane_distance/test.cpp
	test/constraint/pt_line_distance/test.cpp
	test/constraint/pt_face_distance/test.cpp
	test/constraint/proj_pt_distance/test.cpp
	test/constraint/pt_in_plane/test.cpp
	test/constraint/pt_on_line/test.cpp
	test/constraint/pt_on_face/test.cpp
	test/constraint/equal_length_lines/test.cpp
	test/constraint/length_ratio/test.cpp
	test/constraint/eq_len_pt_line_d/test.cpp
	test/constraint/eq_pt_ln_distances/test.cpp
	test/constraint/equal_angle/test.cpp
	test/constraint/equal_line_arc_len/test.cpp
	test/constraint/length_difference/test.cpp
	test/constraint/symmetric/test.cpp
	test/constraint/symmetric_horiz/test.cpp
	test/constraint/symmetric_vert/test.cpp
	test/constraint/symmetric_line/test.cpp
	test/constraint/at_midpoint/test.cpp
	test/constraint/horizontal/test.cpp
	test/constraint/vertical/test.cpp
	test/constraint/diameter/test.cpp
	test/constraint/pt_on_circle/test.cpp
	test/constraint/same_orientation/test.cpp
	test/constraint/angle/test.cpp
	test/constraint/parallel/test.cpp
	test/constraint/perpendicular/test.cpp
	test/constraint/arc_line_tangent/test.cpp
	test/constraint/cubic_line_tangent/test.cpp
	test/constraint/curve_curve_tangent/test.cpp
	test/constraint/equal_radius/test.cpp
	test/constraint/where_dragged/test.cpp
	test/constraint/comment/test.cpp
	test/request/arc_of_circle/test.cpp
	test/request/circle/test.cpp
	test/request/cubic/test.cpp
	test/request/cubic_periodic/test.cpp
	test/request/datum_point/test.cpp
	test/request/image/test.cpp
	test/request/line_segment/test.cpp
	test/request/ttf_text/test.cpp
	test/request/workplane/test.cpp
	test/group/link/test.cpp
	test/group/translate_asy/test.cpp
	test/group/translate_nd/test.cpp
)

add_executable(solvespace-testsuite
	src/platform/haiku/HaikuSpaceUI.cpp
	test/harness.cpp
	${testsuite_sources}
	src/platform/guinone.cpp
	src/platform/EventHooks.cpp
	$<TARGET_OBJECTS:solvespace-core>
	$<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_compile_definitions(solvespace-testsuite PRIVATE HAIKU_GUI=1)
target_link_libraries(solvespace-testsuite
	be agg tracker dxfrw ${ZLIB_LIBRARY} ${PNG_LIBRARY} ${FREETYPE_LIBRARY} ${COVERAGE_LIBRARY})

target_include_directories(solvespace-testsuite
	PRIVATE
	${EIGEN3_INCLUDE_DIRS}
	/boot/home/cppfront/include/
	/boot/system/develop/headers/agg2/
	src/
	test/)

add_dependencies(solvespace-testsuite resources)

add_custom_target(test-solvespace
	COMMAND $<TARGET_FILE:solvespace-testsuite>
	COMMENT "Testing SolveSpace"
	VERBATIM)

# debug runner
add_executable(solvespace-debugtool
	test/debugtool.cpp
	src/platform/guinone.cpp
	$<TARGET_OBJECTS:solvespace-core>
	$<TARGET_PROPERTY:resources>)

target_link_libraries(solvespace-debugtool solvespace-core dxfrw agg ${ZLIB_LIBRARY} ${PNG_LIBRARY} ${FREETYPE_LIBRARY})

target_include_directories(solvespace-debugtool PRIVATE
	/boot/home/cppfront/include/
	/boot/system/develop/headers/agg2/
	src/
)

add_dependencies(solvespace-debugtool resources)

# coverage reports
if(ENABLE_COVERAGE)
	find_program(GCOV gcov)
	find_program(LCOV lcov)
	find_program(GENHTML genhtml)

	set(LCOV_FLAGS   -q --gcov-tool ${GCOV})
	set(LCOV_FLAGS   ${LCOV_FLAGS} --rc lcov_branch_coverage=1)
	set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_line=(ssassert|switch)")
	set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_br_line=BRANCH_ALWAYS_TAKEN")
	set(LCOV_COLLECT -c -b ${CMAKE_SOURCE_DIR}/src --no-external -d ${CMAKE_CURRENT_BINARY_DIR}/)

	add_custom_command(
		OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
		COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
			-o ${CMAKE_BINARY_DIR}/coverage_base.info -i
		DEPENDS solvespace-testsuite
		COMMENT "Importing baseline coverage data"
		VERBATIM)

	add_custom_target(coverage_solvespace ALL
		COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
			-o ${CMAKE_BINARY_DIR}/coverage_test.info
		COMMAND ${LCOV} ${LCOV_FLAGS}
			-o ${CMAKE_BINARY_DIR}/coverage_full.info
			-a ${CMAKE_BINARY_DIR}/coverage_base.info
			-a ${CMAKE_BINARY_DIR}/coverage_test.info
		COMMAND ${LCOV} ${LCOV_FLAGS} --summary
			${CMAKE_BINARY_DIR}/coverage_full.info
		COMMAND ${GENHTML} -q --branch-coverage --demangle-cpp --legend --ignore-errors source
			${CMAKE_BINARY_DIR}/coverage_full.info
			-o ${CMAKE_BINARY_DIR}/coverage/
			-t "SolveSpace testbench"
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
		DEPENDS test-solvespace
		COMMENT "Generating coverage report"
		VERBATIM)
endif()

# print cmake targets (useful for debugging this file)
# if there's a straightforward command for this, *please* let me know about it
################################################################################
#get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
#message("available targets:")
#foreach(item IN LISTS targets)
#	message("- ${item}")
#endforeach()