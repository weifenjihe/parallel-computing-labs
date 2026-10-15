# Run from the repository root. main follows upstream/main; local keeps its
# own commits and merges upstream/main. Existing remote URLs are preserved.
.DEFAULT_GOAL := help
.NOTPARALLEL:

FORK_REMOTE ?= origin
UPSTREAM_REMOTE ?= upstream
UPSTREAM_URL ?= https://github.com/MAdrid1011/parallel-computing-labs.git
UPSTREAM_BRANCH ?= main
MAIN_BRANCH ?= main
LOCAL_BRANCH ?= local
export FORK_REMOTE UPSTREAM_REMOTE UPSTREAM_URL UPSTREAM_BRANCH MAIN_BRANCH LOCAL_BRANCH

.PHONY: help sync sync-main sync-local status

help:
	@printf '%s\n' \
	  'make sync        同步源仓库 main 到 fork 的 main 和 local，并推送' \
	  'make sync-main   仅同步并推送 main（只允许快进）' \
	  'make sync-local  仅将源仓库 main 合并到 local，并推送' \
	  'make status      查看当前分支、未提交改动和远程仓库' \
	  '' \
	  '同步前请先提交或 stash 工作区改动；成功后返回原分支。' \
	  'local 保留已有提交；遇到冲突会停止，等待手动解决。' \
	  '示例：make sync FORK_REMOTE=origin UPSTREAM_REMOTE=upstream'

sync sync-main sync-local:
	@sh scripts/sync-fork.sh $(patsubst sync-%,%,$(patsubst sync,all,$@))

status:
	@git status --short --branch
	@git remote -v
