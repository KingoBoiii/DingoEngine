#include "depch.h"

#ifdef DE_CRASH_REPORTING

#include "DingoEngine/Core/CrashReporter.h"
#include "DingoEngine/Core/CrashReportText.h"
#include "DingoEngine/Core/CrashHandler.h"
#include "DingoEngine/Core/LogLifecycle.h"
#include "DingoEngine/Core/PathText.h"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#include <sentry.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>

namespace Dingo::Internal::CrashReporter
{

	namespace
	{
		using namespace CrashReportText;

		static_assert(spdlog::level::trace == 0 && spdlog::level::debug == 1 && spdlog::level::info == 2
			&& spdlog::level::warn == 3 && spdlog::level::err == 4 && spdlog::level::critical == 5,
			"CrashReportText::LevelFromSpdlog assumes spdlog's level values");

		constexpr size_t k_MaxQueued = 1024;
		constexpr size_t k_MaxMessageBytes = 8 * 1024;
		constexpr uint64_t k_FlushTimeoutMs = 1500;
		constexpr uint64_t k_ShutdownTimeoutMs = 2000;
		constexpr uint64_t k_AttachmentShutdownTimeoutMs = 10000;
		constexpr auto k_FatalWait = std::chrono::milliseconds(2000);
		constexpr uintmax_t k_MaxLogReadBytes = 512 * 1024;
		constexpr uintmax_t k_MaxAttachedLogBytes = 4 * 1024 * 1024;
		constexpr uintmax_t k_MaxAttachedDumpBytes = 32 * 1024 * 1024;
		constexpr const char* k_ReportedListName = "Dingo-reported-crashes.txt";

		struct Entry
		{
			ReportLevel Level;
			std::string Logger;
			std::string Message;
		};

		Config s_Config;
		bool s_Configured = false;
		std::atomic<bool> s_Enabled = false;
		std::atomic<bool> s_Muted = false;
		HomeScrubber s_Scrubber;

		// Producers are the sink (already serialised by the loggers' sink lock) and FlushBeforeFatal,
		// which only try-locks. The worker holds it just to swap the queue out.
		std::timed_mutex s_QueueMutex;
		std::condition_variable_any s_QueueReady;
		std::deque<Entry> s_Queue;
		bool s_Stop = false;
		std::atomic<uint64_t> s_FlushRequested = 0;
		std::atomic<uint64_t> s_FlushDone = 0;

		// A program that ends through exit() never reaches Shutdown; a joinable std::thread's
		// destructor would then terminate it.
		struct WorkerThread
		{
			std::thread Thread;
			~WorkerThread()
			{
				if (Thread.joinable())
					Thread.detach();
			}
		};

		WorkerThread s_Worker;
		std::atomic<std::thread::id> s_WorkerId;
		std::shared_ptr<spdlog::sinks::sink> s_Sink;

		void Enqueue(ReportLevel level, std::string_view logger, std::string_view message)
		{
			if (s_Muted.load())
				return;
			std::unique_lock<std::timed_mutex> lock(s_QueueMutex);
			const bool event = IsEvent(level);
			if (s_Queue.size() >= k_MaxQueued + (event ? 64 : 0))
				return;
			const bool wasEmpty = s_Queue.empty();
			s_Queue.push_back({ level, std::string(logger), std::string(TruncateUtf8(message, k_MaxMessageBytes)) });
			lock.unlock();
			if (wasEmpty)
				s_QueueReady.notify_one();
		}

		class ReportSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
		{
		protected:
			void sink_it_(const spdlog::details::log_msg& message) override
			{
				Enqueue(LevelFromSpdlog(static_cast<int>(message.level)),
					std::string_view(message.logger_name.data(), message.logger_name.size()),
					std::string_view(message.payload.data(), message.payload.size()));
			}

			void flush_() override {}
		};

		sentry_level_t ToSentryLevel(ReportLevel level)
		{
			switch (level)
			{
				case ReportLevel::Debug: return SENTRY_LEVEL_DEBUG;
				case ReportLevel::Info: return SENTRY_LEVEL_INFO;
				case ReportLevel::Warning: return SENTRY_LEVEL_WARNING;
				case ReportLevel::Error: return SENTRY_LEVEL_ERROR;
				case ReportLevel::Fatal: return SENTRY_LEVEL_FATAL;
			}
			return SENTRY_LEVEL_INFO;
		}

