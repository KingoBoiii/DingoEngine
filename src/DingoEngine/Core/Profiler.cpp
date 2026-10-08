#include "depch.h"
#include "DingoEngine/Core/Profiler.h"

#ifdef TRACY_ENABLE
#include <tracy/TracyC.h>

static_assert(sizeof(Dingo::ProfileSite) == sizeof(___tracy_source_location_data) &&
	offsetof(Dingo::ProfileSite, Name) == offsetof(___tracy_source_location_data, name) &&
	offsetof(Dingo::ProfileSite, Function) == offsetof(___tracy_source_location_data, function) &&
	offsetof(Dingo::ProfileSite, File) == offsetof(___tracy_source_location_data, file) &&
	offsetof(Dingo::ProfileSite, Line) == offsetof(___tracy_source_location_data, line) &&
	offsetof(Dingo::ProfileSite, Color) == offsetof(___tracy_source_location_data, color),
	"ProfileSite is handed to Tracy as its ___tracy_source_location_data");
#endif

namespace Dingo
{

#ifdef TRACY_ENABLE
	namespace
	{
		TracyCZoneCtx MakeContext(uint32_t id, int active)
		{
			TracyCZoneCtx context;
			context.id = id;
			context.active = active;
			return context;
		}
	}

	ProfileZone::ProfileZone(const ProfileSite* site)
	{
		const TracyCZoneCtx context = ___tracy_emit_zone_begin(reinterpret_cast<const ___tracy_source_location_data*>(site), 1);
		m_Id = context.id;
		m_Active = context.active;
	}

	ProfileZone::~ProfileZone()
	{
		___tracy_emit_zone_end(MakeContext(m_Id, m_Active));
	}

	void ProfileZone::Text(std::string_view text)
	{
		if (m_Active)
			___tracy_emit_zone_text(MakeContext(m_Id, m_Active), text.data(), text.size());
	}

	namespace Profiler
	{
		bool IsCompiledIn() { return true; }
		bool IsConnected() { return ___tracy_connected() != 0; }
		void MarkFrame() { ___tracy_emit_frame_mark(nullptr); }
		void Plot(const char* name, double value) { ___tracy_emit_plot(name, value); }
		void SetThreadName(const char* name) { ___tracy_set_thread_name(name); }
	}
#else
	ProfileZone::ProfileZone(const ProfileSite*) {}
	ProfileZone::~ProfileZone() {}
	void ProfileZone::Text(std::string_view) {}

	namespace Profiler
	{
		bool IsCompiledIn() { return false; }
		bool IsConnected() { return false; }
		void MarkFrame() {}
		void Plot(const char*, double) {}
		void SetThreadName(const char*) {}
	}
#endif

}
