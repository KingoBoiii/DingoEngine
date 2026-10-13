#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Dingo
{

	/// Where a profile zone is in the source. DE_PROFILE_SCOPE makes one static per zone, so its
	/// address identifies the zone to the profiler and must stay valid for the whole run.
	struct ProfileSite
	{
		const char* Name;		///< Zone name in the profiler; nullptr shows Function instead.
		const char* Function;	///< Name of the enclosing function.
		const char* File;		///< Source file path.
		uint32_t Line;			///< Source line.
		uint32_t Color;			///< 0xRRGGBB zone colour; 0 is the profiler's default.
	};

	/// One zone, from construction to destruction. Made by the DE_PROFILE_* macros, which compile
	/// to nothing unless the engine is built with premake's --profile option.
	class ProfileZone
	{
	public:
		/// Starts the zone; site must outlive the run (the macros make it static).
		explicit ProfileZone(const ProfileSite* site);
		/// Ends the zone.
		~ProfileZone();

		/// Not copyable.
		ProfileZone(const ProfileZone&) = delete;
		/// Not copyable.
		ProfileZone& operator=(const ProfileZone&) = delete;

		/// Shown beside the zone in the profiler, e.g. which layer an "OnUpdate" zone ran.
		void Text(std::string_view text);

	private:
		uint32_t m_Id = 0;
		int m_Active = 0;
	};

	/// Free functions over the profiler (Tracy); they do nothing unless the engine was built with
	/// --profile.
	namespace Profiler
	{
		/// True when the engine was built with --profile (Tracy is compiled in).
		bool IsCompiledIn();
		/// True while the Tracy viewer is connected; nothing is recorded otherwise.
		bool IsConnected();

		/// Marks the end of a frame in the profiler; Application::Run calls it once per loop pass.
		void MarkFrame();
		/// Adds a sample to the named plot. `name` identifies the plot by its address, so pass a
		/// string literal or one that lives as long as the run.
		void Plot(const char* name, double value);
		/// Names the calling thread in the profiler; `name` must outlive the run.
		void SetThreadName(const char* name);
	}

}

#ifdef DE_PROFILE
	/// Implementation detail of the DE_PROFILE_* macros.
	#define DE_PROFILE_CONCAT_INNER(a, b) a##b
	/// Implementation detail of the DE_PROFILE_* macros.
	#define DE_PROFILE_CONCAT(a, b) DE_PROFILE_CONCAT_INNER(a, b)
	/// Implementation detail of the DE_PROFILE_* macros.
	#define DE_PROFILE_SITE_NAME DE_PROFILE_CONCAT(de_profile_site_, __LINE__)
	/// Implementation detail of the DE_PROFILE_* macros.
	#define DE_PROFILE_ZONE_NAME DE_PROFILE_CONCAT(de_profile_zone_, __LINE__)

	/// Times the rest of the enclosing block as a profiler zone called `name` (a string literal).
	/// Declares variables named after the line, so use it as a statement in a block, once per line.
	#define DE_PROFILE_SCOPE(name) \
		static constexpr ::Dingo::ProfileSite DE_PROFILE_SITE_NAME{ name, __FUNCTION__, __FILE__, static_cast<uint32_t>(__LINE__), 0 }; \
		::Dingo::ProfileZone DE_PROFILE_ZONE_NAME(&DE_PROFILE_SITE_NAME)
	/// Like DE_PROFILE_SCOPE, and shows `text` beside the zone (see ProfileZone::Text).
	#define DE_PROFILE_SCOPE_TEXT(name, text) \
		DE_PROFILE_SCOPE(name); \
		DE_PROFILE_ZONE_NAME.Text(text)
	/// Times the rest of the enclosing block as a zone named after the function.
	#define DE_PROFILE_FUNCTION() DE_PROFILE_SCOPE(nullptr)
	/// Marks the end of a frame; see Profiler::MarkFrame.
	#define DE_PROFILE_FRAME() ::Dingo::Profiler::MarkFrame()
	/// Adds a sample to the plot called `name` (a string literal); see Profiler::Plot. Without
	/// --profile the macro is empty and `value` is not evaluated.
	#define DE_PROFILE_PLOT(name, value) ::Dingo::Profiler::Plot(name, static_cast<double>(value))
	/// Names the calling thread in the profiler; see Profiler::SetThreadName.
	#define DE_PROFILE_THREAD(name) ::Dingo::Profiler::SetThreadName(name)
#else
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_SCOPE(name)
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_SCOPE_TEXT(name, text)
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_FUNCTION()
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_FRAME()
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_PLOT(name, value)
	/// Compiles to nothing unless built with premake's --profile option.
	#define DE_PROFILE_THREAD(name)
#endif