		HomeScrubber MakeScrubber()
		{
#ifdef DE_PLATFORM_WINDOWS
			if (const wchar_t* home = _wgetenv(L"USERPROFILE"))
			{
				const std::filesystem::path path(home);
				return HomeScrubber(PathToUtf8(path), path.u16string(), true);
			}
#else
			if (const char* home = std::getenv("HOME"))
			{
				// libstdc++ throws converting a $HOME that isn't UTF-8.
				std::u16string utf16;
				try
				{
					utf16 = std::filesystem::path(home).u16string();
				}
				catch (...)
				{
				}
				return HomeScrubber(home, utf16, false);
			}
#endif
			return {};
		}

		// Message events carry no stack, so the module list is only the install path, which can hold
		// the player's user name; the user is sentry's installation id, which follows one player.
		sentry_value_t BeforeSend(sentry_value_t event, sentry_hint_t*, void*)
		{
			sentry_value_remove_by_key(event, "debug_meta");
			sentry_value_remove_by_key(event, "user");
			return event;
		}

		void IgnoreSentryLog(sentry_level_t, const char*, va_list, void*) {}

		void Send(const Entry& entry, EventLimiter& limiter)
		{
			const std::string message = s_Scrubber.Apply(entry.Message);
			if (IsEvent(entry.Level) && limiter.Allow(message))
				sentry_capture_event(sentry_value_new_message_event(ToSentryLevel(entry.Level), entry.Logger.c_str(), message.c_str()));

			sentry_value_t crumb = sentry_value_new_breadcrumb("default", message.c_str());
			sentry_value_set_by_key(crumb, "category", sentry_value_new_string(entry.Logger.c_str()));
			sentry_value_set_by_key(crumb, "level", sentry_value_new_string(LevelName(entry.Level)));
			sentry_add_breadcrumb(crumb);
		}

		std::string ReadTail(const std::filesystem::path& path, uintmax_t maxBytes)
		{
			std::error_code error;
			const uintmax_t size = std::filesystem::file_size(path, error);
			if (error)
				return {};
			std::ifstream file(path, std::ios::binary);
			if (!file)
				return {};
			const uintmax_t offset = size > maxBytes ? size - maxBytes : 0;
			file.seekg(static_cast<std::streamoff>(offset));
			std::string text(static_cast<size_t>(size - offset), '\0');
			file.read(text.data(), static_cast<std::streamsize>(text.size()));
			text.resize(static_cast<size_t>(file.gcount()));
			if (offset > 0)
			{
				const size_t newline = text.find('\n');
				text.erase(0, newline == std::string::npos ? text.size() : newline + 1);
			}
			return text;
		}

		std::vector<char> ReadBytes(const std::filesystem::path& path, uintmax_t maxBytes)
		{
			std::error_code error;
			const uintmax_t size = std::filesystem::file_size(path, error);
			if (error || size == 0 || size > maxBytes)
				return {};
			std::ifstream file(path, std::ios::binary);
			std::vector<char> bytes(static_cast<size_t>(size));
			if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
				return {};
			return bytes;
		}

		void SetString(sentry_value_t object, const char* key, const std::string& value)
		{
			sentry_value_set_by_key(object, key, sentry_value_new_string(value.c_str()));
		}

		void AddAttachment(sentry_scope_t* scope, const std::vector<char>& bytes, const std::string& name, const char* contentType, const char* type)
		{
			if (bytes.empty())
				return;
			sentry_value_t attachment = sentry_attachment_from_bytes(bytes.data(), bytes.size(), name.c_str());
			if (sentry_value_is_null(attachment))
				return;
			sentry_attachment_set_type(attachment, type);
			sentry_attachment_set_content_type(attachment, contentType);
			sentry_scope_add_attachment(scope, attachment);
		}

