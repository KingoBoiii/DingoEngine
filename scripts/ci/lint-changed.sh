#!/usr/bin/env bash
# Syntax-only compile, with extra warnings, of the C/C++ files changed since <base> and of every file
# that includes a changed header, on this OS's compiler (GCC through make, MSVC through MSBuild).
# Run premake with --lint first (gmake on Linux, vs2026 on Windows).
#
#   scripts/ci/lint-changed.sh [base]
#
# base defaults to HEAD^1, the base side of the merge commit a pull_request run checks out.
# Locally: scripts/ci/lint-changed.sh origin/master
# Warnings are reported but never fail the run; a file that doesn't compile does.
# Environment: PREMAKE (premake binary), MSBUILD (MSBuild.exe), LINT_MSBUILD_ARGS (extra MSBuild args).
set -uo pipefail

base=${1:-HEAD^1}
root=$(git rev-parse --show-toplevel)
cd "$root" || exit 1

case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) os=windows; compiler=MSVC; root_abs=$(cygpath -m "$root") ;;
    *) os=linux; compiler=GCC; root_abs=$root ;;
esac
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'

out=build/lint
mkdir -p "$out"
log=$out/lint-$os.log
diags=$out/diagnostics-$os.tsv
: > "$log"
: > "$diags"

is_source() { [[ $1 == *.c || $1 == *.cpp ]]; }

git rev-parse --verify -q "$base^{commit}" > /dev/null || { echo "Unknown base commit: $base"; exit 1; }
mapfile -t changed < <(git -c core.quotepath=false diff --no-renames --name-only "$base"...HEAD -- include src test examples tools \
    | grep -E '\.(c|cpp|h|hpp|inl)$')

declare -A selected=() seen=() is_changed=()
queue=()
for f in "${changed[@]}"; do
    is_changed[$f]=1
    if is_source "$f"; then
        [[ -f $f ]] && selected[$f]=1
    else
        queue+=("$f")
    fi
done

