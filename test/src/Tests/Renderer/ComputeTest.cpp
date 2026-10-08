#include "ComputeTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <cstring>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr const char* k_FillSource = R"(
#type compute
#version 450
layout(local_size_x = 64) in;
layout(std140, binding = 0) uniform FillData { uvec4 Count; }; // x = values to write
layout(std430, binding = 1) buffer Values { uint values[]; };
layout(binding = 2, rgba8) uniform writeonly image2D u_Image;
void main()
{
	uint i = gl_GlobalInvocationID.x;
	if (i < Count.x)
		values[i] = i * 3u + 1u;

	ivec2 size = imageSize(u_Image);
	if (i < uint(size.x * size.y))
	{
		ivec2 p = ivec2(int(i) % size.x, int(i) / size.x);
		imageStore(u_Image, p, vec4(float(p.x * 4) / 255.0, float(p.y * 4) / 255.0, float((p.x ^ p.y) & 63) / 255.0, 1.0));
	}
}
)";

		constexpr const char* k_SumSource = R"(
#type compute
#version 450
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer Source { uint source[]; };
layout(std430, binding = 1) writeonly buffer Target { uint target[]; };
void main()
{
	uint i = gl_GlobalInvocationID.x;
	if (i < 256u)
		target[i] = source[i] + source[(i + 1u) % 256u];
}
)";

		// One pass dispatched again and again, each step reading what the last one wrote.
		constexpr const char* k_StepSource = R"(
#type compute
#version 450
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Steps { uint steps[]; };
void main()
{
	uint i = gl_GlobalInvocationID.x;
#ifdef DE_STEP_ZERO
	steps[i] = 0u;
#else
	steps[i] = steps[i] + 1u;
#endif
}
)";

		constexpr const char* k_ReadSource = R"(
#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
layout(location = 0) in vec2 v_TexCoord;
layout(std430, binding = 0) readonly buffer Values { uint values[]; };
layout(std430, binding = 1) readonly buffer Sums { uint sums[]; };
layout(location = 0) out vec4 o_Value;
void main()
{
	ivec2 p = ivec2(gl_FragCoord.xy);
	o_Value = vec4(float(p.y == 0 ? values[p.x] : sums[p.x]));
}
)";

		// Sixteen quads across a 256 x 16 target, each placed from the value buffer, so instance i
		// lands in columns 16i + 4 .. 16i + 12 only if the vertex stage read values[i] = 3i + 1.
		constexpr const char* k_InstanceSource = R"(
#type vertex
#version 450
layout(std430, binding = 0) readonly buffer Values { uint values[]; };
layout(location = 0) out vec4 v_Color;
const vec2 CORNERS[6] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
void main()
{
	uint instance = uint(gl_InstanceIndex);
	float slot = float((values[instance] - 1u) / 3u);
	vec2 corner = CORNERS[gl_VertexIndex];
	float x = (slot * 16.0 + 4.0 + corner.x * 8.0) / 256.0;
	gl_Position = vec4(x * 2.0 - 1.0, corner.y * 2.0 - 1.0, 0.0, 1.0);
	v_Color = vec4(float(instance) / 15.0, 1.0 - float(instance) / 15.0, 0.5, 1.0);
}

