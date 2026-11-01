cmake_minimum_required(VERSION 3.21)

project(oyama_hrt_linux_desktop
    VERSION 0.5.0
    DESCRIPTION "Oyama's HRT Tracker - Linux/BSD desktop"
    LANGUAGES C CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    message(STATUS "OpenBSD native build: Qt 6 and libpq come from packages")
    # OpenBSD's base clang does not search /usr/local/include by default.
    # pkg_add installs Qt, libsodium, and nlohmann/json headers there.
    include_directories(SYSTEM /usr/local/include)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PostgreSQL 14 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
include(GNUInstallDirs)

if(OYAMA_SECURITY_MODE)
    add_compile_definitions(OYAMA_SECURITY_MODE=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
        add_link_options(-Wl,-z,relro -Wl,-z,now)
    endif()
else()
    add_compile_definitions(OYAMA_SECURITY_MODE=0 OYAMA_INSECURE_BUILD=1)
endif()

add_library(oyama_vault STATIC
    vault/src/vault.c
    vault/src/vault_crypto.c)
target_include_directories(oyama_vault
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(oyama_vault
    PUBLIC PkgConfig::SODIUM PostgreSQL::PostgreSQL)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama_vault PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
    storage/src/backup_crypto.cpp
    storage/src/connection.cpp
    storage/src/database.cpp
    storage/src/error.cpp
    storage/src/legacy_migration.cpp
    storage/src/legacy_promotion.cpp
    storage/src/local_cluster.cpp
    storage/src/migration.cpp
    storage/src/passphrase.cpp
    storage/src/passphrase_wordlist.cpp
    storage/src/types.cpp
    storage/src/uuid.cpp
    storage/src/vault_codec.cpp
    storage/src/vault_store.cpp
    storage/src/vault_dose_event_repository.cpp
    storage/src/vault_json_record_repository.cpp
    storage/src/vault_lab_result_repository.cpp
    storage/src/vault_preferences_repository.cpp
    storage/src/vault_settings_repository.cpp
    storage/src/vault_tombstone_repository.cpp)
target_include_directories(oyama_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/storage/include)
target_link_libraries(oyama_storage
    PUBLIC PostgreSQL::PostgreSQL
    PRIVATE nlohmann_json::nlohmann_json oyama_vault)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama_storage PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(oyama-hrt-cli cli/main.cpp cli/process_hardening.cpp)
target_link_libraries(oyama-hrt-cli PRIVATE oyama_storage PkgConfig::SODIUM)
target_compile_definitions(oyama-hrt-cli PRIVATE
    OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
    OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama-hrt-cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(OYAMA_BUILD_QT_SHELL "Build the Qt/QML desktop shell" ON)
option(OYAMA_SECURITY_MODE "Enable security hardening (default ON)" ON)

if(OYAMA_BUILD_QT_SHELL)
    find_package(Qt6 6.4 QUIET COMPONENTS Core Qml Quick QuickControls2)
    if(Qt6_FOUND)
        qt_standard_project_setup()
        # These policies were introduced after the minimum supported Qt.
        # Setting an unknown policy is fatal, so keep Qt 6.4 (Debian 12 and
        # Ubuntu 24.04) working as documented.
        if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
            qt_policy(SET QTP0001 NEW)
        endif()
        if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
            qt_policy(SET QTP0004 NEW)
        endif()

        function(oyama_add_app_assets target)
            qt_add_resources(${target} "app_i18n"
                PREFIX "/i18n"
                BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n"
                FILES
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/en.json"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/zh.json"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/ru.json"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/fa.json")
            qt_add_resources(${target} "app_fonts"
                PREFIX "/fonts"
                BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts"
                FILES
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/InterVariable.ttf"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-Regular.ttf"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-Medium.ttf"
                  "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-SemiBold.ttf")
        endfunction()

        qt_add_executable(oyama-hrt-app
            app/src/main.cpp
            app/src/app_controller.cpp
            app/src/app_controller.h
            app/src/legacy_migration_worker.cpp
            app/src/legacy_migration_worker.h
            app/src/ui_preferences.cpp
            app/src/ui_preferences.h)
        set(OYAMA_QML_FILES
            app/qml/Main.qml
            app/qml/VaultGate.qml
            app/qml/SetupVaultPage.qml
            app/qml/UnlockVaultPage.qml
            app/qml/Sidebar.qml
            app/qml/NavItem.qml
            app/qml/FirstRun.qml
            app/qml/OverviewPage.qml
            app/qml/HistoryPage.qml
            app/qml/SettingsPage.qml
            app/qml/SubPage.qml
            app/qml/ModeSettings.qml
            app/qml/WeightSettings.qml
            app/qml/AgeSettings.qml
            app/qml/CalibrationSettings.qml
            app/qml/ParamsSettings.qml
            app/qml/LanguageSettings.qml
            app/qml/AppearanceSettings.qml
            app/qml/DisclaimerPage.qml
            app/qml/DoseForm.qml
            app/qml/DateTimePicker.qml
            app/qml/HormoneChart.qml
            app/qml/DoseHeatmap.qml
            app/qml/LevelRail.qml
            app/qml/Reading.qml
            app/qml/T.qml
            app/qml/Theme.qml
            app/qml/UiText.qml
            app/qml/Icon.qml
            app/qml/Doodle.qml
            app/qml/Tick.qml
            app/qml/AppButton.qml
            app/qml/AppTextField.qml
            app/qml/AppScrollBar.qml
            app/qml/LineField.qml
            app/qml/FigureField.qml
            app/qml/InkRule.qml
            app/qml/ChoiceField.qml
            app/qml/ChoiceRow.qml
            app/qml/SettingsRow.qml
            app/qml/GroupHeading.qml
            app/qml/Hairline.qml
            app/qml/Fold.qml
            app/qml/InlinePrompt.qml
            app/qml/PageHeader.qml
            app/qml/PageColumn.qml
            app/qml/Segmented.qml
            app/qml/Tabs.qml
            app/qml/Drawings.js
            app/qml/Doses.js)
        set_source_files_properties(app/qml/T.qml
            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
        set_source_files_properties(app/qml/Theme.qml
            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
        qt_add_qml_module(oyama-hrt-app
            URI OyamaHrt
            VERSION 1.0
            RESOURCE_PREFIX "/qt/qml"
            QML_FILES ${OYAMA_QML_FILES}
            RESOURCES app/assets/app-icon.png)
        oyama_add_app_assets(oyama-hrt-app)
        target_link_libraries(oyama-hrt-app PRIVATE
            oyama_storage
            oyama_pk
            nlohmann_json::nlohmann_json
            PkgConfig::SODIUM
            Qt6::Core
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2)
        target_compile_definitions(oyama-hrt-app PRIVATE
            OYAMA_APP_VERSION="${PROJECT_VERSION}"
            OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
            OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(oyama-hrt-app PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        install(TARGETS oyama-hrt-app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES app/assets/oyama-hrt.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES app/assets/oyama-hrt.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
        install(FILES app/assets/app-icon.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME oyama-hrt.png)

        qt_add_executable(ui_preferences_test
            app/tests/ui_preferences_test.cpp
            app/src/ui_preferences.cpp
            app/src/ui_preferences.h)
        target_include_directories(ui_preferences_test PRIVATE app/src)
        target_link_libraries(ui_preferences_test PRIVATE Qt6::Core Qt6::Gui)
        oyama_add_app_assets(ui_preferences_test)
        add_test(NAME ui_preferences_test COMMAND ui_preferences_test)
        set_tests_properties(ui_preferences_test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        if(TARGET Qt6::qmake)
            get_target_property(_oyama_qmake Qt6::qmake IMPORTED_LOCATION)
            if(_oyama_qmake)
                get_filename_component(_oyama_qt_bin_dir "${_oyama_qmake}" DIRECTORY)
            endif()
        endif()
        find_program(OYAMA_QMLTESTRUNNER NAMES qmltestrunner
            HINTS "${_oyama_qt_bin_dir}")
        if(OYAMA_QMLTESTRUNNER)
            add_test(NAME qml_components
                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/app/tests/run_qml_tests.sh
                        ${CMAKE_CURRENT_BINARY_DIR}
                        ${OYAMA_QMLTESTRUNNER})
            set_tests_properties(qml_components PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endif()
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

    add_executable(process_hardening_test
        cli/process_hardening_test.cpp
        cli/process_hardening.cpp)
    target_include_directories(process_hardening_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/cli)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(process_hardening_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME process_hardening_test COMMAND process_hardening_test)

    add_executable(vault_crypto_test vault/tests/vault_crypto_test.c)
    target_include_directories(vault_crypto_test PRIVATE vault/src)
    target_link_libraries(vault_crypto_test PRIVATE oyama_vault)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_crypto_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME vault_crypto_test COMMAND vault_crypto_test)

    add_executable(vault_meta_test vault/tests/vault_meta_test.c)
    target_include_directories(vault_meta_test PRIVATE vault/src)
    target_link_libraries(vault_meta_test PRIVATE oyama_vault PostgreSQL::PostgreSQL)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_meta_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME vault_meta_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/vault/tests/run_vault_pg_test.sh
                $<TARGET_FILE:vault_meta_test>)
    set_tests_properties(vault_meta_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(vault_records_test vault/tests/vault_records_test.c)
    target_include_directories(vault_records_test PRIVATE vault/src)
    target_link_libraries(vault_records_test PRIVATE oyama_vault PostgreSQL::PostgreSQL)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_records_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME vault_records_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/vault/tests/run_vault_pg_test.sh
                $<TARGET_FILE:vault_records_test>)
    set_tests_properties(vault_records_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(vault_store_test storage/tests/vault_store_test.cpp)
    target_include_directories(vault_store_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(vault_store_test PRIVATE oyama_storage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_store_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME vault_store_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_vault_store_test.sh
                $<TARGET_FILE:vault_store_test>)
    set_tests_properties(vault_store_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(vault_codec_test storage/tests/vault_codec_test.cpp)
    target_link_libraries(vault_codec_test PRIVATE
        oyama_storage
        nlohmann_json::nlohmann_json)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_codec_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME vault_codec_test COMMAND vault_codec_test)

    add_executable(vault_repository_test storage/tests/vault_repository_test.cpp)
    target_include_directories(vault_repository_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(vault_repository_test PRIVATE oyama_storage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vault_repository_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME vault_repository_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_vault_repository_test.sh
                $<TARGET_FILE:vault_repository_test>)
    set_tests_properties(vault_repository_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(legacy_migration_test storage/tests/legacy_migration_test.cpp)
    target_include_directories(legacy_migration_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(legacy_migration_test PRIVATE
        oyama_storage
        nlohmann_json::nlohmann_json)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(legacy_migration_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME legacy_migration_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_legacy_migration_test.sh
                $<TARGET_FILE:legacy_migration_test>)
    set_tests_properties(legacy_migration_integration PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(legacy_promotion_test storage/tests/legacy_promotion_test.cpp)
    target_include_directories(legacy_promotion_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(legacy_promotion_test PRIVATE oyama_storage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(legacy_promotion_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(
        NAME legacy_promotion_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/storage/tests/run_legacy_promotion_test.sh
                $<TARGET_FILE:legacy_promotion_test>)
    set_tests_properties(legacy_promotion_integration PROPERTIES SKIP_RETURN_CODE 77)

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

    add_executable(backup_crypto_test storage/tests/backup_crypto_test.cpp)
    target_link_libraries(backup_crypto_test PRIVATE oyama_storage)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(backup_crypto_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME backup_crypto_test COMMAND backup_crypto_test)

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
        NAME cli_vault_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/run_cli_vault_integration.sh
                $<TARGET_FILE:oyama-hrt-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations)
    set_tests_properties(cli_vault_integration PROPERTIES SKIP_RETURN_CODE 77)

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

    add_test(
        NAME cli_migration_integration
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/tests/run_cli_migration.sh
                $<TARGET_FILE:oyama-hrt-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations)
    set_tests_properties(cli_migration_integration PROPERTIES SKIP_RETURN_CODE 77)


    if(TARGET oyama-hrt-app)
        add_test(NAME qt_shell_smoke COMMAND oyama-hrt-app --self-test)
        set_tests_properties(qt_shell_smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")

        foreach(locale en zh ru fa)
            add_test(NAME qt_shell_smoke_${locale}
                COMMAND oyama-hrt-app --self-test)
            set_tests_properties(qt_shell_smoke_${locale} PROPERTIES
                ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;OYAMA_UI_LANGUAGE=${locale};OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")
        endforeach()

        file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/qt-first-run")
        add_test(NAME qt_shell_smoke_first_run
            COMMAND oyama-hrt-app --self-test)
        set_tests_properties(qt_shell_smoke_first_run PROPERTIES
            ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/qt-first-run;OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")

        add_executable(app_controller_test
            app/tests/app_controller_test.cpp
            app/src/app_controller.cpp
            app/src/app_controller.h
            app/src/legacy_migration_worker.cpp
            app/src/legacy_migration_worker.h)
        target_include_directories(app_controller_test PRIVATE app/src)
        target_link_libraries(app_controller_test PRIVATE
            oyama_storage
            oyama_pk
            nlohmann_json::nlohmann_json
            PkgConfig::SODIUM
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

        add_executable(legacy_migration_worker_test
            app/tests/legacy_migration_worker_test.cpp
            app/src/legacy_migration_worker.cpp
            app/src/legacy_migration_worker.h)
        target_include_directories(legacy_migration_worker_test PRIVATE app/src)
        target_link_libraries(legacy_migration_worker_test PRIVATE
            oyama_storage
            nlohmann_json::nlohmann_json
            PkgConfig::SODIUM
            Qt6::Core)
        target_compile_definitions(legacy_migration_worker_test PRIVATE
            OYAMA_DEFAULT_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/storage/migrations"
            OYAMA_INSTALL_MIGRATIONS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/oyama-hrt/migrations")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(legacy_migration_worker_test PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME legacy_migration_worker_test
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/app/tests/run_legacy_migration_worker_test.sh
                    $<TARGET_FILE:legacy_migration_worker_test>)
        set_tests_properties(legacy_migration_worker_test PROPERTIES SKIP_RETURN_CODE 77)
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

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/man/oyama-hrt-app.1
    ${CMAKE_CURRENT_SOURCE_DIR}/man/oyama-hrt-cli.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
