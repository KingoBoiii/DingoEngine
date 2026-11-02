-- sentry-native (vendor/sentry-native, the KingoBoiii fork pinned at upstream 0.17.4) as a premake
-- project, mirroring its CMake build with SENTRY_BACKEND=none: events, breadcrumbs and attachments
-- only, no Crashpad/Breakpad/inproc handler, so the engine's own crash handler stays the only one.
-- Transport: WinHTTP on Windows, the system libcurl on Linux. No unwinder (nothing captures stack
-- traces), no screenshots, no WER integration. Kept here, not in the submodule, so vendor/ stays
-- untouched; included from premake5.lua only with --crash-reporting.
local root = path.join(_MAIN_SCRIPT_DIR, "vendor/sentry-native")

project "sentry"
    kind "StaticLib"
    language "C"
    cdialect "C11"
    staticruntime "off"
    warnings "Off"

    location(path.join(_MAIN_SCRIPT_DIR, "build/projects/sentry"))
    targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/build/bin-int/" .. outputdir .. "/%{prj.name}")

    local src = root .. "/src/"

    files {
        root .. "/include/sentry.h",
        src .. "sentry_alloc.c",
        src .. "sentry_app_hang_latch.c",
        src .. "sentry_app_hang_monitor.c",
        src .. "sentry_attachment.c",
        src .. "sentry_backend.c",
        src .. "sentry_batcher.c",
        src .. "sentry_client_report.c",
        src .. "sentry_core.c",
        src .. "sentry_database.c",
        src .. "sentry_envelope.c",
        src .. "sentry_hint.c",
        src .. "sentry_info.c",
        src .. "sentry_json.c",
        src .. "sentry_logger.c",
        src .. "sentry_logs.c",
        src .. "sentry_metrics.c",
        src .. "sentry_mpack.c",
        src .. "sentry_options.c",
        src .. "sentry_os.c",
        src .. "sentry_random.c",
        src .. "sentry_ratelimiter.c",
        src .. "sentry_retry.c",
        src .. "sentry_ringbuffer.c",
        src .. "sentry_scope.c",
        src .. "sentry_session.c",
        src .. "sentry_slice.c",
        src .. "sentry_string.c",
        src .. "sentry_sync.c",
        src .. "sentry_telemetry.c",
        src .. "sentry_transport.c",
        src .. "sentry_utils.c",
        src .. "sentry_uuid.c",
        src .. "sentry_value.c",
        src .. "sentry_writer.c",
        src .. "sentry_tracing.c",
        src .. "path/sentry_path.c",
        src .. "screenshot/sentry_screenshot.c",
        src .. "screenshot/sentry_screenshot_none.c",
        src .. "session_replay/sentry_session_replay.c",
        src .. "session_replay/sentry_session_replay_none.c",
        src .. "transports/sentry_function_transport.c",
        src .. "transports/sentry_http_transport.c",
        src .. "unwinder/sentry_unwinder.c",
        src .. "backends/sentry_backend_none.c",
    }

    includedirs {
        root .. "/include",
        root .. "/src",
    }

    defines {
        "SENTRY_BUILD_STATIC",
        "SENTRY_SCREENSHOT_NONE",
        "SENTRY_HANDLER_STACK_SIZE=64",
        "SENTRY_BATCHER_BUFFER_COUNT=3",
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        files {
            src .. "sentry_windows_dbghelp.c",
            src .. "path/sentry_path_windows.c",
            src .. "process/sentry_process_windows.c",
            src .. "symbolizer/sentry_symbolizer_windows.c",
            src .. "modulefinder/sentry_modulefinder_windows.c",
            src .. "transports/sentry_http_transport_winhttp.c",
            src .. "sentry_thread_stackwalk_windows.c",
            src .. "unwinder/sentry_unwinder_dbghelp.c",
        }
        -- SENTRY_THREAD_STACK_GUARANTEE_AUTO_INIT stays undefined: the engine's PrepareThread owns
        -- each thread's stack guarantee.
        defines {
            "SENTRY_WITH_UNWINDER_DBGHELP",
            "SENTRY_THREAD_STACK_GUARANTEE_FACTOR=10",
            "SIZEOF_LONG=4",
            "_WIN32_WINNT=0x0A00",
        }

    filter "system:linux"
        pic "On"
        files {
            src .. "sentry_unix_pageallocator.c",
            src .. "symbolizer/sentry_symbolizer_unix.c",
            src .. "path/sentry_path_unix.c",
            src .. "process/sentry_process_unix.c",
            src .. "modulefinder/sentry_modulefinder_linux.c",
            src .. "transports/sentry_http_transport_curl.c",
            src .. "sentry_thread_stackwalk_posix.c",
            root .. "/vendor/stb_sprintf.c",
        }
        defines { "SENTRY_LINK_CURL", "SIZEOF_LONG=8", "_GNU_SOURCE" }

    filter "configurations:Debug or configurations:Debug-ASan"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release or configurations:Distribution"
        runtime "Release"
        optimize "On"

    filter {}
