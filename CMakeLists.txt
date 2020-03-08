cmake_minimum_required(VERSION 2.8.12.2)

project(KikoPlay)

# BUILD_VERSION is the current date in YYYYMMDD format. It is only
# used by the nightly version to add the date of the build.
string(TIMESTAMP BUILD_VERSION "%Y%m%d")

# Choose between building a stable version or nightly (the default), depending
# on whether '-DBUILD_STABLE_VERSION=1' is passed on the command line or not.
option(BUILD_STABLE_VERSION "Don't build the stable version by default" OFF)
if(NOT BUILD_STABLE_VERSION)
    add_definitions(-DBUILD_VERSION=${BUILD_VERSION})
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

OPTION(ALL_WARNINGS "Enable some useful warning flags" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

add_definitions(-std=c++11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()
# Fix warning of AUTOMOC behavior
if (POLICY CMP0071)
    # Use of AUTOMOC is set to old for now for consistency.
    # Affects of 'new' behavior is as yet to be determined.
    # https://cmake.org/cmake/help/latest/policy/CMP0071.html
    cmake_policy(SET CMP0071 OLD)
endif()

#message(WARNING $ENV{QTDIR64})

if(WIN32)
	if (QTDIR OR DEFINED ENV{QTDIR} OR DEFINED ENV{QTDIR32} OR DEFINED ENV{QTDIR64})
		# Qt path set by user or env var
	else()
		set(QTDIR "" CACHE PATH "Path to Qt (e.g. C:/Qt/5.7/msvc2015_64)")
		message(WARNING "QTDIR variable is missing.  Please set this variable to specify path to Qt (e.g. C:/Qt/5.7/msvc2015_64)")
	endif()
endif()

#message(WARNING $ENV{QTDIR})

if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
endif()

if(MSVC)
	add_compile_options("/MP")
	set(CMAKE_C_FLAGS_DEBUG "/DDEBUG=1 /D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_DEBUG "/DDEBUG=1 /D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")

	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")
	endif()
else()
	if(MINGW)
		set(CMAKE_WIDL "widl" CACHE STRING "wine IDL header file generation program")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WIN32_WINNT=0x0600 -DWINVER=0x0600")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32_WINNT=0x0600 -DWINVER=0x0600")
	endif()
	set(CMAKE_C_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG=1 -D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

#message(WARNING $ENV{QTDIR${_lib_suffix}})
if(DEFINED QTDIR${_lib_suffix})
	list(APPEND CMAKE_PREFIX_PATH "${QTDIR${_lib_suffix}}")
	#message(WARNING "111111")
elseif(DEFINED QTDIR)
	list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
	#message(WARNING "2222")
elseif(DEFINED ENV{QTDIR${_lib_suffix}})
	list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR${_lib_suffix}}")
	#message(WARNING "33333")
elseif(DEFINED ENV{QTDIR})
	list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
	#message(STATUS "appxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")
endif()

find_package(Qt5 REQUIRED COMPONENTS Concurrent Gui LinguistTools Network Sql Test Widgets Xml)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(PROJ_HDR
	Common/kcache.h
    Download/autodownloadmanager.h
    Play/Danmu/danmuviewmodel.h
    Play/Danmu/eventanalyzer.h
    UI/addpool.h
    UI/addrule.h
    UI/autodownloadwindow.h
    UI/danmuview.h
    UI/inputdialog.h
    UI/mainwindow.h
    UI/framelesswindow.h
    Play/Danmu/Layouts/bottomlayout.h
    Play/Danmu/Layouts/danmulayout.h
    Play/Danmu/Layouts/rolllayout.h
    Play/Danmu/Layouts/toplayout.h
    Play/Danmu/danmupool.h
    UI/widgets/clickslider.h
    UI/widgets/dialogtip.h
    UI/widgets/fonticontoolbutton.h
    globalobjects.h
    Play/Playlist/playlist.h
    Play/Video/mpvplayer.h
    UI/list.h
    UI/player.h
    UI/pooleditor.h
    UI/framelessdialog.h
    Play/Danmu/Provider/localprovider.h
    UI/adddanmu.h
    Play/Danmu/common.h
    UI/matcheditor.h
    Play/Danmu/Provider/bilibiliprovider.h
    Play/Danmu/Provider/info.h
    UI/selectepisode.h
    Play/Danmu/Provider/dandanprovider.h
    Play/Danmu/blocker.h
    UI/blockeditor.h
    UI/capture.h
    UI/mediainfo.h
    UI/about.h
    Play/Danmu/Provider/tucaoprovider.h
    Play/Danmu/Provider/providerbase.h
    Play/Danmu/providermanager.h
    Play/Danmu/Provider/bahamutprovider.h
    Play/Danmu/Provider/dililiprovider.h
    MediaLibrary/animelibrary.h
    Common/network.h
    Common/htmlparsersax.h
    MediaLibrary/animeinfo.h
    MediaLibrary/animeitemdelegate.h
    UI/librarywindow.h
    UI/bangumisearch.h
    MediaLibrary/episodesmodel.h
    Download/util.h
    Download/aria2jsonrpc.h
    UI/widgets/dirselectwidget.h
    Download/downloaditemdelegate.h
    Download/downloadmodel.h
    Download/torrent.h
    UI/downloadwindow.h
    UI/adduritask.h
    UI/selecttorrentfile.h
    UI/downloadsetting.h
    UI/poolmanager.h
    UI/checkupdate.h
    Play/Danmu/Provider/iqiyiprovider.h
    Common/zconf.h
    Common/zlib.h
    Common/flowlayout.h
    UI/animedetailinfo.h
    UI/timelineedit.h
    Play/Danmu/Provider/acfunprovider.h
    UI/mpvparametersetting.h
    UI/mpvlog.h
    LANServer/lanserver.h
    LANServer/httpserver.h
    UI/serversettting.h
    Play/Playlist/playlistitem.h
    Play/Playlist/playlistprivate.h
    Play/Danmu/Render/cacheworker.h
    Play/Danmu/Render/danmurender.h
    Play/Danmu/Manager/danmumanager.h
    Play/Danmu/Manager/nodeinfo.h
    Play/Danmu/Manager/managermodel.h
    MediaLibrary/animeworker.h
    MediaLibrary/animemodel.h
    MediaLibrary/labelmodel.h
    MediaLibrary/animefilterproxymodel.h
    MediaLibrary/labelitemdelegate.h
    UI/bangumiupdate.h
    Play/Danmu/Provider/youkuprovider.h
    Play/Danmu/Provider/tencentprovider.h
    Download/BgmList/bgmlist.h
    UI/bgmlistwindow.h
    Download/Script/scriptmanager.h
    UI/ressearchwindow.h
    UI/managescript.h
    Play/Danmu/Provider/pptvprovider.h
    Play/Danmu/Manager/pool.h
    Common/threadtask.h
    MediaLibrary/capturelistmodel.h
    UI/captureview.h
    UI/tip.h
)

set(PROJ_SRC
	Download/autodownloadmanager.cpp
    Play/Danmu/eventanalyzer.cpp
    UI/addpool.cpp
    UI/addrule.cpp
    UI/autodownloadwindow.cpp
    UI/danmuview.cpp
    UI/inputdialog.cpp
    UI/widgets/clickslider.cpp
    UI/widgets/dialogtip.cpp
    UI/widgets/fonticontoolbutton.cpp
        main.cpp
    UI/mainwindow.cpp
    UI/framelesswindow.cpp
    Play/Danmu/Layouts/bottomlayout.cpp
    Play/Danmu/Layouts/rolllayout.cpp
    Play/Danmu/Layouts/toplayout.cpp
    Play/Danmu/danmupool.cpp
    globalobjects.cpp
    Play/Playlist/playlist.cpp
    Play/Video/mpvplayer.cpp
    UI/list.cpp
    UI/player.cpp
    UI/pooleditor.cpp
    UI/framelessdialog.cpp
    Play/Danmu/Provider/localprovider.cpp
    UI/adddanmu.cpp
    UI/matcheditor.cpp
    Play/Danmu/Provider/bilibiliprovider.cpp
    UI/selectepisode.cpp
    Play/Danmu/Provider/dandanprovider.cpp
    Play/Danmu/blocker.cpp
    UI/blockeditor.cpp
    UI/capture.cpp
    UI/mediainfo.cpp
    Play/Danmu/common.cpp
    UI/about.cpp
    Play/Danmu/Provider/tucaoprovider.cpp
    Play/Danmu/providermanager.cpp
    Play/Danmu/Provider/bahamutprovider.cpp
    Play/Danmu/Provider/dililiprovider.cpp
    MediaLibrary/animelibrary.cpp
    Common/network.cpp
    Common/htmlparsersax.cpp
    MediaLibrary/animeitemdelegate.cpp
    UI/librarywindow.cpp
    UI/bangumisearch.cpp
    MediaLibrary/episodesmodel.cpp
    Download/util.cpp
    Download/aria2jsonrpc.cpp
    UI/widgets/dirselectwidget.cpp
    Download/downloaditemdelegate.cpp
    Download/downloadmodel.cpp
    Download/torrent.cpp
    UI/downloadwindow.cpp
    UI/adduritask.cpp
    UI/selecttorrentfile.cpp
    UI/downloadsetting.cpp
    UI/poolmanager.cpp
    UI/checkupdate.cpp
    Play/Danmu/Provider/iqiyiprovider.cpp
    Common/flowlayout.cpp
    UI/animedetailinfo.cpp
    UI/timelineedit.cpp
    Play/Danmu/Provider/acfunprovider.cpp
    UI/mpvparametersetting.cpp
    UI/mpvlog.cpp
    LANServer/lanserver.cpp
    LANServer/httpserver.cpp
    UI/serversettting.cpp
    Play/Playlist/playlistitem.cpp
    Play/Playlist/playlistprivate.cpp
    Play/Danmu/Render/cacheworker.cpp
    Play/Danmu/Render/danmurender.cpp
    Play/Danmu/Manager/danmumanager.cpp
    Play/Danmu/Manager/nodeinfo.cpp
    Play/Danmu/Manager/managermodel.cpp
    MediaLibrary/animeworker.cpp
    MediaLibrary/animemodel.cpp
    MediaLibrary/labelmodel.cpp
    MediaLibrary/animefilterproxymodel.cpp
    MediaLibrary/labelitemdelegate.cpp
    UI/bangumiupdate.cpp
    Play/Danmu/Provider/youkuprovider.cpp
    Play/Danmu/Provider/tencentprovider.cpp
    Download/BgmList/bgmlist.cpp
    UI/bgmlistwindow.cpp
    Download/Script/scriptmanager.cpp
    UI/ressearchwindow.cpp
    UI/managescript.cpp
    Play/Danmu/Provider/pptvprovider.cpp
    Play/Danmu/Manager/pool.cpp
    MediaLibrary/capturelistmodel.cpp
    UI/captureview.cpp
    UI/tip.cpp
)

set(MPV_DIR Play/Video)
include_directories(${MPV_DIR})

set(LANSERVER_DIR LANServer)
include_directories(${LANSERVER_DIR})

set(LUA_DIR Download/Script/lua)
include_directories(${LUA_DIR})

set(HTTPENGINE_DIR lib/qhttpengine)
add_subdirectory(${HTTPENGINE_DIR})
include_directories(${QCUSTOMPLOT_DIR}/src/include)

set(PROJ_FORMS
	src/AboutDialog.ui
)

set(PROJ_RESOURCES
	res.qrc
)

# Translation files
set(PROJ_TSS
	"${CMAKE_SOURCE_DIR}/res/lang/zh_CN.ts.ts"
)

qt5_wrap_ui(PROJ_FORM_HDR ${PROJ_FORMS})
if(PROJ_TSS)
	# add translations
	foreach(PROJ_TS ${PROJ_TSS})
		SET_SOURCE_FILES_PROPERTIES("${PROJ_TS}" PROPERTIES OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/src/translations")
	endforeach(PROJ_TS ${PROJ_TSS})
	qt5_add_translation(PROJ_QMS ${PROJ_TSS})
endif(PROJ_TSS)
qt5_add_resources(PROJ_RESOURCES_RCC ${PROJ_RESOURCES})

#icon and correct libs/subsystem for windows
if(WIN32)
        #enable version check for windows
        add_definitions(-DCHECKNEWVERSION)

	IF( MINGW )
	# resource compilation for MinGW
		ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sqlbicon.o"
			COMMAND windres "-I${CMAKE_CURRENT_SOURCE_DIR}" "-i${CMAKE_CURRENT_SOURCE_DIR}/src/winapp.rc" -o "${CMAKE_CURRENT_BINARY_DIR}/sqlbicon.o" VERBATIM)
		set(SQLB_SRC ${SQLB_SRC} "${CMAKE_CURRENT_BINARY_DIR}/sqlbicon.o")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows")
		set(WIN32_STATIC_LINK -Wl,-Bstatic -lssl -lcrypto -lws2_32)
		set(ADDITIONAL_LIBS lzma)
	ELSE( MINGW )
		set(SQLB_SRC ${SQLB_SRC} "${CMAKE_CURRENT_SOURCE_DIR}/src/winapp.rc")
	ENDIF( MINGW )
else()
	set(LPTHREAD pthread)
endif(WIN32)

#enable version check for MacOS
if(APPLE)
	add_definitions(-DCHECKNEWVERSION)
endif(APPLE)

# 添加第三方库依赖语句必须在add_executable前
link_directories(${CMAKE_SOURCE_DIR}/lib/x64)

add_executable(${PROJECT_NAME}
		${PROJ_HDR}
		${PROJ_SRC}
		${PROJ_FORM_HDR}
		${PROJ_MOC}
		${PROJ_RESOURCES_RCC}
	)
	
add_dependencies(${PROJECT_NAME} qhttpengine)

# Warnings
if (ALL_WARNINGS AND CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wdouble-promotion -Wformat=2 -Wlogical-op -Wuseless-cast)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
        target_compile_options(${PROJECT_NAME} PRIVATE -Wnull-dereference -Wduplicated-cond -Wduplicated-branches)
    endif()
endif()

set(QT_LIBS Qt5::Gui Qt5::Test Qt5::Widgets Qt5::Network Qt5::Concurrent Qt5::Xml Qt5::Sql)

set(PROJ_DEP_LIBS libmpv lua53 zlibstat qhttpengine)

target_link_libraries(${PROJECT_NAME}
    ${QT_LIBS}
	${PROJ_DEP_LIBS}
    ${WIN32_STATIC_LINK}
    ${ADDITIONAL_LIBS})
	
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:${PROJECT_NAME}>
	#COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Network> $<TARGET_FILE_DIR:${PROJECT_NAME}>
	#COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Concurrent> $<TARGET_FILE_DIR:${PROJECT_NAME}>
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:qhttpengine> $<TARGET_FILE_DIR:${PROJECT_NAME}>
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/lib/runtime/mpv-1.dll $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
	
if (WIN32)
    get_target_property(QT5_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT5_WINDEPLOYQT_EXECUTABLE ${QT5_QMAKE_EXECUTABLE} PATH)
    set(QT5_WINDEPLOYQT_EXECUTABLE "${QT5_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
       COMMAND ${QT5_WINDEPLOYQT_EXECUTABLE} --qmldir ${CMAKE_SOURCE_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif(WIN32)

if(NOT WIN32)
	install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib)
endif()

if(WIN32 AND MSVC)
	install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION "/"
		LIBRARY DESTINATION lib)

	set(QT5_BIN_PATH ${QTDIR}/bin)
	# The Qt5 Debug configuration library files have a 'd' postfix
	install(FILES
			${QT5_BIN_PATH}/Qt5Cored.dll
			${QT5_BIN_PATH}/Qt5Guid.dll
			${QT5_BIN_PATH}/Qt5Networkd.dll
			${QT5_BIN_PATH}/Qt5PrintSupportd.dll
			${QT5_BIN_PATH}/Qt5Widgetsd.dll
			${QT5_BIN_PATH}/Qt5Concurrentd.dll
		DESTINATION "/"
		CONFIGURATIONS Debug)
	# The Qt5 Release configuration files don't have a postfix
	install(FILES
			${QT5_BIN_PATH}/Qt5Core.dll
			${QT5_BIN_PATH}/Qt5Gui.dll
			${QT5_BIN_PATH}/Qt5Network.dll
			${QT5_BIN_PATH}/Qt5PrintSupport.dll
			${QT5_BIN_PATH}/Qt5Widgets.dll
			${QT5_BIN_PATH}/Qt5Concurrent.dll
		DESTINATION "/"
		CONFIGURATIONS Release)
	# The files below are common to all configurations
	install(FILES
			${OPENSSL_PATH}/libeay32.dll
			${OPENSSL_PATH}/ssleay32.dll
		DESTINATION "/")
	install(FILES
		${QT5_PATH}/plugins/platforms/qwindows.dll
		DESTINATION platforms)

	# The XML dll
	install(FILES
			"${QT5_PATH}/bin/Qt5Xmld.dll"
		DESTINATION "/"
		CONFIGURATIONS Debug)
	install(FILES
			"${QT5_PATH}/bin/Qt5Xml.dll"
		DESTINATION "/"
		CONFIGURATIONS Release)

	# The image format plugins
	install(FILES
			${WIN_IMG_PLUGINS_DEBUG}
		DESTINATION imageformats
		CONFIGURATIONS Debug)
	install(FILES
			${WIN_IMG_PLUGINS}
		DESTINATION imageformats
		CONFIGURATIONS Release)

	# The license files
	install(FILES
			${LICENSE_FILES}
		DESTINATION licenses)

	# The batch file launcher
	install(FILES
			distri/winlaunch.bat
		DESTINATION "/")
endif()
