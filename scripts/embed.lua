-- Writes a file's bytes out as a C++ array, so engine shaders compile into DingoEngine.lib and a
-- game ships no engine files. Runs on premake itself rather than adding a build tool:
--   premake5 --file=scripts/embed.lua --input=<file> --output=<file.inl> embed
-- A byte array rather than a raw string literal: MSVC caps a single literal at ~16 KB.

newoption {
	trigger = "input",
	value = "PATH",
	description = "File to embed"
}

newoption {
	trigger = "output",
	value = "PATH",
	description = "Generated .inl to write"
}

newaction {
	trigger = "embed",
	description = "Embed a file into a C++ byte array",
	execute = function()
		local input = _OPTIONS["input"]
		local output = _OPTIONS["output"]
		if not input or not output then
			error("embed needs --input and --output", 0)
		end

		local source = io.open(input, "rb")
		if not source then
			error("embed: cannot read " .. input, 0)
		end
		local bytes = source:read("*a")
		source:close()

		local symbol = "k_" .. path.getname(input):gsub("[^%w]", "_")

		local lines = {}
		for i = 1, #bytes, 16 do
			local row = {}
			for j = i, math.min(i + 15, #bytes) do
				row[#row + 1] = string.format("0x%02X", bytes:byte(j))
			end
			lines[#lines + 1] = "\t" .. table.concat(row, ", ") .. ","
		end

		os.mkdir(path.getdirectory(output))
		local generated = io.open(output, "wb")
		if not generated then
			error("embed: cannot write " .. output, 0)
		end
		generated:write("// Generated from " .. path.getname(input) .. " by scripts/embed.lua - do not edit.\n")
		generated:write("inline constexpr unsigned char " .. symbol .. "[] = {\n")
		generated:write(table.concat(lines, "\n"))
		generated:write("\n};\n")
		generated:close()
	end
}
