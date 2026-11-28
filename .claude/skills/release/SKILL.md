---
name: release
description: "Cut a DingoEngine vX.Y.Z release end to end — the VERSION bump plus release notes in ROADMAP, README and project history pushed straight to master, the `build.yml` gate (Windows + Linux) on that commit, the lightweight tag, the GitHub release with its notes, the build-release workflow and its asset count, and the \"docs: mark released\" commit. Use when the owner says \"make a release\", \"release vX.Y.Z\", \"cut v0.9.1\", \"tag it\", \"make the gh release\", \"tag the RC\", \"cut v1.0.0-rc.1\", or asks what a release still needs. Also covers release candidates (vX.Y.Z-rc.N), which skip the bump."
---

# Release

A release is a version bump and notes committed to `master`, then a tag, a GitHub release on that commit, and the `build-release.yml` run that uploads its assets. A release without its assets is not done. The owner triggers it; each step below needs only that one ask, except where it says to stop.

## 1. Scope

- `git fetch --tags origin`. The last tag is `git tag --sort=-creatordate | head -1`; for a final release after an RC, the last non-RC tag (`| grep -v -- '-rc\.' | head -1`), so the notes cover the RC's changes too.
- `git log --oneline --first-parent <last-tag>..origin/master` lists the merged PRs. For each: `gh pr view N --json number,title,closingIssuesReferences`, then each closed issue's title.
- An issue closed by a PR body that only *mentions* it (GitHub reads "closes #N" anywhere) is not shipped: leave it out of the notes and don't reopen it unless the owner says so.
- Version: a point release (`X.Y.Z+1`) gathers fixes and features merged since the last tag; a minor (`X.Y+1.0`) closes a milestone plan. The owner names it; if not, ask.

## 2. Bump and notes on master

No release branch or PR: a release touches only `VERSION` and markdown, and the owner has decided it goes straight to `master`.

