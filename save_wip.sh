current_branch=$(git branch 2>/dev/null | grep "^*" | cut -c3-)
git stash -u
git checkout wip
git pull
git merge -X theirs $current_branch --no-commit --no-ff
rm .git/MERGE_HEAD
git checkout stash -- .
git commit -m "$(date +'%Y/%m/%d')"
git checkout $current_branch
git push --all
git stash pop