#include <DingoEngine/Asset/AssetManager.h>
#include <DingoEngine/Core/IO.h>

#include "DingoEngine/Asset/Pack/PackReader.h"

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace Dingo;

namespace
{

	int Usage()
	{
		std::fputs(
			"usage: DingoPack pack <assetsDir> <out.dpak> (--key <keyFile> | --no-encrypt) [--exclude <glob>]...\n"
			"       DingoPack list <package.dpak> [--key <keyFile>]\n"
			"       DingoPack verify <package.dpak> [--key <keyFile>]\n"
			"       DingoPack genkey <keyFile>\n", stderr);
		return 2;
	}

	struct Arguments
	{
		std::vector<std::string> Positional;
		std::optional<std::string> KeyFile;
		std::vector<std::string> Exclude;
		bool NoEncrypt = false;
	};

	std::optional<Arguments> Parse(int argc, char** argv)
	{
		Arguments arguments;
		for (int i = 2; i < argc; ++i)
		{
			const std::string_view argument = argv[i];
			if ((argument == "--key" || argument == "--exclude") && i + 1 < argc)
			{
				if (argument == "--key")
					arguments.KeyFile = argv[++i];
				else
					arguments.Exclude.push_back(argv[++i]);
			}
			else if (argument == "--no-encrypt")
				arguments.NoEncrypt = true;
			else if (argument.starts_with("--"))
				return std::nullopt;
			else
				arguments.Positional.emplace_back(argument);
		}
		return arguments;
	}

	std::optional<PackageKey> LoadKey(const Arguments& arguments)
	{
		if (!arguments.KeyFile)
			return PackageKey();

		std::optional<PackageKey> key = PackageKey::FromFile(*arguments.KeyFile);
		if (!key)
			std::fprintf(stderr, "DingoPack: '%s' doesn't hold a key (64 hex digits)\n", arguments.KeyFile->c_str());
		return key;
	}

	int Pack(const Arguments& arguments)
	{
		if (arguments.Positional.size() != 2 || arguments.KeyFile.has_value() == arguments.NoEncrypt)
			return Usage();

		std::optional<PackageKey> key = LoadKey(arguments);
		if (!key)
			return 1;

		PackageParams params = PackageParams()
			.SetSourceDirectory(arguments.Positional[0])
			.SetOutputPath(arguments.Positional[1])
			.SetKey(*key);
		params.Exclude = arguments.Exclude;

		const PackageResult result = AssetManager::Package(params);
		for (const std::string& warning : result.Warnings)
			std::fprintf(stderr, "warning: %s\n", warning.c_str());
		if (!result.Success)
		{
			std::fprintf(stderr, "DingoPack: %s\n", result.Error.c_str());
			return 1;
		}

		std::printf("%s: %u files, %llu bytes packed into %llu%s\n", arguments.Positional[1].c_str(), result.FileCount,
			static_cast<unsigned long long>(result.RawBytes), static_cast<unsigned long long>(result.PackedBytes), key->IsNull() ? " (not encrypted)" : "");
		return 0;
	}

	int ListOrVerify(const Arguments& arguments, bool verify)
	{
		if (arguments.Positional.size() != 1 || arguments.NoEncrypt)
			return Usage();

		std::optional<PackageKey> key = LoadKey(arguments);
		if (!key)
			return 1;

		std::string error;
		std::shared_ptr<Internal::PackReader> reader = Internal::PackReader::Open(arguments.Positional[0], error);
		if (!reader)
		{
			std::fprintf(stderr, "DingoPack: %s: %s\n", arguments.Positional[0].c_str(), error.c_str());
			return 1;
		}

		uint32_t damaged = 0;
		for (uint32_t i = 0; i < reader->GetEntryCount(); ++i)
		{
			const Internal::PackReader::EntryInfo& entry = reader->GetEntry(i);
			if (verify)
			{
				if (!reader->Read(i))
				{
					++damaged;
					std::printf("DAMAGED  %s\n", entry.Name.c_str());
				}
				continue;
			}

			const bool deflated = (entry.Toc.Flags & Internal::Pack::EntryFlag_Deflated) != 0;
			std::printf("%12llu %12llu %s  %s\n", static_cast<unsigned long long>(entry.Toc.RawSize),
				static_cast<unsigned long long>(entry.Toc.PackedSize), deflated ? "deflated" : "stored  ", entry.Name.c_str());
		}

		std::printf("%u entries, %llu bytes, %s%s\n", reader->GetEntryCount(), static_cast<unsigned long long>(reader->GetFileSize()),
			reader->IsEncrypted() ? "encrypted" : "not encrypted", verify ? (damaged ? ": DAMAGED" : ": OK") : "");
		return damaged ? 1 : 0;
	}

	int GenerateKey(const Arguments& arguments)
	{
		if (arguments.Positional.size() != 1 || arguments.KeyFile || arguments.NoEncrypt)
			return Usage();

		const std::string contents = "# DingoPack key: keep it out of public repositories (see docs/distribution.md).\n" + PackageKey::Generate().ToHex() + "\n";
		if (!IO::WriteFileAtomic(arguments.Positional[0], contents))
		{
			std::fprintf(stderr, "DingoPack: can't write '%s'\n", arguments.Positional[0].c_str());
			return 1;
		}
		std::printf("wrote %s\n", arguments.Positional[0].c_str());
		return 0;
	}

}

int main(int argc, char** argv)
{
	if (argc < 2)
		return Usage();

	std::optional<Arguments> arguments = Parse(argc, argv);
	if (!arguments)
		return Usage();

	const std::string_view command = argv[1];
	int status = 2;
	if (command == "pack")
		status = Pack(*arguments);
	else if (command == "list")
		status = ListOrVerify(*arguments, false);
	else if (command == "verify")
		status = ListOrVerify(*arguments, true);
	else if (command == "genkey")
		status = GenerateKey(*arguments);
	else
		Usage();
	return status;
}
