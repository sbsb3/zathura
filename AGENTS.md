# AGENTS.md

This is a personal fork of zathura (`origin` = github.com/sbsb3/zathura). There is **no upstream PR
workflow**: features live on long-lived fork branches and are kept current by merging `develop` into
them. Never open PRs against pwmt/zathura.

## Branches

- `develop`: mirror of upstream `develop`, synced via GitHub's "Sync fork". Never commit to it directly;
  only fast-forward it (`git merge --ff-only origin/develop`).
- `feature/smart-width`, `feature/persistent-highlights`: fork-only features. The latter currently
  contains the former and is the branch that gets installed.
- `backup/*`: local-only safety branches made before risky merges. Safe to delete once verified.

## Keeping features current (the sync workflow)

1. `git fetch --all --prune`, then check divergence:
   `git rev-list --left-right --count origin/develop...HEAD`
   Compare against `origin/develop`, not local `develop`, which is often stale.
2. Safety net: `git branch backup/<feature>`.
3. Fast-forward local `develop`: `git switch develop && git merge --ff-only origin/develop`.
4. Preview conflicts without touching the tree:
   `git merge-tree --write-tree --name-only develop <feature>`
5. On the feature branch run `git merge develop`. **Merge, don't rebase**: the branch is pushed, and a
   merge means each conflict is resolved once. `rerere` and `merge.conflictstyle=zdiff3` are enabled
   locally.
6. Resolve conflicts, then **build and test** (below). Clean auto-merges can still break compilation
   or behavior. Example: upstream removed `zathura->pages` and `shortcuts.c` merged cleanly but no
   longer compiled.
7. Commit the merge and `git push origin <feature>` (a fast-forward; never force-push).

## Build, test, install

- Scratch build: `meson setup build-merge && ninja -C build-merge && ninja -C build-merge test`.
  Delete `build-merge` afterwards; don't `git add` build dirs.
- The install tree is `build/` (prefix `/usr`; the existing `/usr/bin/zathura` is a source install,
  not pacman-owned). Run `ninja -C build`; the user runs `sudo ninja -C build install` themselves.
  Agents cannot sudo.
- Tests don't cover rendering. After a sync, ask the user to manually check smart-width alignment on a
  book with mirrored margins, highlight persistence, and scroll position on resize.

## Conflict hotspots

Most upstream overlap lands in `zathura/document.c`, `internal.h`, `zathura.c`, `document-widget.c`,
`shortcuts.c`, and `page-widget.c`. When resolving:

- Keep both sides' additions in `internal.h`; keep upstream's structural moves (field relocations,
  lock/page-loading changes) and re-add this fork's fields (`smart_width_*`) on top.
- Page widgets are reached via `zathura_page_get_widget_by_number()`, not a `zathura->pages` array.
- `adjust-open` is applied on every document load (fork behavior); don't restore upstream's
  `known_file == false` guard.
- Smart-width realignment (`position_set(-1,-1)` after page size is final) runs after upstream's
  position restore in `document-widget.c`, not before.

## Rules

- Confirm before pushing, force-pushing, deleting branches, or installing system-wide.
- Report test failures and skipped checks as-is; don't claim a merge works until built and tested.
- Match the surrounding C style (clang-format config in repo); no unrelated refactors in sync merges.
