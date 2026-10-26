cmake_minimum_required(VERSION 3.16)

project(DentalLabeler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg Test)

# LGPLv3 배포 조건: Qt 는 공유 라이브러리로만 링크한다 (docs/LicenseCompliance.md)
foreach(QtModule Core Gui Widgets Concurrent Svg)
    get_target_property(QtModuleType Qt5::${QtModule} TYPE)
    if(NOT QtModuleType STREQUAL "SHARED_LIBRARY")
        message(FATAL_ERROR "Qt5::${QtModule} 이 공유 라이브러리가 아니다. LGPLv3 동적 링크 조건을 지킬 수 없다.")
    endif()
endforeach()

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 비즈니스 계층 (Core · Interface · Algorithm · Document · Thread) 과 Presenter. QtWidgets 를 쓰지 않는다
add_library(DentalLabelerCore STATIC
    src/Core/Types.h
    src/Core/LabelTable.h
    src/Core/LabelTable.cpp
    src/Core/Observers.h
    src/Core/Observers.cpp
    src/Core/SystemStatus.h
    src/Core/SystemStatus.cpp
    src/Interface/IPaintStrategy.h
    src/Document/Mesh.h
    src/Document/Mesh.cpp
    src/Document/PlyReader.h
    src/Document/PlyReader.cpp
    src/Document/PlyWriter.h
    src/Document/PlyWriter.cpp
    src/Document/FileHash.h
    src/Document/FileHash.cpp
    src/Document/LabelDocument.h
    src/Document/LabelDocument.cpp
    src/Document/LabelFile.h
    src/Document/LabelFile.cpp
    src/Document/EditHistory.h
    src/Document/EditHistory.cpp
    src/Document/CaseData.h
    src/Algorithm/Geometry/ParallelFor.h
    src/Algorithm/Geometry/MeshTopology.h
    src/Algorithm/Geometry/MeshTopology.cpp
    src/Algorithm/Geometry/Bvh.h
    src/Algorithm/Geometry/Bvh.cpp
    src/Algorithm/Geometry/Curvature.h
    src/Algorithm/Geometry/Curvature.cpp
    src/Algorithm/Geometry/DentalFrame.h
    src/Algorithm/Geometry/DentalFrame.cpp
    src/Algorithm/LabelPaint/LabelPaint.h
    src/Algorithm/LabelPaint/LabelPaint.cpp
    src/Algorithm/LabelPaint/Common/PaintPolicy.h
    src/Algorithm/LabelPaint/Common/PaintPolicy.cpp
    src/Algorithm/LabelPaint/Strategy/BrushPaintCPU.h
    src/Algorithm/LabelPaint/Strategy/BrushPaintCPU.cpp
    src/Algorithm/LabelPaint/Strategy/FillPaintCPU.h
    src/Algorithm/LabelPaint/Strategy/FillPaintCPU.cpp
    src/Algorithm/LabelPaint/Strategy/LassoPaintCPU.h
    src/Algorithm/LabelPaint/Strategy/LassoPaintCPU.cpp
    src/Thread/MeshLoadThread.h
    src/Thread/MeshLoadThread.cpp
    src/Presenter/Workspace/WorkspaceTypes.h
    src/Presenter/Workspace/WorkspacePresenter.h
    src/Presenter/Workspace/WorkspacePresenter.cpp
)
target_include_directories(DentalLabelerCore PUBLIC src)
target_link_libraries(DentalLabelerCore PUBLIC Qt5::Core Qt5::Gui Qt5::Concurrent)
if(NOT MSVC)
    find_package(Threads REQUIRED)
    target_link_libraries(DentalLabelerCore PUBLIC Threads::Threads)
endif()

option(DL_BUILD_APP "UI 앱도 빌드한다 (끄면 Core 와 테스트만)" ON)

if(DL_BUILD_APP)
# 앱: 조립 지점(main · Application) 과 UI
add_executable(DentalLabeler WIN32
    src/main.cpp
    src/Core/Application.h
    src/Core/Application.cpp
    src/UI/Common/OrbitCamera.h
    src/UI/Common/OrbitCamera.cpp
    src/UI/Common/LabelKeyInput.h
    src/UI/Common/LabelKeyInput.cpp
    src/UI/Common/Theme/IconFactory.h
    src/UI/Common/Theme/IconFactory.cpp
    src/UI/Common/Control/OdontogramWidget.h
    src/UI/Common/Control/OdontogramWidget.cpp
    src/UI/View/Workspace/Viewport3DWidget.h
    src/UI/View/Workspace/Viewport3DWidget.cpp
    src/UI/View/Workspace/ToolRailView.h
    src/UI/View/Workspace/ToolRailView.cpp
    src/UI/View/Workspace/ToolOptionsView.h
    src/UI/View/Workspace/ToolOptionsView.cpp
    src/UI/View/Workspace/ToolOptionsView.ui
    src/UI/View/Workspace/LabelPanelView.h
    src/UI/View/Workspace/LabelPanelView.cpp
    src/UI/View/Workspace/LabelPanelView.ui
    src/UI/Skin/Default/MainWindow.h
    src/UI/Skin/Default/MainWindow.cpp
    src/UI/Skin/Default/MainWindow.ui
    src/UI/Skin/Default/Default.qrc
)
target_link_libraries(DentalLabeler PRIVATE DentalLabelerCore Qt5::Widgets Qt5::Svg)
target_compile_definitions(DentalLabeler PRIVATE DL_VERSION="${PROJECT_VERSION}")

# windeployqt 로 실행 파일 옆에 Qt DLL 을 놓는다 (배포 · 탐색기에서 실행할 때)
get_target_property(QtQmakePath Qt5::qmake IMPORTED_LOCATION)
get_filename_component(QtBinDir "${QtQmakePath}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QtBinDir}")
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-system-d3d-compiler "$<TARGET_FILE:DentalLabeler>"
        DEPENDS DentalLabeler
        COMMENT "Qt DLL 을 실행 파일 옆에 복사한다")
endif()
endif()

# 테스트
enable_testing()
add_executable(DentalLabelerTests
    tests/TestMain.cpp
    tests/TestPlyReader.cpp
    tests/TestGeometry.cpp
    tests/TestLabelPaint.cpp
    tests/TestLabelDocument.cpp
    tests/TestWorkspacePresenter.cpp
    tests/TestHelpers.h
    tests/TestHelpers.cpp
)
target_link_libraries(DentalLabelerTests PRIVATE DentalLabelerCore Qt5::Test)
target_compile_definitions(DentalLabelerTests PRIVATE DL_SAMPLE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME DentalLabelerTests COMMAND DentalLabelerTests)
