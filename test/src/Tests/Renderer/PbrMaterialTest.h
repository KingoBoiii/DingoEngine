#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// The metallic-roughness PBR material (Renderer3D::CreatePbrMaterial), one check a frame, each read
	// back from an orthographic camera looking down -z so that a sphere's centre texel has N = V = L:
	//  1. a 5 x 5 grid of spheres (metallic down the rows, roughness along the columns) under one light
	//     from the camera: every centre texel matches a CPU evaluation of Brdf.glsl's own text, the peak
	//     falls as roughness rises along every row, and a metal with a pure red base colour lights no
	//     green or blue; one more sphere with an ambient light, an emissive colour and a tinted mesh
	//     colour matches the same CPU evaluation;
	//  2. a lit material's half of a picture is the same pixels with and without a PBR mesh beside it;
	//  3. a constant normal map tilted toward +u or +v on a flat quad lights it brighter from the tilted
	//     side and darker from the other, as the CPU evaluation says; a mesh with u or v mirrored and
	//     the map flipped to match is the same picture; a mesh without UVs keeps its geometric normal.
	//  4. a skinned PBR Fox at its rest pose is the picture of the same mesh drawn through SubmitMesh;
	//  5. twelve PBR copies drawn instanced match the same copies batched on the CPU;
	//  6. a PBR static batch is the same pixels as SubmitMesh;
	//  7. two meshes of one PBR material with different SurfaceOverride roughness and metallic share one
	//     draw, count as overrides and match the CPU evaluation of their own values;
	//  8. a translucent PBR material's alpha is the mesh's, BaseColor's and the albedo's multiplied, an
	//     opaque one's the mesh's alone;
	//  9. a texture in slot 5 of a PBR material draws nothing (and warns once: read the log).
	// 10. a glTF with data-URI PNGs, loaded with ModelLoadParams::PbrMaterials, gives PBR materials: each
	//     slot holds its own image with a full mip chain (colour mips averaged in linear light, a normal map's renormalised),
	//     the factors, Translucent (blend), CullMode None (doubleSided) and the occlusion strength are the
	//     file's, an image shared by two slots loads once, all draw as PBR and survive a Reload; the same
	//     file without the flag has no PBR shader and one mip;
	// 11. a Scene entity with a SkinnedMeshRendererComponent whose Material is null draws a PBR-imported
	//     Fox through the PBR path (Scene::RenderEntities3D), and the Fox imported without the flag
	//     through none.
	// Check 1 also renders the roughness-0.1 column under a light dim enough that its peak does not clip.
	class PbrMaterialTest : public GraphicsTest
	{
	public:
		PbrMaterialTest() = default;
		virtual ~PbrMaterialTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		Framebuffer* CreateTarget(uint32_t width, uint32_t height);
		Material* CreatePbr(const PbrMaterialParams& params);
		void ReadAll(std::vector<Framebuffer*> targets, std::function<void(const std::vector<TexturePixels>&)> done);
		template<typename Submit>
		Renderer3D::Statistics Render(Framebuffer* target, const glm::mat4& viewProjection, const glm::vec4& clear, Submit&& submit);

		void DrawView(float aspect);
		void DrawLabels(float aspect);
		void SubmitGrid(Renderer3D& renderer);

		void CheckGrid();
		void CheckMixed();
		void CheckCoexistence();
		void CheckFox();
		void CheckInstancing();
		void CheckStaticBatch();
		void CheckSurfaceOverrides();
		void CheckTranslucency();
		void CheckPastLastSlot();
		void CheckNormalMap();
		void CheckImport();
		void CheckSceneSkinned();

	private:
		TestChecks m_Checks;
		Renderer3D* m_Renderer = nullptr;
		Font* m_Font = nullptr;
		std::shared_ptr<int> m_Alive;
		std::vector<std::function<void()>> m_Steps;
		size_t m_NextStep = 0;

		Mesh* m_ExactSphere = nullptr;
		Mesh* m_SmallSphere = nullptr;
		Mesh* m_Quad = nullptr;
		Texture* m_HalfAlpha = nullptr;
		std::vector<Material*> m_Materials;
		std::vector<Texture*> m_Textures;
		std::vector<Mesh*> m_Meshes;
		std::vector<Material*> m_GridMaterials;
		Material* m_PanelMaterials[3] = {};
		Sampler* m_RepeatSampler = nullptr;
		std::vector<Framebuffer*> m_Targets;
		std::shared_ptr<StaticBatch> m_StaticBatch;

		Model* m_ImportPbr = nullptr;
		Model* m_ImportPlain = nullptr;
		Model* m_SceneFoxPbr = nullptr;
		Model* m_SceneFoxPlain = nullptr;
		Scene* m_Scene = nullptr;
		std::filesystem::path m_ImportFolder;

		Model* m_Fox = nullptr;
		Material* m_FoxMaterial = nullptr;
		glm::mat4 m_FoxTransform{ 1.0f };
		std::vector<glm::mat4> m_FoxPalette;
	};

}
