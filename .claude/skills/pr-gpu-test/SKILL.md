---
name: pr-gpu-test
description: GPU-test a DingoEngine pull request on the owner's Windows machine — merge master into the PR branch, build, run the PR's "Needs testing on a real GPU" cases on Vulkan, DX11 and DX12 on hidden desktops, read the logs and the D3D12 debug layer, and draft the results comment. Use when the owner says "GPU test PR #N", "test #N on the GPU", "pr-gpu-test N", "run the GPU checks for #N", or asks to verify a PR on real hardware. Windows only; a Linux or cloud session has no GPU.
model: sonnet
effort: low
---

# PR GPU test

The owner merges every PR after it has run on a real GPU (RX 7900 XTX, VS 2026). Cloud sessions only have llvmpipe, so this run is the PR's only check on Vulkan hardware, D3D11 and D3D12. It must not take over the owner's screen: every launch goes to a private desktop through `gpu-test.ps1`.

If this session isn't on the owner's Windows machine, say so and stop.

## 1. Read the PR

Run `git fetch origin`, then `gh pr view N --json title,headRefName,body,mergeable,commits` and `git diff --name-only origin/master...origin/<head>`.

From **Needs testing on a real GPU**, list:
- the test cases (`--test=…` with their extra flags);
- the examples (`--check` modes, and anything to look at);
- the "look at it" items;
- any **Re-time** or `--perf` line, even one marked optional. Run it (step 4b) without being asked, and never leave it under "Not checked".

If the section says **No**, tell the owner there is nothing to run, and stop.

If the section is missing, work out the list from the changed files with the table in the `pr-writeup` skill.

## 2. Worktree and merge

1. Check `git status -sb` and `git reflog -3`. Another session may be using this worktree; if one is, use a separate worktree. Debug exes read engine shaders from the source tree, so never check out or merge in a worktree whose exes are running.
2. If the worktree is fresh, its submodules are empty. Clone them from the main checkout (`D:/Git/Github/KingoBoiii/DingoEngine`):
   - Run `git -c protocol.file.allow=always -c submodule.<name>.url=<main>/<path> submodule update --init` for each level, including the nested ones (`vendor/nvrhi`, `vendor/msdf-atlas-gen` and its `msdfgen`, and `msdfgen/freetype`).
   - Clone `vendor/tracy` from GitHub.
   - If a submodule's working tree comes out empty, run `git reset --hard HEAD` inside it.
   - Never `git config` the URLs: the config is shared with the main checkout.
3. Run `git checkout -B gpu-test/prN origin/<head>`. Use a local branch of its own, not the session branch, which may hold unpushed work.
4. If the PR is behind master, run `git merge --no-edit -m "Merge branch 'master' into <head>" origin/master`.
   - If a conflict is only in docs or `.claude/`, resolve it.
   - If a conflict is in code, stop and ask the owner.
   - Push the merge after the run passes (step 5). The owner wants the tested commit on the PR.

## 3. Build

1. Run `./vendor/premake/bin/premake5.exe vs2026`. New source files need it.
2. Build with MSBuild (`C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe`, flags `-p:Configuration=Debug -p:Platform=x64 -p:PreferredToolArchitecture=x64 -m -v:m`). Run it as a background command, logging to `build/msb-<project>.log`.
   - Build `test/Dingo-TestFramework.vcxproj` and each `examples/<X>/<X>.vcxproj` you will run.
   - If the PR changes anything under `include/` or a `premake5.lua`, build `DingoEngine.slnx` instead. Nothing else catches a broken example on MSVC before the merge.
3. Every build must exit with 0. Search the logs for `warning C` in the PR's changed files and report any you find. CI's lint never fails on warnings.

## 4. Run

Running is one command. Nothing appears on screen, so you don't need the owner's go-ahead:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .claude/skills/pr-gpu-test/gpu-test.ps1 -Tag prN `
  -Tests "--test=hierarchy@30;--test=anim@20;--test=background;--test=cursor" `
  -Examples "Marionette|--check|75;Candlewick||30|keys:77"
