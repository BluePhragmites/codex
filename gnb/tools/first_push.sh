#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <remote-url>"
  exit 1
fi

remote_url="$1"

git init
git add .
git commit -m "Initial mini gNB Msg1-Msg4 prototype"
git branch -M main
git remote add origin "${remote_url}"
git push -u origin main
