#!/usr/bin/env bash
# Publish the verified build after host tests pass. Reruns repair missing assets.
set -euo pipefail

[[ "$GITHUB_SHA" =~ ^[0-9a-f]{40}$ ]]
case "$GITHUB_REF" in
  refs/heads/*)
    tag="nightly-${GITHUB_SHA}"
    title="Nightly ${GITHUB_SHA:0:12}"
    prerelease=true
    latest=false
    notes=(--notes-file build/nightly-notes.md)
    ;;
  refs/tags/v*)
    tag="${GITHUB_REF#refs/tags/}"
    title="$tag"
    prerelease=false
    latest=true
    notes=(--generate-notes)
    ;;
  *)
    echo "Releases require a branch or a v-prefixed version tag." >&2
    exit 1
    ;;
esac

if gh release view "$tag" --json tagName >/dev/null 2>&1; then
  draft="$(gh release view "$tag" --json isDraft --jq .isDraft)"
  if [ "$draft" = "true" ]; then
    target="$(gh release view "$tag" --json targetCommitish --jq .targetCommitish)"
    test "$target" = "$GITHUB_SHA"
  else
    git fetch --no-tags origin "refs/tags/${tag}"
    test "$(git rev-parse 'FETCH_HEAD^{commit}')" = "$GITHUB_SHA"
  fi
  asset="$(gh release view "$tag" --json assets --jq '.assets[] | select(.name == "VitaArc.vpk") | .name')"
  if [ -z "$asset" ]; then
    gh release upload "$tag" build/VitaArc.vpk
  fi
  if [ "$draft" = "true" ]; then
    gh release edit "$tag" --draft=false --prerelease="$prerelease" --latest="$latest"
  fi
else
  if git ls-remote --exit-code --tags origin "refs/tags/${tag}" >/dev/null; then
    git fetch --no-tags origin "refs/tags/${tag}"
    test "$(git rev-parse 'FETCH_HEAD^{commit}')" = "$GITHUB_SHA"
  else
    status=$?
    test "$status" -eq 2
    # A version release must use the existing pushed tag.
    test "$prerelease" = "true"
  fi
  gh release create "$tag" build/VitaArc.vpk \
    --target "$GITHUB_SHA" --title "$title" \
    "${notes[@]}" --prerelease="$prerelease" --latest="$latest"
fi
