#include "PackTest.h"

#include "DingoEngine/Asset/Pack/ChaCha20.h"
#include "DingoEngine/Asset/Pack/PackReader.h"
#include "DingoEngine/Core/CacheManager.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>

namespace Dingo
{

	namespace
	{

		std::vector<uint8_t> ReadDiskFile(const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::vector<uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		}

		bool WriteDiskFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
		{
			std::filesystem::create_directories(path.parent_path());
			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			return static_cast<bool>(stream);
		}

		bool WriteDiskFile(const std::filesystem::path& path, std::string_view text)
		{
			return WriteDiskFile(path, std::vector<uint8_t>(text.begin(), text.end()));
		}

		std::string ToUtf8(const std::filesystem::path& path)
		{
			const std::u8string text = path.generic_u8string();
			return std::string(text.begin(), text.end());
		}

		std::filesystem::path FromUtf8(std::string_view text)
		{
			return std::filesystem::path(std::u8string(text.begin(), text.end()));
		}

		constexpr std::string_view k_PackOnlyShader =
			"#type vertex\n#version 450\n#include \"pack_only_common.glsl\"\nlayout(location = 0) in vec2 inPosition;\n"
			"void main() { gl_Position = vec4(inPosition * PACK_SCALE, 0.0, 1.0); }\n"
			"#type fragment\n#version 450\nlayout(location = 0) out vec4 outColor;\nvoid main() { outColor = vec4(1.0); }\n";
		std::vector<uint8_t> FromHex(std::string_view hex)
		{
			std::vector<uint8_t> bytes;
			for (size_t i = 0; i + 1 < hex.size(); i += 2)
				bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
			return bytes;
		}

		bool Contains(const std::vector<uint8_t>& bytes, std::string_view marker)
		{
			return std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end(),
				[](uint8_t b, char c) { return b == static_cast<uint8_t>(c); }) != bytes.end();
		}

