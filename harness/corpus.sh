#!/bin/sh
# Manage corpus/: the NitrOS9 source the harness assembles.
#
#   harness/corpus.sh          clone any missing repo and check out the commit
#                              recorded in harness/corpus.lock
#   harness/corpus.sh update   fetch each repo's branch and move the lock to
#                              its newest commit (then rerun the harness and
#                              commit the new lock)
#   harness/corpus.sh status   show locked vs checked-out commits
#
# corpus/ is not version-controlled; corpus.lock is.
set -e

DEVSYS=$(cd "$(dirname "$0")/.." && pwd)
LOCK="$DEVSYS/harness/corpus.lock"
CORPUS="$DEVSYS/corpus"
CMD=${1:-sync}

mkdir -p "$CORPUS"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

grep -v '^#' "$LOCK" | while read -r name url branch commit; do
    [ -n "$name" ] || continue
    dir="$CORPUS/$name"
    if [ ! -d "$dir/.git" ]; then
        echo "cloning $name ($branch)"
        git clone -q -b "$branch" "$url" "$dir"
    fi
    case "$CMD" in
    sync)
        if [ -n "$(git -C "$dir" status --porcelain --untracked-files=no)" ]; then
            echo "$name: has local changes; not touching it" >&2
            exit 1
        fi
        git -C "$dir" fetch -q origin "$branch"
        git -C "$dir" checkout -q "$commit"
        echo "$name: at $(git -C "$dir" log -1 --format='%h %ad %s' --date=short)"
        echo "$name  $url  $branch  $commit" >> "$tmp"
        ;;
    update)
        git -C "$dir" fetch -q origin "$branch"
        new=$(git -C "$dir" rev-parse "origin/$branch")
        if [ "$new" = "$commit" ]; then
            echo "$name: already at newest $branch ($(echo "$commit" | cut -c1-8))"
        else
            echo "$name: $(echo "$commit" | cut -c1-8) -> $(echo "$new" | cut -c1-8)"
            git -C "$dir" log --oneline "$commit..$new" | sed 's/^/    /'
        fi
        git -C "$dir" checkout -q "$new"
        echo "$name  $url  $branch  $new" >> "$tmp"
        ;;
    status)
        head=$(git -C "$dir" rev-parse HEAD)
        if [ "$head" = "$commit" ]; then state="ok"; else state="DIFFERS from lock"; fi
        echo "$name: lock $(echo "$commit" | cut -c1-8), checked out $(echo "$head" | cut -c1-8) ($state)"
        echo "$name  $url  $branch  $commit" >> "$tmp"
        ;;
    *)
        echo "usage: $0 [sync|update|status]" >&2
        exit 2
        ;;
    esac
done

if [ "$CMD" = "update" ]; then
    { grep '^#' "$LOCK"; cat "$tmp"; } > "$LOCK.new" && mv "$LOCK.new" "$LOCK"
    echo "lock updated: rerun harness/lwdiff.py, then commit harness/corpus.lock"
fi
