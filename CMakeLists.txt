cmake_minimum_required(VERSION 3.21)

project(oyama_hrt_linux_desktop
    VERSION 0.1.0
    DESCRIPTION "Oyama's HRT Tracker - Linux/BSD desktop"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PostgreSQL 14 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
include(GNUInstallDirs)

add_library(oyama_pk STATIC
    core/src/pk.c)
target_include_directories(oyama_pk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
if(NOT WIN32)
    target_link_libraries(oyama_pk PUBLIC m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama_pk PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(oyama_storage STATIC
    storage/src/app_config.cpp
    storage/src/backup.cpp
    storage/src/connection.cpp
    storage/src/database.cpp
    storage/src/dose_event_repository.cpp
    storage/src/error.cpp
    storage/src/json_record_repository.cpp
    storage/src/lab_result_repository.cpp
    storage/src/local_cluster.cpp
    storage/src/migration.cpp
    storage/src/preferences_repository.cpp
    storage/src/settings_repository.cpp
    storage/src/tombstone_repository.cpp
    storage/src/types.cpp
    storage/src/uuid.cpp)
target_include_directories(oyama_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/storage/include)
target_link_libraries(oyama_storage
    PUBLIC PostgreSQL::PostgreSQL
    PRIVATE nlohmann_json::nlohmann_json)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama_storage PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(oyama-hrt-cli cli/main.cpp)
target_link_libraries(oyama-hrt-cli PRIVATE oyama_storage)
target_compile_definitions(oyama-hrt-cli PRIVATE
    OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
    OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama-hrt-cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(OYAMA_BUILD_QT_SHELL "Build the Qt/QML desktop shell" ON)

if(OYAMA_BUILD_QT_SHELL)
    find_package(Qt6 6.4 QUIET COMPONENTS Core Qml Quick QuickControls2)
    if(Qt6_FOUND)
        qt_standard_project_setup()
        qt_policy(SET QTP0001 NEW)
        qt_policy(SET QTP0004 NEW)
        qt_add_executable(oyama-hrt-app
            app/src/main.cpp
            app/src/app_controller.cpp
            app/src/app_controller.h)
        qt_add_qml_module(oyama-hrt-app
            URI OyamaHrt
            VERSION 1.0
            QML_FILES
                app/qml/Main.qml
                app/qml/OverviewPage.qml
                app/qml/AddDosePage.qml
                app/qml/HistoryPage.qml
                app/qml/SettingsPage.qml)
        target_link_libraries(oyama-hrt-app PRIVATE
            oyama_storage
            oyama_pk
            nlohmann_json::nlohmann_json
            Qt6::Core
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2)
        target_compile_definitions(oyama-hrt-app PRIVATE
            OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
            OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(oyama-hrt-app PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        install(TARGETS oyama-hrt-app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    else()
        message(STATUS "Qt 6.4+ not found; skipping the Qt/QML shell")
    endif()
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(pk_smoke_test core/tests/pk_smoke_test.c)
    target_link_libraries(pk_smoke_test PRIVATE oyama_pk)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(pk_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME pk_smoke_test COMMAND pk_smoke_test)

    add_executable(storage_test storage/tests/storage_test.cpp)
    target_link_libraries(storage_test PRIVATE oyama_storage)
    target_compile_definitions(storage_test PRIVATE
        OYAMA_TEST_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(storage_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME storage_test COMMAND storage_test)

    add_executable(local_cluster_test storage/tests/local_cluster_test.cpp)
    target_link_libraries(local_cluster_test PRIVATE oyama_storage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(local_cluster_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME local_cluster_test COMMAND local_cluster_test)

    add_test(
        NAME storage_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_with_temp_postgres.sh
                $<TARGET_FILE:storage_test>)
    set_tests_properties(storage_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(backup_test storage/tests/backup_test.cpp)
    target_link_libraries(backup_test PRIVATE oyama_storage nlohmann_json::nlohmann_json)
    target_compile_definitions(backup_test PRIVATE
        OYAMA_TEST_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(backup_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME backup_test COMMAND backup_test)
    add_test(
        NAME backup_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_with_temp_postgres.sh
                $<TARGET_FILE:backup_test>)
    set_tests_properties(backup_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_test(NAME cli_version COMMAND oyama-hrt-cli version)
    set_tests_properties(cli_version PROPERTIES
        PASS_REGULAR_EXPRESSION "oyama-hrt-cli ${PROJECT_VERSION}")

    add_test(
        NAME cli_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/run_cli_integration.sh
                $<TARGET_FILE:oyama-hrt-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations)
    set_tests_properties(cli_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_test(
        NAME cli_managed_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/run_cli_managed_integration.sh
                $<TARGET_FILE:oyama-hrt-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations)
    set_tests_properties(cli_managed_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_test(
        NAME cli_backup_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/run_cli_backup_integration.sh
                $<TARGET_FILE:oyama-hrt-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations)
    set_tests_properties(cli_backup_integration PROPERTIES SKIP_RETURN_CODE 77)


    if(TARGET oyama-hrt-app)
        add_test(NAME qt_shell_smoke COMMAND oyama-hrt-app --self-test)
        set_tests_properties(qt_shell_smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")

        add_executable(app_controller_test
            app/tests/app_controller_test.cpp
            app/src/app_controller.cpp
            app/src/app_controller.h)
        target_include_directories(app_controller_test PRIVATE app/src)
        target_link_libraries(app_controller_test PRIVATE
            oyama_storage
            oyama_pk
            nlohmann_json::nlohmann_json
            Qt6::Core)
        target_compile_definitions(app_controller_test PRIVATE
            OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
            OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(app_controller_test PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME app_controller_test
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/app/tests/run_app_controller_test.sh
                    $<TARGET_FILE:app_controller_test>)
        set_tests_properties(app_controller_test PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

option(OYAMA_BUILD_COMPARE_TOOLS "Build the upstream comparison dumper" ON)

if(OYAMA_BUILD_COMPARE_TOOLS)
    add_executable(oyama_pk_dump tools/compare/dump.c)
    target_link_libraries(oyama_pk_dump PRIVATE oyama_pk)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(oyama_pk_dump PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

install(TARGETS oyama-hrt-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY storage/migrations/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/oyama-hrt/migrations
    FILES_MATCHING PATTERN "*.sql")
