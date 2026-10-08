#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// Compute shaders, storage buffers and instancing (ComputePass, Renderer::Dispatch). A kernel fills
	// a storage buffer with 3i + 1 and a 64 x 64 storage image with a pattern; a second kernel reads the
	// buffer through a readonly block and writes neighbour sums into another; a fragment stage reads
	// both buffers into an R32F strip, and an instanced draw places sixteen quads from the buffer, read
	// in the vertex stage by gl_InstanceIndex. The viewport shows the image, the strip and the quads.
	//
	// On start it checks by readback that every value, every sum, the image's pattern and the sixteen
	// quads come out exactly, which covers writable and readonly storage buffers in compute, a storage
	// image, a uniform buffer in compute, readonly storage buffers in the fragment and vertex stages and
	// the barriers between them.
	class ComputeTest : public GraphicsTest
	{
	public:
		ComputeTest() = default;
		virtual ~ComputeTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

		void RunKernels();
		void RunChecks();

	private:
		TestChecks m_Checks;
		bool m_ChecksDone = false;
		std::shared_ptr<int> m_Alive;

		static constexpr uint32_t k_ValueCount = 256;
		static constexpr uint32_t k_ImageSize = 64;
		static constexpr uint32_t k_Instances = 16;

		Shader* m_FillShader = nullptr;
		Shader* m_SumShader = nullptr;
		Shader* m_ReadShader = nullptr;
		Shader* m_InstanceShader = nullptr;
		ComputePass* m_FillPass = nullptr;
		ComputePass* m_SumPass = nullptr;
		Material* m_ReadMaterial = nullptr;
		Material* m_InstanceMaterial = nullptr;

		GraphicsBuffer* m_Params = nullptr;
		GraphicsBuffer* m_Values = nullptr;
		GraphicsBuffer* m_Sums = nullptr;
		Texture* m_Image = nullptr;
		Framebuffer* m_Strip = nullptr;     // k_ValueCount x 2 R32F: values, then sums
		Framebuffer* m_Instanced = nullptr; // 256 x 16 RGBA8: a quad per instance
	};

}
