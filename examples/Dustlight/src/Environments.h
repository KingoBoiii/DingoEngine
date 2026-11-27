#pragma once

#include <DingoEngine.h>

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

	class Environments
	{
	public:
		static constexpr int k_Count = 3;

		~Environments() { Release(); }

		void Build(Scene& scene);
		void Release();
		void PrepareStep();

		void Select(int index);
		void SetSunAzimuth(float azimuthDegrees);

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
	};

}
