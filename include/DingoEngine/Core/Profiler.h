#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Dingo
{

	// Where a profile zone is in the source. DE_PROFILE_SCOPE makes one static per zone, so its
	// address identifies the zone to the profiler and must stay valid for the whole run.
	struct ProfileSite
	{
		const char* Name;
		const char* Function;
		const char* File;
		uint32_t Line;
		uint32_t Color;
	};

	// One zone, from construction to destruction. Made by the DE_PROFILE_* macros, which compile to
	// nothing unless the engine is built with premake's --profile option.
	class ProfileZone
	{
	public:
		explicit ProfileZone(const ProfileSite* site);
		~ProfileZone();

		ProfileZone(const ProfileZone&) = delete;
		ProfileZone& operator=(const ProfileZone&) = delete;

		// Shown beside the zone in the profiler, e.g. which layer an "OnUpdate" zone ran.
		void Text(std::string_view text);

	private:
		uint32_t m_Id = 0;
		int m_Active = 0;
	};

	namespace Profiler
	{
		// True when the engine was built with --profile (Tracy is compiled in).
		bool IsCompiledIn();
		// True while the Tracy viewer is connected; nothing is recorded otherwise.
		bool IsConnected();

		void MarkFrame();
		// `name` identifies the plot by its address, so pass a string literal or one that lives as
		// long as the run.
		void Plot(const char* name, double value);
		void SetThreadName(const char* name);
	}

}

#ifdef DE_PROFILE
	#define DE_PROFILE_CONCAT_INNER(a, b) a##b
	#define DE_PROFILE_CONCAT(a, b) DE_PROFILE_CONCAT_INNER(a, b)
	#define DE_PROFILE_SITE_NAME DE_PROFILE_CONCAT(de_profile_site_, __LINE__)
	#define DE_PROFILE_ZONE_NAME DE_PROFILE_CONCAT(de_profile_zone_, __LINE__)

	#define DE_PROFILE_SCOPE(name) \
		static constexpr ::Dingo::ProfileSite DE_PROFILE_SITE_NAME{ name, __FUNCTION__, __FILE__, static_cast<uint32_t>(__LINE__), 0 }; \
		::Dingo::ProfileZone DE_PROFILE_ZONE_NAME(&DE_PROFILE_SITE_NAME)
	#define DE_PROFILE_SCOPE_TEXT(name, text) \
		DE_PROFILE_SCOPE(name); \
		DE_PROFILE_ZONE_NAME.Text(text)
	#define DE_PROFILE_FUNCTION() DE_PROFILE_SCOPE(nullptr)
	#define DE_PROFILE_FRAME() ::Dingo::Profiler::MarkFrame()
	#define DE_PROFILE_PLOT(name, value) ::Dingo::Profiler::Plot(name, static_cast<double>(value))
	#define DE_PROFILE_THREAD(name) ::Dingo::Profiler::SetThreadName(name)
#else
	#define DE_PROFILE_SCOPE(name)
	#define DE_PROFILE_SCOPE_TEXT(name, text)
	#define DE_PROFILE_FUNCTION()
	#define DE_PROFILE_FRAME()
	#define DE_PROFILE_PLOT(name, value)
	#define DE_PROFILE_THREAD(name)
#endif