		constexpr uint32_t k_PatchTextureWidth = 4;
		constexpr std::string_view k_PackOnlyInclude = "#define PACK_SCALE 0.5\n";

	}

	PackTest::Snapshot PackTest::TakeSnapshot()
	{
		auto countModel = [](const std::filesystem::path& path)
		{
			ModelCounts counts;
			std::unique_ptr<Model> model(Model::LoadFromFile(path));
			if (!model)
				return counts;

			counts.SubMeshes = model->GetSubMeshCount();
			for (const SubMesh& subMesh : model->GetSubMeshes())
				counts.Vertices += subMesh.MeshData ? subMesh.MeshData->GetVertexCount() : 0;
			counts.Joints = model->GetSkeleton() ? model->GetSkeleton()->GetJointCount() : 0;
			counts.Clips = model->GetAnimationCount();
			for (uint32_t i = 0; i < model->GetAnimationCount(); ++i)
				counts.Events += model->GetAnimation(i)->GetEvents().size();
			return counts;
		};

		Snapshot snapshot;
		snapshot.Fox = countModel("models/Fox/Fox.gltf");
		snapshot.Duck = countModel("models/Duck/Duck.gltf");
		snapshot.DuckEmbedded = countModel("models/Duck/DuckEmbedded.glb");

		if (std::shared_ptr<AudioClip> clip = Application::Get().GetAudioEngine().LoadClip("audio/beep.wav"))
			snapshot.ClipFrames = clip->GetFrameCount();

		if (Font* font = Font::Create("fonts/arial.ttf"))
		{
			snapshot.FontWidth = font->GetStringWidth("Packed: AaBbQq 0123", 1.0f);
			DestroyAndDelete(font);
		}

		Shader* shader = Shader::CreateFromFile("PackTest_asset_test", "shaders/asset_test.glsl");
		snapshot.ShaderValid = shader && shader->IsValid();
		DestroyAndDelete(shader);
		return snapshot;
	}

	void PackTest::Initialize()
	{
		Renderer2DTest::Initialize();

		m_Checks.clear();
		m_Stage = 0;
		m_Done = false;
		m_Files.clear();
		m_AssetFileCount = 0;
		m_LoosePixels.reset();
		m_PackedPixels.reset();

		m_WorkDirectory = CacheManager::GetCacheDirectory("pack-test");
		std::error_code ec;
		std::filesystem::remove_all(m_WorkDirectory, ec);
		std::filesystem::create_directories(m_WorkDirectory, ec);
		m_BasePackage = m_WorkDirectory / "base.dpak";
		m_Key = PackageKey::Generate();
		m_PatchPackage = m_WorkDirectory / "patch.dpak";

		AssetManager& assets = Application::Get().GetAssetManager();
		const std::filesystem::path root = assets.GetRootDirectory();
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
		{
			if (!entry.is_regular_file())
				continue;
			m_Files.push_back(ToUtf8(entry.path().lexically_relative(root)));
			if (AssetTypeFromExtension(entry.path().extension()) != AssetType::None)
				++m_AssetFileCount;
		}
		std::sort(m_Files.begin(), m_Files.end());

		m_Loose = TakeSnapshot();
		m_LooseTexture = Texture::CreateFromFile("textures/container.jpg");
		if (m_LooseTexture)
			m_LooseTexture->ReadPixels([this](const TexturePixels& pixels) { m_LoosePixels = pixels; });

		CheckCipher();
		CheckPackaging();
		CheckEncryption();
		CheckMountedBase();
	}

	void PackTest::CheckPackaging()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		const PackageResult result = AssetManager::Package(PackageParams()
			.SetSourceDirectory(assets.GetRootDirectory())
			.SetOutputPath(m_BasePackage)
			.SetKey(m_Key));
		std::error_code ec;
		Check(result.Success && result.Error.empty(), std::format("Package writes '{}' ({})", m_BasePackage.filename().string(), result.Error));
		Check(result.FileCount == m_Files.size(), std::format("Package takes every file under the root ({} of {})", result.FileCount, m_Files.size()));
		Check(result.PackedBytes == std::filesystem::file_size(m_BasePackage, ec) && result.PackedBytes < result.RawBytes,
			std::format("PackageResult reports the archive's size, smaller than the files ({} of {} bytes)", result.PackedBytes, result.RawBytes));

		const PackageResult defaultRoot = AssetManager::Package(PackageParams().SetOutputPath(m_WorkDirectory / "default-root.dpak"));
		Check(defaultRoot.Success && defaultRoot.FileCount == m_Files.size(), "Package without a source directory packages the AssetManager's root");

		uint32_t excludedCount = 0;
		for (const std::string& file : m_Files)
			excludedCount += file.ends_with(".wav") || file.starts_with("models/Duck/");
		const PackageResult excluded = AssetManager::Package(PackageParams()
			.SetSourceDirectory(assets.GetRootDirectory())
			.SetOutputPath(m_WorkDirectory / "excluded.dpak")
			.AddExclude("*.wav")
			.AddExclude("Duck/"));
		Check(excluded.Success && excluded.FileCount + excludedCount == m_Files.size(), "Exclude globs leave out file names and directories");

		const std::filesystem::path ignoreSource = m_WorkDirectory / "ignore-source";
		WriteDiskFile(ignoreSource / "keep.txt", "kept");
		WriteDiskFile(ignoreSource / "notes/draft.psd", "left out");
		WriteDiskFile(ignoreSource / "notes/readme.txt", "kept");
		WriteDiskFile(ignoreSource / ".dpakignore", "# work files\n*.psd\n");
		const PackageResult ignored = AssetManager::Package(PackageParams().SetSourceDirectory(ignoreSource).SetOutputPath(m_WorkDirectory / "ignored.dpak"));
		Check(ignored.Success && ignored.FileCount == 2, "a .dpakignore in the source root leaves its matches (and itself) out");

		std::filesystem::create_directories(m_WorkDirectory / "empty-source", ec);
		const PackageResult empty = AssetManager::Package(PackageParams().SetSourceDirectory(m_WorkDirectory / "empty-source").SetOutputPath(m_WorkDirectory / "empty.dpak"));
		Check(!empty.Success && !empty.Error.empty(), "Package refuses a directory with no files");

#ifndef DE_PLATFORM_WINDOWS
		const std::filesystem::path clashSource = m_WorkDirectory / "clash-source";
		WriteDiskFile(clashSource / "textures/Stone.png", "a");
		WriteDiskFile(clashSource / "textures/stone.png", "b");
		const PackageResult clash = AssetManager::Package(PackageParams().SetSourceDirectory(clashSource).SetOutputPath(m_WorkDirectory / "clash.dpak"));
		Check(!clash.Success && clash.Error.find("case") != std::string::npos, "Package refuses two paths that differ only by case");
