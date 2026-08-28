#!/bin/bash
#
# update_readme.sh - Synchronize active SlackBuilds into repository README files
#

set -e

CWD=$(cd $(dirname $0); pwd)
REPO_ROOT=$(cd $CWD/..; pwd)

python3 "$CWD/slackbuild_tool.py" update-readme --repo-dir "$REPO_ROOT"
