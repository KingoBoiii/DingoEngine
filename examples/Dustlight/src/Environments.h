#pragma once

#include <DingoEngine.h>

#include <algorithm>

namespace Dingo
{

	struct EnvironmentPreset
	{
		const char* Name;
		const char* Path;
		float SkySunAzimuth;
		float SkyIntensity;
		float LightingIntensity;
		glm::vec3 SunTint;
		float SunScale;
		glm::vec3 AmbientColor;
		float AmbientIntensity;
		float FogDensity;
		float LanternScale;
		float ShaftScale;
	};

	inline ProceduralSky MakeProceduralSettings()
	{
		ProceduralSky sky;
		sky.SunIntensity = 4.0f;
		return sky;
	}

	class Environments
	{
	public:
		static constexpr int k_Count = 4;
		static constexpr int k_ProceduralIndex = 3;

		~Environments() { Release(); }

		void Build(Scene& scene);
		void Release();
		void PrepareStep();

		void Select(int index);
		void SetSunAzimuth(float azimuthDegrees);

		static bool IsProceduralPreset(int index) { return GetPreset(index).Path == nullptr; }
		Environment* GetEnvironment(int index) const { return m_Environments[std::clamp(index, 0, k_Count - 1)]; }
		Environment* GetProcedural() const { return m_Environments[k_ProceduralIndex]; }
		ProceduralSky& GetProceduralSettings() { return m_ProceduralSettings; }
		void SetSunFade(float fade);
		void ApplyProcedural();

		int GetIndex() const { return m_Index; }
		bool AllReady() const;
		bool AllLoaded() const;
		Entity GetSky() const { return m_Sky; }
		static const EnvironmentPreset& GetPreset(int index);

	private:
		Environment* m_Environments[k_Count] = {};
		Entity m_Sky;
		int m_Index = 0;
		int m_PrepareCursor = 0;
		ProceduralSky m_ProceduralSettings = MakeProceduralSettings();
		float m_SunFade = 1.0f;
	};

}