# Includers are matched by file name, so a name shared by two headers only checks more files.
while ((${#queue[@]})); do
    name=$(basename "${queue[0]}")
    queue=("${queue[@]:1}")
    [[ -n ${seen[$name]:-} ]] && continue
    seen[$name]=1
    pattern="#[[:space:]]*include[[:space:]]*[\"<]([^\">]*/)?${name//./\\.}[\">]"
    while IFS= read -r f; do
        if is_source "$f"; then selected[$f]=1; else queue+=("$f"); fi
    done < <(git grep -lE "$pattern" -- include src test examples tools | grep -E '\.(c|cpp|h|hpp|inl)$')
done

if ((${#selected[@]} == 0)); then
    echo "No C/C++ files to check."
    [[ -n ${GITHUB_STEP_SUMMARY:-} ]] && echo "### Lint ($os, $compiler): no C/C++ files to check" >> "$GITHUB_STEP_SUMMARY"
    exit 0
fi

if [[ -z ${PREMAKE:-} ]]; then
    if [[ $os == windows ]]; then PREMAKE=vendor/premake/bin/premake5.exe
    elif command -v premake5 > /dev/null; then PREMAKE=premake5
    else PREMAKE=vendor/premake/bin/premake5
    fi
fi

# The engine includes its shaders as generated .inl files, which a full build makes first.
for f in "${!selected[@]}"; do
    if [[ $f == src/* ]]; then
        embedded=build/bin-int/Debug-$os-x86_64/DingoEngine/Embedded
        while IFS= read -r shader; do
            "$PREMAKE" --file=scripts/embed.lua --input="$shader" --output="$embedded/$(basename "$shader").inl" embed > /dev/null \
                || { echo "Embedding $shader failed"; exit 1; }
        done < <(find src -path '*/Shaders/*.glsl')
        break
    fi
done

# Diagnostics from one project's compiler output as file<TAB>line<TAB>col<TAB>severity<TAB>message,
# with repo-relative paths. Anything outside the repo, in vendor/ or in build/ is dropped.
parse_diagnostics() {
    local project_dir=$1
    awk -v os="$os" -v root="$root_abs" -v dir="$root_abs/$project_dir" '
        function normalize(p,   n, parts, stack, i, k, joined) {
            gsub(/\\/, "/", p)
            if (p !~ /^([A-Za-z]:)?\//) p = dir "/" p
            n = split(p, parts, "/")
            k = 0
            for (i = 1; i <= n; i++) {
                if (parts[i] == "." || (parts[i] == "" && i > 1)) continue
                if (parts[i] == ".." && k > 1) { k--; continue }
                stack[++k] = parts[i]
            }
            joined = stack[1]
            for (i = 2; i <= k; i++) joined = joined "/" stack[i]
            prefix = root "/"
            if (os == "windows") { if (tolower(substr(joined, 1, length(prefix))) != tolower(prefix)) return "" }
            else if (substr(joined, 1, length(prefix)) != prefix) return ""
            joined = substr(joined, length(prefix) + 1)
            if (joined ~ /^(vendor|build)\//) return ""
            return joined
        }
        {
            sub(/\r$/, "")
            file = ""
            if (os == "linux" && match($0, /^[^ :][^:]*:[0-9]+:[0-9]+: (warning|error|fatal error): /)) {
                head = substr($0, 1, RLENGTH); msg = substr($0, RLENGTH + 1)
                n = split(head, f, ":")
                file = f[1]; line = f[2]; col = f[3]; sev = f[4]
            } else if (os == "windows" && match($0, /^ *[^ ].*\([0-9]+(,[0-9]+)?\): (warning|error|fatal error) [A-Z]+[0-9]+: /)) {
                head = substr($0, 1, RLENGTH); msg = substr($0, RLENGTH + 1)
                sub(/^ +/, "", head)
                paren = match(head, /\([0-9]+(,[0-9]+)?\): /)
                file = substr(head, 1, paren - 1)
                loc = substr(head, paren + 1, RLENGTH - 4)
                split(loc, lc, ","); line = lc[1]; col = (lc[2] == "" ? 1 : lc[2])
                rest = substr(head, paren + RLENGTH)
                sev = rest; sub(/ [A-Z]+[0-9]+: $/, "", sev)
                code = rest; sub(/^[a-z ]+ /, "", code); sub(/: $/, "", code)
                msg = code ": " msg
                sub(/ \[[^]]*\.vcxproj\]$/, "", msg)
            }
            if (file == "") next
            gsub(/^ +/, "", sev)
            sev = (sev == "warning" ? "warning" : "error")
            file = normalize(file)
            if (file == "") next
            print file "\t" line "\t" col "\t" sev "\t" msg
        }'
}

failed_projects=()
checked=0
declare -A handled=()
project_count=0

if [[ $os == linux ]]; then
    [[ -f Makefile ]] || { echo "No Makefile: run premake5 --lint gmake first."; exit 1; }
    mapfile -t projects < <(sed -n 's/.*-C \([^ ]*\) -f \([^ ]*\) config=.*/\1 \2/p' Makefile | grep -v '^build/' | sort -u)
    for entry in "${projects[@]}"; do
        dir=${entry% *}; makefile=${entry#* }
        objdir=$(sed -n 's/^OBJDIR = //p' "$dir/$makefile" | head -1)
        targets=()
        for f in "${!selected[@]}"; do
            [[ -n ${handled[$f]:-} ]] && continue
            if [[ $dir == . ]]; then rel=$f
            elif [[ $f == "$dir"/* ]]; then rel=${f#"$dir"/}
            else continue
            fi
            obj=$(awk -v src="$rel" '$2 == src && $1 ~ /^\$\(OBJDIR\)\// { sub(/^\$\(OBJDIR\)\//, "", $1); sub(/:$/, "", $1); print $1; exit }' "$dir/$makefile")
            [[ -n $obj ]] || continue
            targets+=("$objdir/$obj")
            handled[$f]=1
        done
        ((${#targets[@]})) || continue
        project_count=$((project_count + 1))
        checked=$((checked + ${#targets[@]}))
        echo "::group::$dir ($(( ${#targets[@]} )) files)"
        mkdir -p "$dir/$objdir"
        make --no-print-directory -C "$dir" -f "$makefile" config=debug -k -j"$(nproc)" "${targets[@]}" 2>&1 | tee "$out/project.log"
        status=${PIPESTATUS[0]}
        echo "::endgroup::"
        cat "$out/project.log" >> "$log"
        parse_diagnostics "$dir" < "$out/project.log" >> "$diags"
        if ((status != 0)); then
            if [[ $dir == . ]]; then failed_projects+=("$makefile"); else failed_projects+=("$dir/$makefile"); fi
        fi
    done
else
    if [[ -z ${MSBUILD:-} ]]; then
        vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
        MSBUILD=$("$vswhere" -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | head -1 | tr -d '\r')
    fi
    [[ -n $MSBUILD ]] || { echo "MSBuild not found: set MSBUILD."; exit 1; }
    mapfile -t projects < <(find . -name '*.vcxproj' -not -path './vendor/*' -not -path './build/*' | sed 's|^\./||' | sort)
    ((${#projects[@]})) || { echo "No .vcxproj files: run premake5 --lint vs2026 first."; exit 1; }
    for project in "${projects[@]}"; do
        dir=$(dirname "$project")
        files=()
        for f in "${!selected[@]}"; do
            [[ -n ${handled[$f]:-} ]] && continue
            if [[ $dir == . ]]; then rel=$f
            elif [[ $f == "$dir"/* ]]; then rel=${f#"$dir"/}
            else continue
            fi
            rel=${rel//\//\\}
            grep -qF "<ClCompile Include=\"$rel\"" "$project" || continue
            files+=("$rel")
            handled[$f]=1
        done
        ((${#files[@]})) || continue
        project_count=$((project_count + 1))
        checked=$((checked + ${#files[@]}))
        # A response file, because no command-line quoting gets a ';' list through to MSBuild intact.
        # A selection that matches nothing compiles the whole project.
        echo "-p:SelectedFiles=\"$(IFS=';'; echo "${files[*]}")\"" > "$out/msbuild.rsp"
        echo "::group::$project (${#files[@]} files)"
        # shellcheck disable=SC2086
        "$MSBUILD" "$project" -nologo -v:m -clp:NoSummary -t:ClCompile -p:Configuration=Debug -p:Platform=x64 \
            "@$out/msbuild.rsp" ${LINT_MSBUILD_ARGS:-} 2>&1 | tee "$out/project.log"
        status=${PIPESTATUS[0]}
        echo "::endgroup::"
        cat "$out/project.log" >> "$log"
        parse_diagnostics "$dir" < "$out/project.log" >> "$diags"
        ((status == 0)) || failed_projects+=("$project")
    done
fi
rm -f "$out/project.log" "$out/msbuild.rsp"

skipped=()
for f in "${!selected[@]}"; do
    [[ -n ${handled[$f]:-} ]] || skipped+=("$f")
done

sort -u -o "$diags" "$diags"
warnings=$(awk -F'\t' '$4 == "warning"' "$diags" | wc -l)
errors=$(awk -F'\t' '$4 == "error"' "$diags" | wc -l)

changed_list=$(printf '%s\n' "${!is_changed[@]}")
awk -F'\t' -v os="$os" -v compiler="$compiler" '
    function esc(s) { gsub(/%/, "%25", s); gsub(/\r/, "%0D", s); gsub(/\n/, "%0A", s); return s }
    { printf "::%s file=%s,line=%s,col=%s,title=%s (%s)::%s\n", $4, $1, $2, $3, compiler, os, esc($5) }' "$diags"

for dir in "${failed_projects[@]}"; do
    echo "::error title=$compiler ($os)::Compiling the changed files in $dir failed: see the log above."
done

echo "Lint ($os, $compiler): $checked files in $project_count projects, $errors errors, $warnings warnings."
((${#skipped[@]})) && echo "Not in any $os project: ${skipped[*]}"

if [[ -n ${GITHUB_STEP_SUMMARY:-} ]]; then
    {
        echo "### Lint ($os, $compiler)"
        echo
        echo "$checked files in $project_count projects: **$errors errors, $warnings warnings** (warnings don't fail the check)."
        ((${#failed_projects[@]})) && echo && echo "**Failed to compile:** ${failed_projects[*]}"
        ((${#skipped[@]})) && echo && echo "Not part of a $os project, so not checked: \`${skipped[*]}\`"
        if [[ -s $diags ]]; then
            echo
            echo "| | File | Message |"
            echo "|---|---|---|"
            printf '%s\n' "$changed_list" | awk -F'\t' '
                NR == FNR { changed[$0] = 1; next }
                {
                    msg = $5; gsub(/\|/, "\\|", msg)
                    row = sprintf("| %s | `%s:%s` | %s |", ($4 == "error" ? "error" : "warning"), $1, $2, msg)
                    if ($1 in changed) mine[++m] = row; else other[++o] = row
                }
                END {
                    shown = 0
                    for (i = 1; i <= m && shown < 150; i++) { print mine[i]; shown++ }
                    for (i = 1; i <= o && shown < 150; i++) { print other[i]; shown++ }
                    if (m + o > shown) printf "\n%d more in the uploaded log.\n", m + o - shown
                }' - "$diags"
        fi
    } >> "$GITHUB_STEP_SUMMARY"
fi

((${#failed_projects[@]} == 0))
