#!/usr/bin/env python3
"""Publish a commit-specific GitLab nightly using CI_JOB_TOKEN."""

import argparse
import json
import os
import re
import zipfile
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

from check_vpk import check_vpk


class GitLab:
    def __init__(self, api_url, project_id, token):
        self.project_url = f"{api_url.rstrip('/')}/projects/{quote(project_id, safe='')}"
        self.token = token

    def request(self, method, path, payload=None, missing_ok=False, binary=False):
        data = payload if binary else json.dumps(payload).encode("utf-8") if payload is not None else None
        headers = {"JOB-TOKEN": self.token}
        if data is not None:
            headers["Content-Type"] = "application/octet-stream" if binary else "application/json"
        request = Request(self.project_url + path, data=data, headers=headers, method=method)
        try:
            with urlopen(request, timeout=60) as response:
                if binary and method == "GET":
                    # A completed package upload is already durable; preserve it on reruns.
                    return True
                content = response.read()
                return json.loads(content) if content else {}
        except HTTPError as error:
            if missing_ok and error.code == 404:
                return None
            raise RuntimeError(f"GitLab {method} {path}: HTTP {error.code}") from None


def publish_gitlab(client, vpk, commit, notes):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("nightly commit must be a full lowercase SHA-1")
    check_vpk(vpk)
    tag = f"nightly-{commit}"
    release_path = f"/releases/{tag}"
    release = client.request("GET", release_path, missing_ok=True)
    if release is not None and release.get("commit", {}).get("id") != commit:
        raise ValueError(f"existing {tag} targets another commit")
    if release is None:
        existing_tag = client.request("GET", f"/repository/tags/{tag}", missing_ok=True)
        if existing_tag is not None and existing_tag.get("commit", {}).get("id") != commit:
            raise ValueError(f"existing {tag} targets another commit")

    package_path = f"/packages/generic/vitaarc-nightly/{commit}/VitaArc.vpk"
    package_url = client.project_url + package_path
    if client.request("GET", package_path, missing_ok=True, binary=True) is None:
        client.request("PUT", package_path, Path(vpk).read_bytes(), binary=True)
    asset = {"name": "VitaArc.vpk", "url": package_url,
             "direct_asset_path": "/VitaArc.vpk", "link_type": "package"}
    if release is None:
        client.request("POST", "/releases", {
            "name": f"Nightly {commit[:12]}", "tag_name": tag, "ref": commit,
            "description": notes, "assets": {"links": [asset]},
        })
    else:
        links = release.get("assets", {}).get("links", [])
        existing = next((link for link in links if link.get("name") == "VitaArc.vpk"), None)
        if existing is None:
            client.request("POST", release_path + "/assets/links", asset)
        elif existing.get("url") != package_url:
            client.request("PUT", release_path + f"/assets/links/{existing['id']}", asset)
    return tag


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vpk", type=Path, required=True)
    parser.add_argument("--notes", type=Path, required=True)
    args = parser.parse_args()
    try:
        client = GitLab(os.environ["CI_API_V4_URL"], os.environ["CI_PROJECT_ID"],
                        os.environ["CI_JOB_TOKEN"])
        tag = publish_gitlab(client, args.vpk, os.environ["CI_COMMIT_SHA"],
                             args.notes.read_text(encoding="utf-8"))
    except (KeyError, OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Nightly publication failed: {error}\n")
    print(f"Published {tag}")


if __name__ == "__main__":
    main()