```

- **Tests:** `;`-separated argument sets, each with an optional `@seconds` wait (default 15). The first job on each backend waits at least 30 s: after a build, the shader and font caches beside the exe (`build/bin/.../.cache`) are cold. Collider tests need `@30`, and the Animation Test `@20`. The script refuses a `--test=` name that matches no test in `TestLayer.cpp`, because the app would silently start the first test.
- **Examples:** `Name|args|seconds|action`. Marionette `--check` needs 75 s. Hearthvale `--check` needs 150 s and runs on one backend at a time (`-Apis "vulkan"`, then the next), because every backend writes the same `check-saves/` beside the exe.
- **Actions:**
  - `keys:<vk hex,…>` posts keys 10 s after launch. Useful codes: F3 = `72` and up, one per Debug window tab (see `Application.cpp`), Space = `20`. Extended keys (arrows, Home, End, Insert, Delete) arrive as numpad keys.
  - `min` minimizes the window at 8 s and closes it while it is minimized. It takes no PNG.
- **Backends:** Vulkan with validation, DX11 and DX12 with the debug layer read through OutputDebugString. Use `-Apis "vulkan"` to narrow them. The D3D12 listener is one per machine, so run only one DX12 job at a time on the machine (another session's run, or DebugView, would take its messages).
- **How jobs run:** the backends run in parallel, and each backend runs its jobs one after another. Each test app process gets its own `%TEMP%\<name>-<pid>` folders, so the backends don't share files. Pack Test is the exception: its `pack-test` folder sits in the shared cache, so run `--test=pack` on one backend at a time.
- **Output:** `build/gpu-test/<Tag>/out/<api>/`. Each job leaves its `Dingo.log` copy, stdout, stderr, a PNG of the window just before it closes, and its D3D12 messages. Re-running with the same tag replaces the output, so retry under a new tag (`prN-retry`) to keep the first run's logs. `-ReportOnly` prints the report again.
- **Bigger Debug window:** `build/gpu-test/<Tag>/<api>/<project>/imgui.ini` is copied once per tag and then kept, so you can raise `Size=` under `[Window][Debug]` there before a re-run to fit more of a debug tab in the PNG.
- Never stop processes by name: another session's runs share the machine. A job that ignores `WM_CLOSE` is killed after 15 s on its own.
- Expect about 2–5 min for a typical PR. A build adds 2–5 min when it's incremental, more from cold.

## 4b. Re-time (when the PR body asks for `--perf`)

Do this after the pass/fail run is clean. Time **Release** on Vulkan, master against the PR. A Debug exe says nothing about a CPU-cost change. If the PR is stacked on an unmerged PR, the baseline is that PR's head instead of master.

1. Build the PR as Release: `MSBuild test/Dingo-TestFramework.vcxproj -p:Configuration=Release …` (same flags as step 3), then `cp -r build/bin/Release-windows-x86_64 build/bin/ReleaseNew-windows-x86_64`.
2. Put the baseline's version of the PR's changed source files in the worktree: `git checkout origin/master -- <the files from git diff --name-only origin/master...HEAD>`. Build Release again (incremental), then `cp -r build/bin/Release-windows-x86_64 build/bin/ReleaseBase-windows-x86_64`. Restore with `git checkout HEAD -- <the same files>`, and check `git status -sb` is clean. The base and new `Dingo-TestFramework.exe` must have different `md5sum`s.
3. Run 3 interleaved pairs, one tag per run, with the `-Config` naming the copied folder (the `@45` wait lets the PERF line print):
   ```
   for i in 1 2 3; do for v in Base New; do
     powershell -NoProfile -ExecutionPolicy Bypass -File .claude/skills/pr-gpu-test/gpu-test.ps1 -Tag perf$v$i -Config Release$v -Apis vulkan \
       -Tests "--test=throughput --throughput=mixed --perf@45;--test=throughput --throughput=repeat --perf@45"
   done; done
   ```
   Use the modes and flags the PR body names. About 1.5 min per run, so run it in the background.
4. Read the `[PERF]` line in each job's log (`build/gpu-test/perf<v><i>/out/vulkan/*.log`). Report the metric the PR names (e.g. `render_entities_ms`) per run and as an average, plus `end_scene_ms` or whatever else shouldn't have moved.
5. A `--perf` job can show a 1-px `[FAIL]` that a plain run doesn't (it happens on master too). Note which runs and which build, and don't treat it as the PR's fault unless it repeats on the PR only.

Compare with the PR's own claim (e.g. its instruction-count cut) and with the issue's targets, and say whether they are met.

## 5. Read the results

The report ends with `RESULT: all clean` or a count of items to look at:
- A row marked `!!` means a non-zero exit, a `[FAIL]`, a `VUID-`, a sync hazard, a window that didn't close, or a runner that stopped before its last job.
- `UNEXPECTED` marks an `[error]` log line or a D3D12/DXGI message that isn't in the script's known lists. `KNOWN-ERROR` marks a known D3D12 error, which still counts.

Then:
- **Pass counts:** compare them with the PR body's Linux counts. Windows can show more when master has added checks since the PR was written.
- **Keys jobs:** Read the job's PNG and describe what it shows.
- **Triage:**
  - `KILLED(no exit)` or `NO-WINDOW(killed)`: either shaders were still compiling on a cold cache, or a Debug assert dialog is open on the hidden desktop (EnTT asserts print to `<job>.stderr`). Retry once under a new tag. If it happens again, it's a hang: report it.
  - `EXITED-EARLY` or `EXITED-NO-WINDOW`: a crash, or a mode that closes itself. Read `<job>.stderr` and the log.
  - `fail=no-log`: the job's `Dingo.log` wasn't where the runner looked (the working directory, or the job's `--log-dir=`). Jobs without `--log-dir=` get `--log-here`, so check the exe honours it and read `<job>.stdout`. Give `--log-dir=` a relative path: the backends run in parallel with the same args, and an absolute one makes them share a log.
  - A real failure goes to the owner with the log lines. Don't post anything yet.
- **New noise:** when an error or warning turns out to be noise that master already has, file an issue for it. Add it to `$knownErrors` or `$knownD3D12` in `gpu-test.ps1`, with the issue number, in that issue's fix PR or a skill update.

When the run is clean, push the merge: `git push origin HEAD:<head>`. Check `git reflog -3` first.

## 6. What the run can't check

- Posted mouse messages never reach ImGui, because its GLFW backend reads the real cursor. So clicking widgets, tree nodes, filters and drags needs the owner.
- Gamepads, sound, and anything on a second monitor also need the owner.

Before anything goes under "Not checked", try to run it yourself: a different `--layout`, `--time`, camera or seed that puts the item in frame, a `keys:` action, extra `--check` flags, or a Release `--perf` run. Re-run under a new tag (`prN-look`) and Read the new PNG. Only what truly needs a person (the list above, or something you tried and could not get on screen) goes to the owner, and the comment says what you tried.

Give the owner the exact steps. When they ask, launch the example on their screen with `Start-Process <exe> -WorkingDirectory examples/<X>`. That takes focus, so only do it when asked.

A visible run rewrites the tracked `imgui.ini` (`test/`, `examples/Breakout3D`, `examples/DungeonCrawler3D`). Afterwards, run `git checkout --` on it.

## 7. Report and comment

In chat, give one table (case × backend with pass counts), the warnings that were known noise, and what's left for the owner.

Write the comment to the scratchpad. Post it only when the owner says so: `gh pr comment N --body-file <file>`. To change it later, run `gh api -X PATCH repos/KingoBoiii/DingoEngine/issues/comments/<id> -F body=@<file>`.

```markdown
## GPU verification (owner)

Windows 11, RX 7900 XTX, VS 2026 (MSVC), **Debug**. Tested at `<sha>` (<this PR, or this PR plus master `<sha>`, merged and pushed>). <MSVC warnings in the PR's files: none, or which.>

| Case | Vulkan (validation) | DX11 | DX12 (debug layer) |
|---|---|---|---|
| `--test=…` | N PASS | N PASS | N PASS |

- <New checks by name, if the PR added any, and that they passed on every backend.>
- No `VUID-`, no sync hazards, no D3D12 debug-layer errors. Known warnings only: <list with issue numbers>.
- <Expected `[error]` lines and why they are expected.>

**<Look-at-it item>:** <what each backend showed, from the PNGs.>

**Re-time (Release, Vulkan, `--perf`):** <master (or the stacked base) vs this PR at `<sha>`, same flags, 3 runs each, interleaved.>

| Mode | master | this PR | Change |
|---|---|---|---|
| <mode> | <a / b / c (avg x)> | <a / b / c (avg x)> | <±N%> |

<What didn't move, how it compares with the PR's claim and the issue's targets, and any 1-px `--perf` flakes with the build they hit.> <Omit this block when the PR has no re-time.>

**Not checked:** <only what needs a person's eyes or input, with what you already tried and the exact steps for them. "Nothing else outstanding" if all of the PR's list ran.>
```

Never claim a result you didn't see: no Release, other configs or other examples unless they ran. A re-time listed in the PR is not a "not checked" item: run it. Nor is anything else you can run or get on screen yourself: "Not checked" is only for what needs a person. When the owner checks the "not checked" items by hand, update the comment to say so.

When the owner later says **done**, the CLAUDE.md keyword applies. For a GPU-test session, cleaning up means deleting the local `gpu-test/prN` branch. Leave the PR branch's deletion to the merge.
