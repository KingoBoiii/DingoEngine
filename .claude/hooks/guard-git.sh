#!/usr/bin/env bash
# Pure bash and sed, so it also runs under Git Bash on the owner's Windows machine.
# The release skill's direct-to-master pushes are prefixed with DINGO_RELEASE_PUSH=1.

input=$(cat)
cmd=$(printf '%s' "$input" | tr '\n' ' ' | sed -n 's/.*"command"[[:space:]]*:[[:space:]]*"\(\([^"\\]\|\\.\)*\)".*/\1/p')
[ -z "$cmd" ] && exit 0
cmd=${cmd//\\n/;}
cmd=${cmd//\\t/ }
cmd=${cmd//\\\"/\"}
case "$cmd" in *git*) ;; *) exit 0 ;; esac

block() { echo "guard-git: $1" >&2; exit 2; }

current_branch() { git symbolic-ref --quiet --short HEAD 2>/dev/null; }

segments=$(printf '%s' "$cmd" | sed 's/&&/;/g; s/||/;/g; s/|/;/g')
IFS=';' read -ra parts <<< "$segments"
for part in "${parts[@]}"; do
	read -ra w <<< "$part"
	release=0
	i=0
	while [ $i -lt ${#w[@]} ] && [ "${w[$i]}" != git ]; do
		[ "${w[$i]}" = DINGO_RELEASE_PUSH=1 ] && release=1
		i=$((i + 1))
	done
	[ $i -ge ${#w[@]} ] && continue
	i=$((i + 1))
	while [ $i -lt ${#w[@]} ]; do
		case "${w[$i]}" in
			-C|-c|--git-dir|--work-tree) i=$((i + 2)) ;;
			-*) i=$((i + 1)) ;;
			*) break ;;
		esac
	done
	sub=${w[$i]}
	args=("${w[@]:$((i + 1))}")

	if [ "$sub" = push ]; then
		force=0 lease=0 positional=()
		for a in "${args[@]}"; do
			case "$a" in
				--force-with-lease*|--force-if-includes) lease=1 ;;
				--force|-f) force=1 ;;
				--delete|-d) ;;
				-*f*) [[ "$a" != --* ]] && force=1 ;;
				-*) ;;
				+*) force=1; positional+=("${a#+}") ;;
				*) positional+=("$a") ;;
			esac
		done
		refs=("${positional[@]:1}")
		[ ${#refs[@]} -eq 0 ] && refs=("$(current_branch)")
		for ref in "${refs[@]}"; do
			dst=${ref##*:}
			dst=${dst#refs/heads/}
			case "$dst" in
				master|main)
					[ $release -eq 1 ] || block "push to $dst is the owner's (merge through a PR; the release skill prefixes DINGO_RELEASE_PUSH=1)" ;;
			esac
			[ $force -eq 1 ] && block "force push to $dst; never rewrite a pushed branch (use a merge commit)"
			if [ $lease -eq 1 ]; then
				case "$dst" in
					claude/*|v*) ;;
					*) block "--force-with-lease is allowed only on claude/* and v* branches, not $dst" ;;
				esac
			fi
		done
	elif [ "$sub" = rebase ]; then
		positional=() resume=0
		for a in "${args[@]}"; do
			case "$a" in
				--abort|--continue|--skip|--quit|--edit-todo|--show-current-patch) resume=1 ;;
				-*) ;;
				*) positional+=("$a") ;;
			esac
		done
		[ $resume -eq 1 ] && continue
		branch=${positional[1]:-$(current_branch)}
		[ -n "$branch" ] && git show-ref --verify --quiet "refs/remotes/origin/$branch" &&
			block "rebase of $branch, which exists on origin; merge the base branch in instead"
	fi
done
exit 0
