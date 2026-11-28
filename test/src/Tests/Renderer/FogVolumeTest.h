#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace Dingo
{

	// Local fog volumes (Renderer3D::SubmitFogVolume, FogVolumeComponent). Checks 1 and 11-14 read the CPU
	// records and the statistics the latest scene left (Internal::Renderer3DAccess), on a 32 x 18 RGBA16F
	// target, after 10 frames. A camera at (0, 1, 0) looks down -Z at a 16:9 aspect:
	//  1. with Capabilities.Volumetrics off, an orthographic camera, or a volume behind the camera or
	//     past the volumetric MaxDistance, the scene keeps no volume record and dispatches nothing, and a
	//     volume in front is kept and, with no global fog, dispatches the froxel passes (H15);
	//  The next checks read the scatter and integrated volumes back, on a 32 x 18 x 16 grid without Temporal
	//  unless said (the froxel sample of a texel is its slice's centre), against a CPU mirror of the medium:
	//  2. a box with no VolumetricFog keeps the distance fog and holds exactly its own extinction, and a
	//     VolumetricFog replaces the distance fog;
	//  3. a rotated box's extinction inside, outside and across its edge fade; 4. the same for an ellipsoid;
	//  5. a tall volume's height fade and band at every texel from 0 to 20 m above its bottom;
	//  6. near and far fades scale the in-scatter with the camera distance and leave the extinction alone;
	//  7. NoiseMin equal to NoiseMax is no noise at all; NoiseMin 0 and NoiseMax 2 follow the CPU noise
	//     texture, phase included;
	//  8. a noise that drifts under SetFogVolumeTime stays within 2/255 of the Temporal off volume after 40
	//     Temporal frames, at a clock of 1x and 4x, and is not close to the same volume a few frames earlier;
	//  9. a volume's ScatterTint, Albedo, ambient and light intensities, Anisotropy and OutsideShadowLight;
	//  10. two overlapping volumes and the global fog against a CPU mixture of the three, and the global
	//      MaxInScatter cap on the integrated volume.
	//  11. 10 volumes against a budget of 8 keep the 8 nearest in submission order and drop the 2
	//      farthest, 20 against a budget of 20 keep 16, and equal distances drop the later submissions;
	//  12. FogVolumeComponents through Scene::SubmitLights record exactly what SubmitFogVolume with the
	//      same settings and world matrices records;
	//  13. a disabled component, a component on an inactive entity and one without a Transform3DComponent
	//      submit nothing, and a duplicated entity submits a second copy of the volume;
	//  14. SubmitFogVolume rejects a NaN, a negative or an infinite field, a flat or non-affine transform
	//      and an unknown enum, and accepts the rest;
	//  15. point and spot lights scatter in the froxel volume (global fog and volumes) as the CPU's Henyey-
	//      Greenstein x falloff^2 x cone^2 says; a LocalLightIntensity of 0 or a light's VolumetricIntensity
	//      of 0 leaves the directional-only volume, a spot outside its cone adds nothing, and a spot behind
	//      a shadow caster adds nothing while the same spot unshadowed does; the global LocalLightIntensity
	//      of 0 silences the lights in a volume too, and a sun's VolumetricIntensity scales its in-scatter
	//      in a scene with only the global fog;
//  16. the ShadowSamples debug view paints the froxel scatter volume green inside the lit cascades, blue
//      past the last one and yellow with no shadow-casting sun, leaves the normal picture when off, and
//      the slice request copies a slice (times the exposure) into a texture of the grid's x and y size.
	class FogVolumeTest : public GraphicsTest
	{
	public:
		FogVolumeTest() = default;
		virtual ~FogVolumeTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	public:
		struct Placed
		{
			FogVolume Settings;
			glm::mat4 World{ 1.0f };
		};

		struct Sun
		{
			glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
			glm::vec3 Color{ 1.0f };
			bool Casts = false;
			float VolumetricIntensity = 1.0f;
		};

		struct Scene
		{
			Scene()
			{
				Fog.Density = 0.0f;
				Fog.Temporal = false;
			}

			VolumetricFog Fog;
			bool Global = true;
			bool DistanceFog = false;
			float Ambient = 0.5f;
			double Time = 0.0;
			std::vector<Sun> Suns;
			std::vector<PointLight> Points;
			std::vector<SpotLight> Spots;
			bool Wall = false;
			std::vector<Placed> Volumes;
		};

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunChecks();
		void CheckMedia();
		void CheckShapes();
		void CheckHeight();
		void CheckFades();
		void CheckNoise();
		void CheckLight();
		void CheckMixture();
		void CheckLocalLights();
		void StepDrift();
		Renderer3D::Statistics Draw(const Scene& scene);
		Texture* Scatter() const;
		Texture* Integrated() const;
		void CheckOff();
		void CheckBudget();
		void CheckScenePath();
		void CheckSceneRules();
		void CheckInvalid();
		void CheckDebugView();

		Renderer3D::Statistics Render(Renderer3D& renderer, const std::function<void(Renderer3D&)>& body, bool orthographic = false);

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		uint32_t m_FramesBeforeChecks = 0;

		Renderer3D* m_Renderer = nullptr;
		Renderer3D* m_Wide = nullptr;
		Renderer3D* m_Off = nullptr;
		Renderer3D* m_Probe = nullptr;
		Framebuffer* m_Target = nullptr;
		Mesh* m_Box = nullptr;
		std::shared_ptr<int> m_Alive;

		bool m_DriftActive = false;
		uint32_t m_DriftFrame = 0;
		double m_DriftTime = 5.0;
	};

}
