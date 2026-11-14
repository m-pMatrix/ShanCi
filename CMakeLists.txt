cmake_minimum_required(VERSION 3.21)

project(ShanCi
        VERSION 1.0.0
        DESCRIPTION "CET-4/6 English Vocabulary Assistant (offline, C++/Qt6)"
        LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 QUIET COMPONENTS Multimedia)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Bundled SQLite (amalgamation)
# ---------------------------------------------------------------------------
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_ENABLE_JSON1=1
    SQLITE_ENABLE_FTS5=1
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DQS=0)
set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Core library: domain + data + service + generator + infra
# ---------------------------------------------------------------------------
set(VOCAB_CORE_SOURCES
    src/core/AppContext.cpp
    src/core/AudioPlayer.cpp
    src/core/Config.cpp
    src/core/Logger.cpp
    src/core/Paths.cpp
    src/core/Utils.cpp
    src/domain/Rules.cpp
    src/domain/Types.cpp
    src/data/DailySyncRepositories.cpp
    src/data/Database.cpp
    src/data/QuestionPaperRepositories.cpp
    src/data/SqliteDb.cpp
    src/data/StudyPlanRepository.cpp
    src/data/UserRepositories.cpp
    src/data/WordBookRepository.cpp
    src/data/WordMapper.cpp
    src/data/WordRepository.cpp
    src/data/WrongBookRepository.cpp
    src/service/AuthSettingService.cpp
    src/service/DailyListService.cpp
    src/service/QuestionService.cpp
    src/service/SyncService.cpp
    src/service/WordBookService.cpp
    src/service/WrongBookService.cpp
    src/generator/Distractors.cpp
    src/generator/QuestionStrategy.cpp
    src/importer/JsonlImporter.cpp)

add_library(vocab_core STATIC ${VOCAB_CORE_SOURCES})
target_include_directories(vocab_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vocab_core PUBLIC Qt6::Core Qt6::Gui sqlite3 Threads::Threads)
if(Qt6Multimedia_FOUND)
    target_link_libraries(vocab_core PUBLIC Qt6::Multimedia)
    target_compile_definitions(vocab_core PUBLIC VOCAB_HAS_MULTIMEDIA=1)
    message(STATUS "Qt6 Multimedia enabled - audio playback available")
else()
    message(STATUS "Qt6 Multimedia not found - falling back to winmm audio")
    if(WIN32)
        target_link_libraries(vocab_core PUBLIC winmm)
    endif()
endif()

# ---------------------------------------------------------------------------
# Application (UI)
# ---------------------------------------------------------------------------
set(VOCAB_UI_SOURCES
    src/ui/Page.h
    src/ui/Navigation.h
    src/ui/MainWindow.cpp
    src/ui/LoginDialog.cpp
    src/ui/Theme.cpp
    src/ui/UiHelpers.cpp
    src/ui/dialogs/WordDialog.cpp
    src/ui/dialogs/WordBookPicker.cpp
    src/ui/dialogs/ChangePasswordDialog.cpp
    src/ui/pages/HomePage.cpp
    src/ui/pages/WordBookPage.cpp
    src/ui/pages/WrongBookPage.cpp
    src/ui/pages/DailyListPage.cpp
    src/ui/pages/PracticePage.cpp
    src/ui/pages/QuizPage.cpp
    src/ui/pages/ResultPage.cpp
    src/ui/pages/DictionaryPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/SyncPage.cpp)

qt_add_executable(ShanCi src/main.cpp ${VOCAB_UI_SOURCES})
target_link_libraries(ShanCi PRIVATE vocab_core Qt6::Widgets)

# 编译为 GUI 子系统程序：双击启动时不弹出命令行黑框
set_target_properties(ShanCi PROPERTIES WIN32_EXECUTABLE ON)

# ---------------------------------------------------------------------------
# CLI tools (词表导入)
# ---------------------------------------------------------------------------
add_executable(vocab-import tools/ImportTool.cpp)
target_link_libraries(vocab-import PRIVATE vocab_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(VOCAB_BUILD_TESTS "Build unit tests" ON)
if(VOCAB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
