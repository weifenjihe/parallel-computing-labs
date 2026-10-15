#!/bin/sh
# Called by Makefile. Fetch from upstream; push only to the fork remote.
set -eu

mode=${1:-all}
case "$mode" in
    all|main|local) ;;
    *) printf 'Unknown sync mode: %s\n' "$mode" >&2; exit 2 ;;
esac

FORK_REMOTE=${FORK_REMOTE:-origin}
UPSTREAM_REMOTE=${UPSTREAM_REMOTE:-upstream}
UPSTREAM_URL=${UPSTREAM_URL:-https://github.com/MAdrid1011/parallel-computing-labs.git}
UPSTREAM_BRANCH=${UPSTREAM_BRANCH:-main}
MAIN_BRANCH=${MAIN_BRANCH:-main}
LOCAL_BRANCH=${LOCAL_BRANCH:-local}

start_branch=$(git symbolic-ref --quiet --short HEAD) || {
    printf '请先切换到一个本地分支，再执行同步。\n' >&2
    exit 1
}
if [ -n "$(git status --porcelain)" ]; then
    printf '工作区有未提交改动，请先提交，或执行 git stash push -u 后再同步。\n' >&2
    exit 1
fi
if git rev-parse --quiet --verify MERGE_HEAD >/dev/null ||
   [ -d "$(git rev-parse --git-path rebase-merge)" ] ||
   [ -d "$(git rev-parse --git-path rebase-apply)" ]; then
    printf '请先完成或取消正在进行的 merge/rebase，再执行同步。\n' >&2
    exit 1
fi
if [ "$FORK_REMOTE" = "$UPSTREAM_REMOTE" ] || [ "$MAIN_BRANCH" = "$LOCAL_BRANCH" ]; then
    printf 'fork 和 upstream 必须使用不同远程名称，main 和 local 必须使用不同分支名称。\n' >&2
    exit 1
fi
git check-ref-format --branch "$MAIN_BRANCH" >/dev/null
git check-ref-format --branch "$LOCAL_BRANCH" >/dev/null
git check-ref-format --branch "$UPSTREAM_BRANCH" >/dev/null
git remote get-url "$FORK_REMOTE" >/dev/null
if ! git remote get-url "$UPSTREAM_REMOTE" >/dev/null 2>&1; then
    git remote add "$UPSTREAM_REMOTE" "$UPSTREAM_URL"
fi

finish() {
    sync_status=$?
    trap - EXIT
    if git rev-parse --quiet --verify MERGE_HEAD >/dev/null; then
        printf '\n合并未完成，停留在当前分支。解决冲突后 git add 并 git commit，再重新运行同步。\n' >&2
    elif [ "$(git symbolic-ref --quiet --short HEAD)" != "$start_branch" ]; then
        git switch --quiet "$start_branch" || sync_status=1
    fi
    exit "$sync_status"
}
trap finish EXIT

git fetch --no-tags "$FORK_REMOTE"
git fetch --no-tags "$UPSTREAM_REMOTE" "$UPSTREAM_BRANCH"
upstream_ref="$UPSTREAM_REMOTE/$UPSTREAM_BRANCH"
git rev-parse --verify "$upstream_ref^{commit}" >/dev/null

sync_main() {
    printf '\n同步 %s：%s → %s/%s\n' "$MAIN_BRANCH" "$upstream_ref" "$FORK_REMOTE" "$MAIN_BRANCH"
    if git show-ref --verify --quiet "refs/heads/$MAIN_BRANCH"; then
        git switch "$MAIN_BRANCH"
    else
        git switch --create "$MAIN_BRANCH" --track "$FORK_REMOTE/$MAIN_BRANCH"
    fi
    # Preserve fork-only commits. A divergence requires manual resolution.
    git merge --ff-only "$FORK_REMOTE/$MAIN_BRANCH"
    git merge --ff-only "$upstream_ref"
    git push --set-upstream "$FORK_REMOTE" "refs/heads/$MAIN_BRANCH:refs/heads/$MAIN_BRANCH"
}

sync_local() {
    printf '\n同步 %s：合并 %s，并推送到 %s/%s\n' "$LOCAL_BRANCH" "$upstream_ref" "$FORK_REMOTE" "$LOCAL_BRANCH"
    if git show-ref --verify --quiet "refs/heads/$LOCAL_BRANCH"; then
        git switch "$LOCAL_BRANCH"
    elif git show-ref --verify --quiet "refs/remotes/$FORK_REMOTE/$LOCAL_BRANCH"; then
        git switch --create "$LOCAL_BRANCH" --track "$FORK_REMOTE/$LOCAL_BRANCH"
    else
        git switch --no-track --create "$LOCAL_BRANCH" "$upstream_ref"
    fi
    if git show-ref --verify --quiet "refs/remotes/$FORK_REMOTE/$LOCAL_BRANCH"; then
        git merge --no-edit "$FORK_REMOTE/$LOCAL_BRANCH"
    fi
    # Merge instead of resetting or rebasing, retaining local work and history.
    git merge --no-edit "$upstream_ref"
    git push --set-upstream "$FORK_REMOTE" "refs/heads/$LOCAL_BRANCH:refs/heads/$LOCAL_BRANCH"
}

case "$mode" in
    all) sync_main; sync_local ;;
    main) sync_main ;;
    local) sync_local ;;
esac
printf '\n同步完成。\n'