#type fragment
#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;
void main() { o_Color = v_Color; }
)";

		Material* MakeMaterial(const char* name, Shader* shader)
		{
			return Material::Create(MaterialParams()
				.SetDebugName(name)
				.SetShader(shader)
				.SetCullMode(CullMode::None)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendMode(BlendMode::Opaque));
		}
	}

	void ComputeTest::Initialize()
	{
		m_Checks.clear();
		m_ChecksDone = false;
		m_Alive = std::make_shared<int>(0);

		m_FillShader = Shader::CreateFromSource("ComputeTestFill", k_FillSource);
		m_SumShader = Shader::CreateFromSource("ComputeTestSum", k_SumSource);
		m_ReadShader = Shader::CreateFromSource("ComputeTestRead", k_ReadSource);
		m_InstanceShader = Shader::CreateFromSource("ComputeTestInstances", k_InstanceSource);
		m_StepShader = Shader::Create(ShaderParams().SetName("ComputeTestStep").SetSourceCode(k_StepSource));
		m_ZeroShader = Shader::Create(ShaderParams().SetName("ComputeTestZero").SetSourceCode(k_StepSource).AddDefine("DE_STEP_ZERO"));

		m_Params = GraphicsBuffer::CreateUniformBuffer(sizeof(glm::uvec4), "ComputeTest params");
		m_Values = GraphicsBuffer::CreateStorageBuffer(k_ValueCount * sizeof(uint32_t), "ComputeTest values");
		m_Sums = GraphicsBuffer::CreateStorageBuffer(k_ValueCount * sizeof(uint32_t), "ComputeTest sums");
		m_Steps = GraphicsBuffer::CreateStorageBuffer(k_StepCount * sizeof(uint32_t), "ComputeTest steps");
		{
			std::vector<uint32_t> seed(k_ValueCount);
			for (uint32_t i = 0; i < k_ValueCount; ++i)
				seed[i] = i * 7 + 2;
			m_Seeded = GraphicsBuffer::CreateStorageBuffer(k_ValueCount * sizeof(uint32_t), "ComputeTest seeded", seed.data());
		}
		m_Image = Texture::Create(TextureParams()
			.SetDebugName("ComputeTest image")
			.SetWidth(k_ImageSize)
			.SetHeight(k_ImageSize)
			.SetFormat(TextureFormat::RGBA8_UNORM)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsStorage(true));

		m_FillPass = ComputePass::Create(ComputePassParams().SetDebugName("ComputeTest fill").SetShader(m_FillShader));
		m_FillPass->SetUniformBuffer(0, m_Params);
		m_FillPass->SetStorageBuffer(1, m_Values);
		m_FillPass->SetStorageTexture(2, m_Image);

		m_SumPass = ComputePass::Create(ComputePassParams().SetDebugName("ComputeTest sum").SetShader(m_SumShader));
		m_SumPass->SetStorageBuffer(0, m_Values);
		m_SumPass->SetStorageBuffer(1, m_Sums);

		m_StepPass = ComputePass::Create(ComputePassParams().SetDebugName("ComputeTest step").SetShader(m_StepShader));
		m_StepPass->SetStorageBuffer(0, m_Steps);
		m_ZeroPass = ComputePass::Create(ComputePassParams().SetDebugName("ComputeTest zero").SetShader(m_ZeroShader));
		m_ZeroPass->SetStorageBuffer(0, m_Steps);

		m_ReadMaterial = MakeMaterial("ComputeTest read", m_ReadShader);
		m_ReadMaterial->SetStorageBuffer(0, m_Values);
		m_ReadMaterial->SetStorageBuffer(1, m_Sums);
		m_InstanceMaterial = MakeMaterial("ComputeTest instances", m_InstanceShader);
		m_InstanceMaterial->SetStorageBuffer(0, m_Values);

		m_Strip = Framebuffer::Create(FramebufferParams()
			.SetDebugName("ComputeTest strip")
			.SetWidth(static_cast<int32_t>(k_ValueCount))
			.SetHeight(2)
			.AddAttachment({ TextureFormat::R32F }));
		m_SwapStrip = Framebuffer::Create(FramebufferParams()
			.SetDebugName("ComputeTest swapped strip")
			.SetWidth(static_cast<int32_t>(k_ValueCount))
			.SetHeight(2)
			.AddAttachment({ TextureFormat::R32F }));
		m_Instanced = Framebuffer::Create(FramebufferParams()
			.SetDebugName("ComputeTest instances")
			.SetWidth(256)
			.SetHeight(16)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));
	}

	void ComputeTest::RunKernels()
	{
		// The params buffer is volatile, so it is written in every frame that binds it.
		const glm::uvec4 count(k_ValueCount, 0, 0, 0);
		Renderer::Upload(m_Params, &count, sizeof(count));
		Renderer::Dispatch(m_FillPass, (k_ImageSize * k_ImageSize + 63) / 64);
		Renderer::Dispatch(m_SumPass, (k_ValueCount + 63) / 64);

		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(m_Strip);
		Renderer::Clear(m_Strip, glm::vec4(0.0f));
		Renderer::Draw(m_ReadMaterial, 3);
		Renderer::SetRenderTarget(m_Instanced);
		Renderer::Clear(m_Instanced, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		Renderer::Draw(m_InstanceMaterial, 6, k_Instances);
		Renderer::SetRenderTarget(previous);
	}

	void ComputeTest::RunChecks()
	{
		const std::weak_ptr<int> alive = m_Alive;
		auto readBack = [alive](Texture* texture, std::function<void(const TexturePixels&)> check)
		{
			texture->ReadPixels([alive, check = std::move(check)](const TexturePixels& pixels)
			{
				if (!alive.expired())
					check(pixels);
			});
		};

		// The same pass back to back, and again after a copy out of its buffer: each dispatch must see
		// the last one's writes.
		// Every step equals expected, but the first, which equals first.
		auto checkSteps = [this, alive](uint32_t expected, uint32_t first, const char* name)
		{
			m_Steps->ReadBack([this, alive, expected, first, name](const std::vector<uint8_t>& bytes)
			{
				if (alive.expired())
					return;
				uint32_t wrong = 0;
				if (bytes.size() != k_StepCount * sizeof(uint32_t))
				{
					wrong = k_StepCount;
				}
				else
				{
					for (size_t i = 0; i < k_StepCount; ++i)
					{
						uint32_t value = 0;
						std::memcpy(&value, bytes.data() + i * sizeof(uint32_t), sizeof(value));
						wrong += value != (i == 0 ? first : expected) ? 1 : 0;
					}
				}
				Check(wrong == 0, std::format("{} ({} of {} wrong)", name, wrong, k_StepCount));
			});
		};
		const uint32_t patch[2] = { 1000, 1001 };
		m_Seeded->Upload(patch, sizeof(patch), 4 * sizeof(uint32_t));
		m_Seeded->ReadBack([this, alive](const std::vector<uint8_t>& bytes)
		{
			if (alive.expired())
				return;
			uint32_t wrong = bytes.size() == k_ValueCount * sizeof(uint32_t) ? 0 : k_ValueCount;
			for (uint32_t i = 0; wrong == 0 && i < k_ValueCount; ++i)
			{
				uint32_t value = 0;
				std::memcpy(&value, bytes.data() + i * sizeof(uint32_t), sizeof(value));
				const uint32_t expected = i == 4 ? 1000u : i == 5 ? 1001u : i * 7 + 2;
				wrong += value != expected ? 1 : 0;
			}
			Check(wrong == 0, std::format("a storage buffer holds its initial data and a later GraphicsBuffer::Upload ({} of {} wrong)", wrong, k_ValueCount));
		});

		// The read material's buffers swapped and drawn, then put back: its cached pass re-points its
		// binding set at the new buffers.
		{
			m_ReadMaterial->SetStorageBuffer(0, m_Seeded);
			m_ReadMaterial->SetStorageBuffer(1, m_Values);
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(m_SwapStrip);
			Renderer::Clear(m_SwapStrip, glm::vec4(0.0f));
			Renderer::Draw(m_ReadMaterial, 3);
			Renderer::SetRenderTarget(previous);
			m_ReadMaterial->SetStorageBuffer(0, m_Values);
			m_ReadMaterial->SetStorageBuffer(1, m_Sums);
		}
		readBack(m_SwapStrip->GetAttachment(0), [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "the swapped storage buffers read back");
				return;
			}
			uint32_t wrong = 0;
			for (uint32_t i = 0; i < k_ValueCount; ++i)
			{
				const uint32_t seeded = i == 4 ? 1000u : i == 5 ? 1001u : i * 7 + 2;
				wrong += pixels.GetPixel(i, 0).r != static_cast<float>(seeded) ? 1 : 0;
				wrong += pixels.GetPixel(i, 1).r != static_cast<float>(i * 3 + 1) ? 1 : 0;
			}
			Check(wrong == 0, std::format("a material whose storage buffers are swapped draws from the new ones ({} of {} wrong)", wrong, 2 * k_ValueCount));
		});

		Renderer::Dispatch(m_ZeroPass, k_StepCount / 64);
		for (uint32_t i = 0; i < k_StepDispatches; ++i)
			Renderer::Dispatch(m_StepPass, k_StepCount / 64);
		checkSteps(k_StepDispatches, k_StepDispatches, "one compute pass dispatched back to back sees each dispatch's writes");
		for (uint32_t i = 0; i < k_StepDispatches; ++i)
			Renderer::Dispatch(m_StepPass, k_StepCount / 64);
		checkSteps(2 * k_StepDispatches, 2 * k_StepDispatches, "a pass dispatched after GraphicsBuffer::ReadBack copied its buffer still sees its writes");
		const uint32_t restart = 100;
		m_Steps->Upload(&restart, sizeof(restart));
		for (uint32_t i = 0; i < k_StepDispatches; ++i)
			Renderer::Dispatch(m_StepPass, k_StepCount / 64);
		checkSteps(3 * k_StepDispatches, restart + k_StepDispatches, "a pass dispatched after an Upload into its buffer sees the upload and its own writes");

		readBack(m_Strip->GetAttachment(0), [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "the storage buffers read back through a fragment stage");
				return;
			}
			uint32_t wrongValues = 0, wrongSums = 0;
			for (uint32_t i = 0; i < k_ValueCount; ++i)
			{
				const uint32_t value = i * 3 + 1;
				const uint32_t next = ((i + 1) % k_ValueCount) * 3 + 1;
				wrongValues += pixels.GetPixel(i, 0).r != static_cast<float>(value) ? 1 : 0;
				wrongSums += pixels.GetPixel(i, 1).r != static_cast<float>(value + next) ? 1 : 0;
			}
			Check(wrongValues == 0, std::format("a kernel writes a storage buffer, read in a fragment stage ({} of {} wrong)", wrongValues, k_ValueCount));
			Check(wrongSums == 0, std::format("a second kernel reads it through a readonly block and writes another ({} of {} sums wrong)", wrongSums, k_ValueCount));
		});

		readBack(m_Image, [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "the storage image reads back");
				return;
			}
			uint32_t wrong = 0;
			for (uint32_t y = 0; y < k_ImageSize; ++y)
			{
				for (uint32_t x = 0; x < k_ImageSize; ++x)
				{
					const size_t offset = (static_cast<size_t>(y) * pixels.Width + x) * 4;
					const bool match = pixels.Data[offset] == x * 4 && pixels.Data[offset + 1] == y * 4 &&
						pixels.Data[offset + 2] == ((x ^ y) & 63) && pixels.Data[offset + 3] == 255;
					wrong += match ? 0 : 1;
				}
			}
			Check(wrong == 0, std::format("a kernel writes a storage image ({} of {} pixels wrong)", wrong, k_ImageSize * k_ImageSize));
		});

		readBack(m_Instanced->GetAttachment(0), [this](const TexturePixels& pixels)
		{
			if (pixels.Data.empty())
			{
				Check(false, "the instanced quads read back");
				return;
			}
			uint32_t wrong = 0;
			for (uint32_t i = 0; i < k_Instances; ++i)
			{
				const glm::vec4 inside = pixels.GetPixel(i * 16 + 8, 8);
				const glm::vec4 gap = pixels.GetPixel(i * 16 + 1, 8);
				const float red = static_cast<float>(i) / 15.0f;
				const bool match = std::abs(inside.r - red) <= 1.01f / 255.0f && std::abs(inside.g - (1.0f - red)) <= 1.01f / 255.0f && gap.r == 0.0f && gap.g == 0.0f;
				wrong += match ? 0 : 1;
			}
			Check(wrong == 0, std::format("an instanced draw places each instance from the buffer, read in the vertex stage ({} of {} wrong)", wrong, k_Instances));
		});
	}

	void ComputeTest::Update(float deltaTime)
	{
		RunKernels();
		if (!m_ChecksDone && !Renderer::IsFrameSkipped())
		{
			m_ChecksDone = true;
			RunChecks();
		}

		Renderer2D& renderer2D = Application::Get().GetRenderer2D();
		renderer2D.BeginScene(glm::ortho(-m_AspectRatio, m_AspectRatio, -1.0f, 1.0f, -1.0f, 1.0f));
		renderer2D.Clear(m_ClearColor);
		renderer2D.DrawQuad({ -0.4f, 0.2f }, { 1.0f, 1.0f }, m_Image);
		renderer2D.DrawQuad({ 0.0f, -0.6f }, { 1.6f, 0.1f }, m_Instanced->GetAttachment(0));
		renderer2D.EndScene();
	}

	void ComputeTest::Cleanup()
	{
		m_Alive.reset();
		DestroyAndDelete(m_FillPass);
		DestroyAndDelete(m_SumPass);
		DestroyAndDelete(m_StepPass);
		DestroyAndDelete(m_ZeroPass);
		DestroyAndDelete(m_ReadMaterial);
		DestroyAndDelete(m_InstanceMaterial);
		DestroyAndDelete(m_FillShader);
		DestroyAndDelete(m_SumShader);
		DestroyAndDelete(m_ReadShader);
		DestroyAndDelete(m_InstanceShader);
		DestroyAndDelete(m_StepShader);
		DestroyAndDelete(m_ZeroShader);
		DestroyAndDelete(m_Params);
		DestroyAndDelete(m_Values);
		DestroyAndDelete(m_Sums);
		DestroyAndDelete(m_Steps);
		DestroyAndDelete(m_Seeded);
		DestroyAndDelete(m_Image);
		DestroyAndDelete(m_Strip);
		DestroyAndDelete(m_SwapStrip);
		DestroyAndDelete(m_Instanced);
		m_Checks.clear();
	}

	void ComputeTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void ComputeTest::ImGuiRender()
	{
		ImGui::TextWrapped("Top: a 64 x 64 storage image a kernel wrote. Bottom: sixteen instanced quads placed from a storage buffer.");
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