#endif

		const std::filesystem::path patchSource = m_WorkDirectory / "patch-source";
		WriteDiskFile(patchSource / "shaders/pack_only.glsl", k_PackOnlyShader);
		WriteDiskFile(patchSource / "shaders/pack_only_common.glsl", k_PackOnlyInclude);
		const std::vector<uint8_t> patchPixels(k_PatchTextureWidth * 2 * 4, 0xC8);
		std::filesystem::create_directories(patchSource / "textures", ec);
		FileSystem::WriteImage(patchSource / "textures/container.jpg", k_PatchTextureWidth, 2, 4, patchPixels.data());
		const PackageResult patch = AssetManager::Package(PackageParams().SetSourceDirectory(patchSource).SetOutputPath(m_PatchPackage));
		Check(patch.Success && patch.FileCount == 3, "a patch package builds from a second directory");
	}

	void PackTest::CheckCipher()
	{
		Internal::ChaCha20::Key key;
		for (size_t i = 0; i < key.size(); ++i)
			key[i] = static_cast<uint8_t>(i);

		Internal::ChaCha20::Nonce nonce = { 0, 0, 0, 0x09, 0, 0, 0, 0x4a, 0, 0, 0, 0 };
		uint8_t block[64];
		Internal::ChaCha20::Block(key, 1, nonce, block);
		Check(std::vector<uint8_t>(block, block + 64) == FromHex("10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"
			"d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e"), "ChaCha20 matches RFC 8439's block function vector (2.3.2)");

		constexpr std::string_view plaintext = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
		std::vector<uint8_t> text(plaintext.begin(), plaintext.end());
		nonce = { 0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0 };
		Internal::ChaCha20::Apply(key, nonce, 1, text.data(), text.size());
		Check(text == FromHex("6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b357"
			"1639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b"
			"8eedf2785e42874d"), "ChaCha20 matches RFC 8439's encryption vector (2.4.2)");
		Internal::ChaCha20::Apply(key, nonce, 1, text.data(), text.size());
		Check(text == std::vector<uint8_t>(plaintext.begin(), plaintext.end()), "ChaCha20 decrypts what it encrypted");

		const PackageKey generated = PackageKey::Generate();
		const std::optional<PackageKey> parsed = PackageKey::FromHex(generated.ToHex());
		Check(!generated.IsNull() && parsed && *parsed == generated && PackageKey().IsNull(), "PackageKey round-trips through hex");
		Check(!PackageKey::FromHex(generated.ToHex().substr(1)) && !PackageKey::FromHex(std::string(63, '0') + "g")
			&& PackageKey::FromHex(generated.ToHex().insert(32, "\n  ")), "PackageKey::FromHex skips whitespace and refuses anything but 64 hex digits");

		const std::filesystem::path keyFile = m_WorkDirectory / "test.key";
		WriteDiskFile(keyFile, "# a comment\r\n" + generated.ToHex() + "\r\n");
		const std::optional<PackageKey> fromFile = PackageKey::FromFile(keyFile);
		Check(fromFile && *fromFile == generated && !PackageKey::FromFile(m_WorkDirectory / "missing.key"), "PackageKey::FromFile reads a key file with comments and CRLF");
	}

	void PackTest::CheckEncryption()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const std::vector<uint8_t> encrypted = ReadDiskFile(m_BasePackage);
		const std::vector<uint8_t> plain = ReadDiskFile(m_WorkDirectory / "default-root.dpak");

		// Five bytes or more each, so random ciphertext matching one by chance is out of the question.
		constexpr std::string_view k_Markers[] = { "textures/", "models/", "#type vertex", std::string_view("glTF\x02\0\0\0", 8),
			"\x89PNG\r\n\x1a\n", std::string_view("Exif\0\0", 6), "WAVEfmt " };
		const bool plainShowsMarkers = Contains(plain, std::string_view("Exif\0\0", 6)) && Contains(plain, "\x89PNG\r\n\x1a\n");
		bool encryptedHidesMarkers = !encrypted.empty();
		for (std::string_view marker : k_Markers)
			encryptedHidesMarkers = encryptedHidesMarkers && !Contains(encrypted, marker);
		Check(plainShowsMarkers && encryptedHidesMarkers, "an encrypted package shows no file names or file signatures (an unencrypted one does)");

		std::string error;
		Check(!Internal::PackReader::Open(m_BasePackage, PackageKey(), error) && error.find("no key") != std::string::npos,
			"an encrypted package refuses to open without a key");
		PackageKey wrong = m_Key;
		wrong.Bytes[7] ^= 0x01;
		Check(!Internal::PackReader::Open(m_BasePackage, wrong, error) && error == "wrong key", "an encrypted package refuses a key one bit off");
		Check(assets.Unpackage(m_BasePackage, wrong) == k_InvalidPackage && assets.Unpackage(m_BasePackage, PackageKey()) == k_InvalidPackage,
			"Unpackage with a wrong or null key returns k_InvalidPackage");

		const PackageResult again = AssetManager::Package(PackageParams()
			.SetSourceDirectory(assets.GetRootDirectory())
			.SetOutputPath(m_WorkDirectory / "base-again.dpak")
			.SetKey(m_Key));
		const std::vector<uint8_t> second = ReadDiskFile(m_WorkDirectory / "base-again.dpak");
		Check(again.Success && second.size() == encrypted.size() && second != encrypted,
			"packaging the same files with the same key twice gives different bytes (a fresh salt each time)");
	}

	void PackTest::CheckMountedBase()
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		Check(assets.Unpackage(m_WorkDirectory / "missing.dpak", PackageKey()) == k_InvalidPackage, "Unpackage of a missing file returns k_InvalidPackage");

		m_Base = assets.Unpackage(m_BasePackage, m_Key);
		Check(m_Base != k_InvalidPackage && assets.IsPackageMounted(m_Base), "Unpackage mounts the package");

		const std::span<const AssetHandle> packaged = assets.GetPackageAssets(m_Base);
		bool everyAssetRegistered = packaged.size() == m_AssetFileCount;
		for (const std::string& name : m_Files)
		{
			const std::filesystem::path file = FromUtf8(name);
			const AssetHandle handle = assets.FindByPath(file);
			if (AssetTypeFromExtension(file.extension()) != AssetType::None)
				everyAssetRegistered = everyAssetRegistered && std::find(packaged.begin(), packaged.end(), handle) != packaged.end();
		}
		Check(everyAssetRegistered, std::format("GetPackageAssets holds a registration for every asset file ({} of {})", packaged.size(), m_AssetFileCount));

		uint32_t identical = 0, packedSource = 0;
		for (const std::string& name : m_Files)
		{
			const std::filesystem::path file = FromUtf8(name);
			std::optional<std::vector<uint8_t>> packed = VirtualFileSystem::ReadBytes(file);
			identical += packed && *packed == ReadDiskFile(assets.GetRootDirectory() / file);
			std::optional<VfsResolvedPath> resolved = VirtualFileSystem::Resolve(file);
			packedSource += resolved && resolved->DiskPath.empty() && !VirtualFileSystem::IsLooseFile(file);
		}
		Check(identical == m_Files.size(), std::format("every packed file reads back byte for byte ({} of {})", identical, m_Files.size()));
		Check(packedSource == m_Files.size(), "a mounted package wins over the loose files");
		Check(!VirtualFileSystem::GetWriteTime("textures/container.jpg").has_value(), "a packed file has no write time, so hot reload never watches it");

		const AssetHandle containerHandle = assets.FindByPath("textures/container.jpg");
		Check(assets.GetAssetSource(containerHandle) == m_BasePackage.filename().string(), "GetAssetSource names the package an asset comes from");

		const Snapshot packed = TakeSnapshot();
		Check(packed.Fox == m_Loose.Fox && m_Loose.Fox.Joints > 0 && m_Loose.Fox.Events > 0,
			std::format("a packed skinned glTF + .bin + .events loads like the loose one ({} vertices, {} joints, {} clips, {} events)",
				packed.Fox.Vertices, packed.Fox.Joints, packed.Fox.Clips, packed.Fox.Events));
		Check(packed.Duck == m_Loose.Duck && m_Loose.Duck.Vertices > 0, "a packed glTF with an external .bin and texture loads like the loose one");
		Check(packed.DuckEmbedded == m_Loose.DuckEmbedded && m_Loose.DuckEmbedded.Vertices > 0, "a packed GLB loads like the loose one");
		Check(packed.ClipFrames == m_Loose.ClipFrames && m_Loose.ClipFrames > 0, std::format("a packed audio clip decodes to the same {} frames", packed.ClipFrames));
		Check(packed.FontWidth == m_Loose.FontWidth && m_Loose.FontWidth > 0.0f, "a packed font measures text like the loose one");
		Check(packed.ShaderValid && m_Loose.ShaderValid, "a packed shader compiles");

		m_PackedTexture = Texture::CreateFromFile("textures/container.jpg");
		if (m_PackedTexture)
			m_PackedTexture->ReadPixels([this](const TexturePixels& pixels) { m_PackedPixels = pixels; });

		for (AssetHandle handle : packaged)
			assets.LoadAsync(assets.GetMetadata(handle)->FilePath);

		uint32_t readWhileDecoding = 0;
		for (const std::string& name : m_Files)
			readWhileDecoding += VirtualFileSystem::ReadBytes(FromUtf8(name)).has_value();
		Check(readWhileDecoding == m_Files.size(), "the main thread reads the package while the loader thread decodes from it");
	}

	void PackTest::CheckPatchAndClose()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const AssetHandle container = assets.FindByPath("textures/container.jpg");
		const Texture* managed = assets.GetTexture(container);
		const uint32_t baseWidth = managed ? managed->GetWidth() : 0;

		const PackageHandle patch = assets.Unpackage(m_PatchPackage, PackageKey());
		Check(patch != k_InvalidPackage, "a second package mounts over the first");

		Texture* patched = Texture::CreateFromFile("textures/container.jpg");
		Check(patched && patched->GetWidth() == k_PatchTextureWidth && baseWidth != k_PatchTextureWidth, "the later package overrides a file the earlier one holds");
		DestroyAndDelete(patched);
		Check(assets.GetTexture(container) == managed && managed && managed->GetWidth() == k_PatchTextureWidth, "Unpackage reloads an overridden asset in place");

		Shader* packOnly = Shader::CreateFromFile("PackTest_pack_only", "shaders/pack_only.glsl");
		Check(packOnly && packOnly->IsValid() && packOnly->GetIncludedFiles().size() == 1, "a shader and the file it #includes both read from a package");
		DestroyAndDelete(packOnly);

		assets.ClosePackage(patch);
		Check(!assets.IsPackageMounted(patch) && assets.GetPackageAssets(patch).empty(), "ClosePackage unmounts the package");
		Check(!VirtualFileSystem::Exists("shaders/pack_only.glsl") && !IsValidAssetHandle(assets.FindByPath("shaders/pack_only.glsl")),
			"ClosePackage drops the files and registrations only it had");
		Check(managed && managed->GetWidth() == baseWidth, "ClosePackage reloads what the earlier package serves again");

		assets.ClosePackage(m_Base);
		m_Base = k_InvalidPackage;
		Check(!IsValidAssetHandle(assets.FindByPath("textures/container.jpg")), "ClosePackage removes the registrations the package made");
		std::optional<VfsResolvedPath> loose = VirtualFileSystem::Resolve("textures/container.jpg");
		const AssetHandle reloaded = assets.Load("textures/container.jpg");
		Check(loose && !loose->DiskPath.empty() && assets.GetAssetSource(reloaded) == "loose" && assets.IsReady(reloaded),
			"after ClosePackage the loose files serve again");
		assets.Remove(reloaded);
	}

	void PackTest::CheckDamagedPackages()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		const std::vector<uint8_t> original = ReadDiskFile(m_BasePackage);

		std::string error;
		std::shared_ptr<Internal::PackReader> reader = Internal::PackReader::Open(m_BasePackage, m_Key, error);
		const std::optional<uint32_t> entry = reader ? reader->Find("textures/container.jpg") : std::nullopt;
		if (!entry)
		{
			Check(false, "the base package opens for the damage checks");
			return;
		}

		std::vector<uint8_t> damaged = original;
		const Internal::Pack::TocEntry& toc = reader->GetEntry(*entry).Toc;
		damaged[static_cast<size_t>(toc.Offset + toc.PackedSize / 2)] ^= 0x5A;
		const std::filesystem::path damagedPath = m_WorkDirectory / "damaged.dpak";
		WriteDiskFile(damagedPath, damaged);
		reader.reset();

		const PackageHandle damagedPackage = assets.Unpackage(damagedPath, m_Key);
		Check(damagedPackage != k_InvalidPackage, "a package with a damaged entry still mounts");
		Check(!VirtualFileSystem::ReadBytes("textures/container.jpg").has_value(), "a flipped byte fails the entry's CRC check");
		Texture* fromDamaged = Texture::CreateFromFile("textures/container.jpg");
		Check(fromDamaged == nullptr, "a factory reading a damaged entry fails cleanly");
		DestroyAndDelete(fromDamaged);
		Check(VirtualFileSystem::ReadBytes("audio/beep.wav").has_value(), "the other entries of a damaged package still read");
		assets.ClosePackage(damagedPackage);

		std::vector<uint8_t> badToc = original;
		// Mid-stream: the zlib trailer's Adler-32 isn't checked (stb skips it), the TOC's CRC is.
		Internal::Pack::Header header{};
		std::memcpy(&header, original.data(), sizeof(header));
		badToc[static_cast<size_t>(header.TocOffset + header.TocPackedSize / 2)] ^= 0x5A;
		WriteDiskFile(m_WorkDirectory / "bad-toc.dpak", badToc);
		Check(assets.Unpackage(m_WorkDirectory / "bad-toc.dpak", m_Key) == k_InvalidPackage, "a damaged table of contents refuses to mount");

		std::vector<uint8_t> truncated(original.begin(), original.begin() + original.size() / 2);
		WriteDiskFile(m_WorkDirectory / "truncated.dpak", truncated);
		Check(assets.Unpackage(m_WorkDirectory / "truncated.dpak", m_Key) == k_InvalidPackage, "a truncated package refuses to mount");

		WriteDiskFile(m_WorkDirectory / "not-a-package.dpak", "just some text, not a package");
		Check(assets.Unpackage(m_WorkDirectory / "not-a-package.dpak", m_Key) == k_InvalidPackage, "a file that isn't a package refuses to mount");
	}

	void PackTest::Update(float deltaTime)
	{
		AssetManager& assets = Application::Get().GetAssetManager();

		if (m_Stage == 0 && m_LoosePixels && m_PackedPixels && assets.GetPendingCount() == 0)
		{
			Check(m_LoosePixels->Width > 0 && m_LoosePixels->Data == m_PackedPixels->Data, "a packed texture's pixels match the loose one's");

			uint32_t ready = 0;
			for (AssetHandle handle : assets.GetPackageAssets(m_Base))
				ready += assets.IsReady(handle);
			Check(ready == assets.GetPackageAssets(m_Base).size(), std::format("LoadAsync loads every package asset ({} of {})", ready, assets.GetPackageAssets(m_Base).size()));

			CheckPatchAndClose();
			CheckDamagedPackages();
			FinishChecks();
			m_Stage = 1;
		}

		m_Renderer->BeginScene(m_ProjectionViewMatrix);
		m_Renderer->Clear(m_ClearColor);
		if (m_LooseTexture)
			m_Renderer->DrawQuad({ -1.55f, 0.0f }, { 2.0f, 2.0f }, m_LooseTexture);
		if (m_PackedTexture)
			m_Renderer->DrawQuad({ 1.55f, 0.0f }, { 2.0f, 2.0f }, m_PackedTexture);
		m_Renderer->EndScene();
	}

	void PackTest::FinishChecks()
	{
		m_Done = true;
		const int failed = m_Checks.FailedCount();
		if (failed == 0)
			DE_INFO("PackTest: all {} checks passed.", m_Checks.size());
		else
			DE_ERROR("PackTest: {} of {} checks FAILED.", failed, m_Checks.size());
	}

	void PackTest::Cleanup()
	{
		AssetManager& assets = Application::Get().GetAssetManager();
		if (m_Base != k_InvalidPackage)
			assets.ClosePackage(m_Base);
		m_Base = k_InvalidPackage;
		DestroyAndDelete(m_LooseTexture);
		DestroyAndDelete(m_PackedTexture);

		Renderer2DTest::Cleanup();
	}

	void PackTest::ImGuiRender()
	{
		Renderer2DTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
		if (!m_Done)
			ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[....] waiting for the packed loads and pixel readbacks");
	}

}