		void SendCrash(const CrashLog& crash, const std::filesystem::path& log, std::filesystem::file_time_type written)
		{
			sentry_value_t event = sentry_value_new_message_event(SENTRY_LEVEL_FATAL, "crash-handler", s_Scrubber.Apply(crash.Header).c_str());

			// MSVC's file_clock has no to_sys; the offset between the clocks is near enough for a timestamp.
			const auto system = std::chrono::system_clock::now()
				+ std::chrono::duration_cast<std::chrono::system_clock::duration>(written - std::filesystem::file_time_type::clock::now());
			const auto seconds = std::chrono::floor<std::chrono::seconds>(system);
			SetString(event, "timestamp", std::format("{:%FT%TZ}", seconds));

			sentry_value_t extra = sentry_value_new_object();
			SetString(extra, "crash_report", s_Scrubber.Apply(crash.Report));
			SetString(extra, "log_tail", s_Scrubber.Apply(crash.Tail));
			SetString(extra, "log_file", PathToUtf8(log.filename()));
			sentry_value_set_by_key(event, "extra", extra);

			sentry_value_t tags = sentry_value_new_object();
			SetString(tags, "crash.previous_run", "true");
			sentry_value_set_by_key(event, "tags", tags);

			if (!s_Config.SendAttachments)
			{
				sentry_capture_event(event);
				return;
			}

			sentry_scope_t* scope = sentry_local_scope_new();
			const std::string logText = s_Scrubber.Apply(ReadTail(log, k_MaxAttachedLogBytes));
			AddAttachment(scope, std::vector<char>(logText.begin(), logText.end()), PathToUtf8(log.filename()), "text/plain", SENTRY_ATTACHMENT_TYPE_GENERIC);
			if (!crash.MinidumpPath.empty())
			{
				const std::filesystem::path dump = PathFromUtf8(crash.MinidumpPath);
				std::vector<char> bytes = ReadBytes(dump, k_MaxAttachedDumpBytes);
				s_Scrubber.MaskBinary(bytes);
				AddAttachment(scope, bytes, PathToUtf8(dump.filename()), "application/x-dmp", SENTRY_ATTACHMENT_TYPE_MINIDUMP);
			}
			sentry_scope_capture_event(scope, event, nullptr);
		}

		bool IsRunLog(const std::filesystem::path& path)
		{
			const std::u8string name = path.filename().u8string();
			return name.ends_with(u8".log") && (name.starts_with(u8"Dingo.") || name.starts_with(u8"Dingo-"));
		}

		// Earlier runs' logs that end in a finished crash report and aren't on the reported list (keyed
		// by CrashKey). Only a run that had reports on is sent: a player who turns them on later never
		// uploads a crash from before. The list keeps only logs that still exist.
		void ReportPreviousCrashes()
		{
			const std::filesystem::path listPath = s_Config.LogDirectory / k_ReportedListName;
			std::set<std::string> reported;
			{
				std::ifstream list(listPath);
				for (std::string line; std::getline(list, line);)
				{
					if (!line.empty())
						reported.insert(line);
				}
			}

			std::set<std::string> keep;
			std::error_code error;
			std::filesystem::directory_iterator it(s_Config.LogDirectory, error);
			for (; !error && it != std::filesystem::directory_iterator(); it.increment(error))
			{
				const std::filesystem::directory_entry& entry = *it;
				std::error_code entryError;
				if (!entry.is_regular_file(entryError) || !IsRunLog(entry.path()) || entry.path().filename() == s_Config.LogFile.filename())
					continue;
				const std::filesystem::file_time_type written = entry.last_write_time(entryError);
				if (entryError)
					continue;
				const std::optional<CrashLog> crash = FindCrash(ReadTail(entry.path(), k_MaxLogReadBytes));
				// An unfinished report may belong to a process still writing its dump: look again next launch.
				if (!crash || !crash->Complete)
					continue;
				const std::string key = CrashKey(*crash);
				if (!reported.contains(key) && crash->ReportingOn)
					SendCrash(*crash, entry.path(), written);
				keep.insert(key);
			}

			if (keep == reported)
				return;
			std::ofstream list(listPath, std::ios::trunc);
			for (const std::string& key : keep)
				list << key << '\n';
		}

		// The worker runs under the crash handler, where an escaping exception would end the game.
		template<typename Fn>
		void Guarded(Fn&& fn)
		{
			try
			{
				fn();
			}
			catch (...)
			{
			}
		}

		void WorkerMain(void*)
		{
			s_WorkerId.store(std::this_thread::get_id());
			Guarded(ReportPreviousCrashes);

			EventLimiter limiter;
			for (;;)
			{
				std::deque<Entry> batch;
				bool stop = false;
				{
					std::unique_lock<std::timed_mutex> lock(s_QueueMutex);
					// Timed: FlushBeforeFatal only try-locks, so its notify can come unlocked and be missed.
					s_QueueReady.wait_for(lock, std::chrono::milliseconds(100), [] {
						return s_Stop || !s_Queue.empty() || s_FlushRequested.load() != s_FlushDone.load();
					});
					batch.swap(s_Queue);
					stop = s_Stop;
				}

				for (const Entry& entry : batch)
					Guarded([&] { Send(entry, limiter); });

				const uint64_t requested = s_FlushRequested.load();
				if (requested != s_FlushDone.load())
				{
					sentry_flush(k_FlushTimeoutMs);
					s_FlushDone.store(requested);
				}

				if (stop && batch.empty())
					break;
			}
			s_WorkerId.store(std::thread::id());
		}

