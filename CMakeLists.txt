cmake_minimum_required(VERSION 3.25)

project(Doomgazer
    VERSION 0.1.1
    DESCRIPTION "Downtuned guitar processor"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CPM.cmake)

option(DOOMGAZER_BUILD_PLUGIN "Build the JUCE 9 VST3, CLAP, and Standalone targets" ON)
option(DOOMGAZER_BUILD_CLAP "Build the CLAP target (requires the plugin)" ON)
option(DOOMGAZER_BUILD_TESTS "Build the headless DSP tests" ON)

add_library(doomgazer_dsp STATIC
    dsp/src/Processor.cpp
    dsp/src/FactoryPresets.cpp
    dsp/src/PresetIO.cpp
)
target_include_directories(doomgazer_dsp PUBLIC dsp/include)
target_compile_features(doomgazer_dsp PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(doomgazer_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(doomgazer_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DOOMGAZER_BUILD_TESTS)
    enable_testing()

    add_executable(doomgazer_test_cli tests/test_cli_main.cpp)
    target_link_libraries(doomgazer_test_cli PRIVATE doomgazer_dsp)
    if(MSVC)
        target_compile_options(doomgazer_test_cli PRIVATE /W4 /permissive-)
    else()
        target_compile_options(doomgazer_test_cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME cli_self_test COMMAND doomgazer_test_cli --self-test)
    add_test(NAME cli_audit COMMAND doomgazer_test_cli --audit)
    add_test(NAME cli_impulse COMMAND doomgazer_test_cli --signal impulse --blocks 32 --json)
    add_test(NAME cli_noise COMMAND doomgazer_test_cli --signal noise --blocks 48 --json)
    add_test(NAME cli_zero COMMAND doomgazer_test_cli --signal zero --blocks 16 --json)

    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        VERSION 3.16.1
        OPTIONS
            "CATCH_INSTALL_DOCS OFF"
            "CATCH_BUILD_TESTING OFF"
    )

    add_executable(doomgazer_unit_tests tests/test_dsp.cpp)
    target_link_libraries(doomgazer_unit_tests PRIVATE doomgazer_dsp Catch2::Catch2WithMain)
    if(MSVC)
        target_compile_options(doomgazer_unit_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(doomgazer_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME unit_tests COMMAND doomgazer_unit_tests)
endif()

if(DOOMGAZER_BUILD_PLUGIN AND MINGW)
    message(FATAL_ERROR
        "JUCE does not support MinGW. Use the MSVC desktop toolset for the plugin, "
        "or configure -DDOOMGAZER_BUILD_PLUGIN=OFF to build the DSP tests.")
endif()

if(DOOMGAZER_BUILD_PLUGIN)
    CPMAddPackage(
        NAME JUCE
        GITHUB_REPOSITORY juce-framework/JUCE
        GIT_TAG 9.0.5
        OPTIONS
            "JUCE_BUILD_EXAMPLES OFF"
            "JUCE_BUILD_EXTRAS OFF"
    )

    juce_add_plugin(Doomgazer
        COMPANY_NAME "Distant Nebula"
        BUNDLE_ID "com.doomgazer.plugin"
        ICON_BIG plugin/assets/doomgazer_icon_512.png
        ICON_SMALL plugin/assets/doomgazer_icon_32.png
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Dmzr
        PLUGIN_CODE Dmgz
        FORMATS VST3 Standalone
        PRODUCT_NAME "Doomgazer"
        VST3_CATEGORIES Fx Distortion
    )

    target_sources(Doomgazer PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/PresetLibrary.cpp
        plugin/UiKit.cpp
        plugin/ReleaseCheck.cpp
    )

    # GitHub release check. Owner and repo are the public repository. The version
    # string tracks project() VERSION, which juce_add_plugin publishes as well.
    set(DOOMGAZER_RELEASE_DEFS
        "DOOMGAZER_GITHUB_OWNER=\"distantdev\""
        "DOOMGAZER_GITHUB_REPO=\"doomgazer\""
        "DOOMGAZER_VERSION_STRING=\"${PROJECT_VERSION}\""
    )

    target_compile_definitions(Doomgazer PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_ASIO=1
        ${DOOMGAZER_RELEASE_DEFS}
    )

    target_link_libraries(Doomgazer PRIVATE
        doomgazer_dsp
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    if(DOOMGAZER_BUILD_TESTS)
        juce_add_console_app(DoomgazerPluginTests PRODUCT_NAME "DoomgazerPluginTests")
        target_sources(DoomgazerPluginTests PRIVATE
            tests/test_plugin.cpp
            plugin/PluginProcessor.cpp
            plugin/PluginEditor.cpp
            plugin/PresetLibrary.cpp
            plugin/UiKit.cpp
            plugin/ReleaseCheck.cpp
        )
        target_include_directories(DoomgazerPluginTests PRIVATE plugin)
        target_compile_definitions(DoomgazerPluginTests PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_MODAL_LOOPS_PERMITTED=1
            ${DOOMGAZER_RELEASE_DEFS}
        )
        target_link_libraries(DoomgazerPluginTests PRIVATE
            doomgazer_dsp
            juce::juce_audio_utils
            juce::juce_dsp
            Catch2::Catch2WithMain
        )
        add_test(NAME plugin_tests COMMAND DoomgazerPluginTests)
    endif()

    if(DOOMGAZER_BUILD_CLAP)
        CPMAddPackage(
            NAME clap-juce-extensions
            GITHUB_REPOSITORY free-audio/clap-juce-extensions
            GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8
        )
        if(clap-juce-extensions_ADDED)
            clap_juce_extensions_plugin(
                TARGET Doomgazer
                CLAP_ID "com.doomgazer.plugin"
                CLAP_FEATURES audio-effect distortion
            )
        endif()
    endif()
endif()