1. `git fetch origin && git switch -c vX.Y.Z origin/master` (a local working branch, never pushed under that name; keep any untracked files the owner has).
2. `VERSION` holds the full version (`0.9.1`), LF ending. Commit: `chore(release): bump version to X.Y.Z`.
3. Release notes, one commit `docs(release): add the vX.Y.Z notes`:
   - `ROADMAP.md`: a `## vX.Y.Z — <Title>` section in version order (a point release sits after its minor's section). Group by area, cite PRs and issues as full GitHub links, end with a **Test** line that claims only what was run.
   - `README.md`: the "Currently at" paragraph, the point-release list after it, and a table row.
   - `.claude/reference/project-history.md`: one bullet for the version, before "**Then**".
   - Check every claim against the code or the workflows (configs CI builds, what each PR verified). Never write "verified on Vulkan/DX11/DX12" unless the PRs record it.
4. Stop if anything beyond `VERSION` and markdown changed. Push with `DINGO_RELEASE_PUSH=1 git push origin HEAD:master` (the `guard-git` hook blocks any other push to `master`); if master moved, merge it in (never rebase) and push again. Record the pushed sha.

## 3. Gate

1. The release commit is the sha pushed in step 2.4. If code merged on top of it since, stop and ask which sha to release.
2. Master pushes that touch only `VERSION` and markdown start no build (`paths-ignore`), so the release commit has no run. Start it: `gh workflow run build.yml --ref master` (it calls Build Windows and Build Linux). `--ref` takes a branch or tag, never a sha, so it builds master's tip at dispatch: confirm the tip is the release sha first (`git fetch origin && git rev-parse origin/master`; if master moved, stop and ask). Then `gh run list --workflow build.yml --branch master --limit 3 --json databaseId,headSha,status` and pick the run whose `headSha` is the release sha. Wait with `gh run watch <id> --exit-status` in a background command. It must pass on that sha before tagging; tag only that sha. If it fails, stop and report the failing job and step.

## 4. Tag and GitHub release

1. Lightweight tag on the release commit: `git tag vX.Y.Z <sha> && git push origin refs/tags/vX.Y.Z`, then confirm with `git ls-remote --tags origin vX.Y.Z`. The full ref matters: while the local `vX.Y.Z` branch exists, `git push origin vX.Y.Z` matches both and pushes nothing, and `gh release create` then makes an untagged draft.
2. Notes file in the scratchpad, in the style of the previous release (`gh release view <last-tag> --json body --jq .body`): `## vX.Y.Z — <Title>`, one intro paragraph, `###` sections by area, a **Docs** list and a **Full changelog** compare link. Links are absolute `https://github.com/KingoBoiii/DingoEngine/blob/vX.Y.Z/...`: relative ones 404 on the release page.
   Every release's notes, RCs too, include a **For games** section (Headstone reads release notes for an engine bump, with `gh release view vX.Y.Z`, instead of this repo): breaking API changes with their `docs/` page, behaviour changes a game would notice, and the `headstone` issues the release closes.
3. `gh release create vX.Y.Z --verify-tag --title vX.Y.Z --notes-file <file>`, then check `gh release view vX.Y.Z --json isDraft` is `false`.

## 5. Assets

- Always run `build-release.yml` yourself, straight after step 4, without asking: it is part of every release. It is `workflow_dispatch` only and uploads to an existing release, so it can't start earlier: `gh workflow run build-release.yml --ref vX.Y.Z`, then `gh run watch <id> --exit-status` in a background command. If a job fails, report it and re-run only the failed jobs (`gh run rerun <id> --failed`).
- Expect **32 assets** (12 examples today): Windows 16 (DingoEngine Debug/Release/Distribution zips, the Distribution symbols zip + 12 example zips) and Linux 16 (3 engine `.tar.gz` + 12 examples + Dingo-TestFramework). Recompute from the workflow: Windows = 4 + the `example-game-*` jobs in `build-release.yml` (`grep -c '^  example-game-' .github/workflows/build-release.yml`); Linux = 3 + the directories in `examples/` (`scripts/package-linux-programs.sh` packs each) + 1. Check with `gh release view vX.Y.Z --json assets --jq '.assets | length'`.

## 6. Mark released

Commit straight to `master` and push it with `DINGO_RELEASE_PUSH=1 git push origin master` (the `guard-git` hook blocks any other push to `master`): `docs: mark vX.Y.Z released`. It updates the ROADMAP section with a `**Status**: released on <date> (<sha>)` line and the project-history bullet with the same, plus anything else that still says "planned" or "in progress" for this version.

## 7. Clean up

Delete the local working branch (`git branch -D vX.Y.Z`) and switch the owner's checkout back to `master`. Reply with the release URL, the tag's sha and the asset count. Don't call the release done before the asset count matches.

## Release candidate (`vX.Y.Z-rc.N`)

- No `VERSION` bump or release commit (step 2 doesn't apply).
- Tag the master commit to release from (for rc.1, the merge that finishes the milestone), after `build.yml`, started by hand, passes on that sha (step 3.2). Lightweight tag, full-ref push as in step 4.1.
- Notes as in step 4.2, with `vX.Y.Z-rc.N` in the heading, the blob links and the compare link (the final tag doesn't exist yet). `gh release create vX.Y.Z-rc.N --verify-tag --prerelease --title vX.Y.Z-rc.N --notes-file <file>`, then check `gh release view vX.Y.Z-rc.N --json isPrerelease,isDraft` gives `true`/`false`; it must not show as Latest.
- Step 5 as usual: `gh workflow run build-release.yml --ref vX.Y.Z-rc.N`, expect the same count as step 5 (**32 assets** today). The workflow accepts only tags matching `^v\d+\.\d+\.\d+(-rc\.\d+)?$`.
- Skip step 6 and step 7's branch deletion. Add an "RC tagged" line to the milestone plan (commit it straight to `master` like step 6), then reply as step 7 does: release URL, tag sha, asset count.