		bool Start()
		{
			if (s_Enabled)
				return true;
			if (s_Config.Dsn.empty() || s_Config.LogDirectory.empty())
				return false;

			sentry_options_t* options = sentry_options_new();
			sentry_options_set_dsn(options, s_Config.Dsn.c_str());
			sentry_options_set_release(options, s_Config.Release.c_str());
			sentry_options_set_environment(options, s_Config.Environment.c_str());
#ifdef DE_PLATFORM_WINDOWS
			sentry_options_set_database_pathw(options, (s_Config.LogDirectory / "crash-reports").c_str());
#else
			sentry_options_set_database_path(options, (s_Config.LogDirectory / "crash-reports").c_str());
#endif
			sentry_options_set_auto_session_tracking(options, 0);
			sentry_options_set_debug(options, 0);
			sentry_options_set_logger(options, IgnoreSentryLog, nullptr);
			sentry_options_set_max_breadcrumbs(options, 100);
			sentry_options_set_before_send(options, BeforeSend, nullptr);
			// A minidump upload takes seconds; one cut off at exit is only retried on the next launch.
			sentry_options_set_shutdown_timeout(options, s_Config.SendAttachments ? k_AttachmentShutdownTimeoutMs : k_ShutdownTimeoutMs);
			// A report that can't go out (offline) stays in the database and is retried next launch.
			sentry_options_set_http_retry(options, 1);
			if (sentry_init(options) != 0)
				return false;

			{
				std::lock_guard<std::timed_mutex> lock(s_QueueMutex);
				s_Stop = false;
				s_Queue.clear();
			}
			s_Worker.Thread = std::thread([] { Internal::CrashHandler::RunThread(WorkerMain, nullptr); });
			s_Sink = std::make_shared<ReportSink>();
			Internal::AddLogSink(s_Sink);
			s_Enabled = true;
			return true;
		}

		void Stop()
		{
			if (!s_Enabled)
				return;
			Internal::RemoveLogSink(s_Sink);
			s_Sink.reset();
			{
				std::lock_guard<std::timed_mutex> lock(s_QueueMutex);
				s_Stop = true;
			}
			s_QueueReady.notify_one();
			if (s_Worker.Thread.joinable())
				s_Worker.Thread.join();
			s_Enabled = false;
			sentry_close();
		}
	}

	bool IsConfigured()
	{
		return s_Configured;
	}

	void Configure(const Config& config)
	{
		if (s_Configured)
			return;
		s_Config = config;
		s_Configured = true;
		s_Scrubber = MakeScrubber();
	}

	bool SetEnabled(bool enabled)
	{
		if (!enabled)
		{
			const bool wasEnabled = s_Enabled;
			Stop();
			if (wasEnabled)
				DE_CORE_INFO("Crash reporting: off");
			return true;
		}
		if (s_Enabled)
			return true;
		if (s_Config.Dsn.empty())
		{
			DE_CORE_WARN("Crash reporting: can't turn on without ApplicationParams::CrashReporting::Dsn");
			return false;
		}
		if (s_Config.LogDirectory.empty())
		{
			DE_CORE_WARN("Crash reporting: can't turn on without a log directory");
			return false;
		}
		if (!Start())
		{
			DE_CORE_WARN("Crash reporting: sentry failed to start (check the Dsn)");
			return false;
		}
		DE_CORE_INFO("Crash reporting: on ({}, {})", s_Config.Release, s_Config.Environment);
		return true;
	}

	bool IsEnabled()
	{
		return s_Enabled;
	}

	void MuteForCrash()
	{
		s_Muted.store(true);
	}

	void Shutdown()
	{
		Stop();
	}

	void FlushBeforeFatal()
	{
		if (!s_Enabled || s_WorkerId.load() == std::this_thread::get_id())
			return;

		const uint64_t target = s_FlushRequested.fetch_add(1) + 1;
		{
			std::unique_lock<std::timed_mutex> lock(s_QueueMutex, std::chrono::milliseconds(200));
		}
		s_QueueReady.notify_one();

		const auto deadline = std::chrono::steady_clock::now() + k_FatalWait;
		while (s_FlushDone.load() < target && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

}

#endif
