git fetch origin wip
git merge -X theirs origin/wip --no-commit --no-ff
rm .git/MERGE_HEAD
git reset