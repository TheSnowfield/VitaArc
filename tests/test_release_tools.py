#!/usr/bin/env python3
"""Host tests for release payloads, commit history, and retry behavior."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
import warnings
import zipfile
from pathlib import Path
from unittest.mock import MagicMock, patch
from urllib.error import HTTPError

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from check_vpk import check_vpk
from nightly_notes import generate_notes, previous_nightly
from publish_nightly import GitLab, publish_gitlab


def create_vpk(path, extra=None):
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("eboot.bin", b"executable")
        archive.writestr("sce_sys/param.sfo", b"metadata")
        for name, data in (extra or {}).items():
            archive.writestr(name, data)


class VPKTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.vpk = Path(self.temp.name) / "VitaArc.vpk"

    def test_accepts_only_application_resources(self):
        create_vpk(self.vpk, {"licenses/freetype.txt": b"license", "shaders/color.cg": b"shader",
                              "shaders/color.vgxp": b"binary", "shaders/offline.enabled": b""})
        check_vpk(self.vpk)

    def test_rejects_game_payloads_and_unknown_files(self):
        for name in ("package/assets/game.png", "assets/game.png", "obb/main.obb",
                     "lib/armeabi-v7a/libcocos2dcpp.so", "version/1.6.1c/game",
                     "arcaea.apk", "shaders/arcaea.apk", "game.dat"):
            with self.subTest(name=name):
                create_vpk(self.vpk, {name: b"game"})
                with self.assertRaisesRegex(ValueError, "unexpected VPK content"):
                    check_vpk(self.vpk)

    def test_rejects_unsafe_paths(self):
        for name in ("../eboot.bin", "/eboot.bin", "shaders\\game.cg"):
            with self.subTest(name=name):
                create_vpk(self.vpk)
                with zipfile.ZipFile(self.vpk, "a") as archive:
                    info = zipfile.ZipInfo("safe")
                    info.filename = name
                    archive.writestr(info, b"unsafe")
                with self.assertRaisesRegex(ValueError, "unsafe VPK path"):
                    check_vpk(self.vpk)

    def test_rejects_duplicates(self):
        create_vpk(self.vpk)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.vpk, "a") as archive:
                archive.writestr("eboot.bin", b"another executable")
        with self.assertRaisesRegex(ValueError, "duplicate VPK path"):
            check_vpk(self.vpk)

    def test_rejects_missing_or_empty_executable(self):
        with zipfile.ZipFile(self.vpk, "w") as archive:
            archive.writestr("eboot.bin", b"")
            archive.writestr("sce_sys/param.sfo", b"metadata")
        with self.assertRaisesRegex(ValueError, "missing or empty"):
            check_vpk(self.vpk)

    def test_rejects_crc_corruption(self):
        create_vpk(self.vpk)
        self.vpk.write_bytes(self.vpk.read_bytes().replace(b"executable", b"corruption"))
        with self.assertRaisesRegex(ValueError, "corrupt VPK file"):
            check_vpk(self.vpk)


class HistoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "VitaArc test")
        self.git("config", "user.email", "test@example.invalid")
        self.first = self.commit("Initial bridge")

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.repo), *args],
                                       text=True, encoding="utf-8", stderr=subprocess.PIPE).strip()

    def commit(self, subject):
        self.git("-c", "commit.gpgsign=false", "commit", "-q", "--allow-empty", "-m", subject)
        return self.git("rev-parse", "HEAD")

    def test_first_nightly_contains_history_and_full_sha(self):
        current = self.commit("Add version picker")
        notes = generate_notes(self.repo, current, "https://github.com/example/VitaArc")
        self.assertIn(current, notes)
        self.assertIn("Initial bridge", notes)
        self.assertIn("Add version picker", notes)
        self.assertIn(f"https://github.com/example/VitaArc/commit/{current}", notes)

    def test_prior_nightly_bounds_history_and_rerun_is_stable(self):
        self.git("tag", "nightly-" + self.first)
        current = self.commit("Next change")
        before = generate_notes(self.repo)
        self.git("tag", "nightly-" + current)
        self.assertEqual(before, generate_notes(self.repo))
        self.assertNotIn("Initial bridge", before)
        self.assertIn("Next change", before)

    def test_annotated_nightly_is_supported(self):
        self.git("-c", "tag.gpgsign=false", "tag", "-a", "nightly-" + self.first, "-m", "Nightly")
        current = self.commit("Next change")
        self.assertEqual(previous_nightly(self.repo, current), self.first)

    def test_forged_tag_and_nonancestor_tags_are_ignored(self):
        current = self.commit("Current change")
        self.git("tag", "nightly-" + "f" * 40)
        self.git("checkout", "-q", "--detach", self.first)
        other = self.commit("Unrelated branch")
        self.git("tag", "nightly-" + other)
        notes = generate_notes(self.repo, current)
        self.assertIn("Initial bridge", notes)
        self.assertNotIn("Unrelated branch", notes)

    def test_subjects_are_escaped_and_gitlab_links_are_used(self):
        current = self.commit("Fix [texture] *scale* <size>")
        notes = generate_notes(self.repo, current, "https://gitlab.example/group/repo/", "gitlab")
        self.assertIn(r"Fix \[texture\] \*scale\* &lt;size&gt;", notes)
        self.assertIn(f"https://gitlab.example/group/repo/-/commit/{current}", notes)


class FakeGitLab:
    project_url = "https://gitlab.example/api/v4/projects/1"

    def __init__(self):
        self.calls = []
        self.release = None
        self.tag = None
        self.package = None

    def request(self, method, path, payload=None, **kwargs):
        self.calls.append((method, path, payload))
        if method == "GET":
            if "/repository/tags/" in path:
                return self.tag
            return self.package if "/packages/" in path else self.release
        if method == "PUT" and "/packages/" in path:
            self.package = True
        if method == "POST" and path == "/releases":
            self.release = {"commit": {"id": payload["ref"]}, "assets": payload["assets"]}
        return {}


class PublicationTests(unittest.TestCase):
    commit = "1234567890abcdef" * 2 + "12345678"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.vpk = Path(self.temp.name) / "VitaArc.vpk"
        create_vpk(self.vpk)
        self.client = FakeGitLab()

    def test_new_release_pins_sha_and_links_durable_vpk(self):
        tag = publish_gitlab(self.client, self.vpk, self.commit, "History")
        self.assertEqual(tag, "nightly-" + self.commit)
        mutations = [call for call in self.client.calls if call[0] != "GET"]
        self.assertEqual([call[0] for call in mutations], ["PUT", "POST"])
        release = mutations[-1][2]
        self.assertEqual(release["ref"], self.commit)
        self.assertEqual(release["description"], "History")
        self.assertEqual(len(release["assets"]["links"]), 1)
        self.assertTrue(release["assets"]["links"][0]["url"].endswith(f"/{self.commit}/VitaArc.vpk"))

    def test_completed_release_rerun_has_no_mutations(self):
        publish_gitlab(self.client, self.vpk, self.commit, "Original history")
        self.client.calls.clear()
        publish_gitlab(self.client, self.vpk, self.commit, "Original history")
        self.assertTrue(all(call[0] == "GET" for call in self.client.calls))

    def test_recovers_after_package_upload_before_release_creation(self):
        self.client.package = True
        publish_gitlab(self.client, self.vpk, self.commit, "History")
        self.assertEqual([call[0] for call in self.client.calls if call[0] != "GET"], ["POST"])

    def test_repairs_missing_release_asset(self):
        self.client.release = {"commit": {"id": self.commit}, "assets": {"links": []}}
        self.client.package = True
        publish_gitlab(self.client, self.vpk, self.commit, "History")
        mutations = [call for call in self.client.calls if call[0] != "GET"]
        self.assertEqual(len(mutations), 1)
        self.assertTrue(mutations[0][1].endswith("/assets/links"))

    def test_rejects_wrong_commit_before_publishing(self):
        for existing in ("tag", "release"):
            with self.subTest(existing=existing):
                client = FakeGitLab()
                setattr(client, existing, {"commit": {"id": "f" * 40}})
                with self.assertRaisesRegex(ValueError, "another commit"):
                    publish_gitlab(client, self.vpk, self.commit, "History")
                self.assertTrue(all(call[0] == "GET" for call in client.calls))

    def test_rejects_game_payload_before_network_requests(self):
        create_vpk(self.vpk, {"arcaea.apk": b"game"})
        with self.assertRaises(ValueError):
            publish_gitlab(self.client, self.vpk, self.commit, "History")
        self.assertEqual(self.client.calls, [])

    def test_job_token_authentication_and_missing_package(self):
        response = MagicMock()
        response.__enter__.return_value.read.return_value = b'{"name": "nightly"}'
        with patch("publish_nightly.urlopen", return_value=response) as opened:
            client = GitLab("https://gitlab.example/api/v4", "group/project", "job-secret")
            self.assertEqual(client.request("POST", "/releases", {"name": "nightly"}), {"name": "nightly"})
            request = opened.call_args.args[0]
            self.assertEqual(request.get_header("Job-token"), "job-secret")
            self.assertIn("group%2Fproject", request.full_url)
            self.assertNotIn("job-secret", request.full_url)
            self.assertEqual(json.loads(request.data), {"name": "nightly"})
        error = HTTPError("https://gitlab.example", 404, "Not found", {}, None)
        with patch("publish_nightly.urlopen", side_effect=error):
            self.assertIsNone(client.request("GET", "/package", missing_ok=True))
        error = HTTPError("https://gitlab.example", 403, "Forbidden", {}, None)
        with patch("publish_nightly.urlopen", side_effect=error):
            with self.assertRaisesRegex(RuntimeError, "HTTP 403"):
                client.request("GET", "/package", missing_ok=True)


@unittest.skipIf(os.name == "nt" or not shutil.which("bash"), "GitHub publishing runs in Linux Bash")
class GitHubPublicationTests(unittest.TestCase):
    commit = "1234567890abcdef" * 2 + "12345678"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.calls = self.root / "calls.jsonl"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        # Stub only the remote CLIs; exercise the actual publishing script.
        stub = textwrap.dedent('''\
            #!/usr/bin/env python3
            import json
            import os
            import sys
            from pathlib import Path

            tool = Path(sys.argv[0]).name
            args = sys.argv[1:]
            with open(os.environ["FAKE_CALLS"], "a") as output:
                output.write(json.dumps([tool, *args]) + "\\n")
            state = os.environ["FAKE_RELEASE"]
            if tool == "gh" and args[:2] == ["release", "view"]:
                if state == "absent":
                    sys.exit(1)
                field = args[args.index("--json") + 1]
                if field == "isDraft":
                    print("true" if state == "draft" else "false")
                elif field == "targetCommitish":
                    print(os.environ["FAKE_TARGET"])
                elif field == "assets" and state == "complete":
                    print("VitaArc.vpk")
            elif tool == "gh" and args[:2] == ["release", "upload"]:
                sys.exit(int(os.environ.get("FAKE_UPLOAD_FAIL", "0")))
            elif tool == "git" and args[0] == "ls-remote":
                sys.exit(0 if os.environ["FAKE_TAG_EXISTS"] == "1" else 2)
            elif tool == "git" and args[0] == "rev-parse":
                print(os.environ["FAKE_TARGET"])
            ''')
        for name in ("gh", "git"):
            executable = self.bin / name
            executable.write_text(stub, encoding="utf-8")
            executable.chmod(0o755)

    def publish(self, ref="refs/heads/main", state="absent", target=None, tag_exists=None, upload_fail=False):
        environment = os.environ.copy()
        environment.update({
            "PATH": str(self.bin) + os.pathsep + environment["PATH"],
            "GITHUB_SHA": self.commit, "GITHUB_REF": ref,
            "FAKE_CALLS": str(self.calls), "FAKE_RELEASE": state,
            "FAKE_TARGET": target or self.commit,
            "FAKE_TAG_EXISTS": str(int(ref.startswith("refs/tags/") if tag_exists is None else tag_exists)),
            "FAKE_UPLOAD_FAIL": str(int(upload_fail)),
        })
        script = Path(__file__).resolve().parents[1] / "tools/publish_github_release.sh"
        result = subprocess.run(["bash", str(script)], cwd=self.root, env=environment,
                                text=True, capture_output=True)
        calls = [json.loads(line) for line in self.calls.read_text().splitlines()] if self.calls.exists() else []
        mutations = [call for call in calls if call[:2] == ["gh", "release"] and
                     call[2] in ("create", "upload", "edit")]
        return result, mutations

    def test_branch_push_publishes_commit_nightly_and_vpk(self):
        result, mutations = self.publish()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(mutations), 1)
        create = mutations[0]
        self.assertEqual(create[:5], ["gh", "release", "create", "nightly-" + self.commit, "build/VitaArc.vpk"])
        self.assertIn("--prerelease=true", create)
        self.assertIn("--latest=false", create)
        self.assertIn("build/nightly-notes.md", create)
        self.assertEqual(create[create.index("--target") + 1], self.commit)

    def test_version_tag_publishes_regular_release_with_generated_notes(self):
        result, mutations = self.publish("refs/tags/v1.0")
        self.assertEqual(result.returncode, 0, result.stderr)
        create = mutations[0]
        self.assertEqual(create[:5], ["gh", "release", "create", "v1.0", "build/VitaArc.vpk"])
        self.assertIn("--prerelease=false", create)
        self.assertIn("--latest=true", create)
        self.assertIn("--generate-notes", create)

    def test_completed_release_rerun_does_not_mutate(self):
        result, mutations = self.publish(state="complete")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(mutations, [])

    def test_rerun_repairs_missing_vpk(self):
        result, mutations = self.publish(state="missing_asset")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(mutations, [["gh", "release", "upload", "nightly-" + self.commit, "build/VitaArc.vpk"]])

    def test_draft_version_release_uploads_before_publishing(self):
        result, mutations = self.publish("refs/tags/v1.0", state="draft")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([call[2] for call in mutations], ["upload", "edit"])
        self.assertIn("--prerelease=false", mutations[-1])

    def test_failed_upload_keeps_release_draft(self):
        result, mutations = self.publish(state="draft", upload_fail=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual([call[2] for call in mutations], ["upload"])

    def test_wrong_tag_commit_is_rejected_before_mutations(self):
        result, mutations = self.publish("refs/tags/v1.0", target="f" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(mutations, [])

    def test_missing_version_tag_is_not_created_by_publisher(self):
        result, mutations = self.publish("refs/tags/v1.0", tag_exists=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(mutations, [])

    def test_pull_request_cannot_publish(self):
        result, mutations = self.publish("refs/pull/1/merge")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(mutations, [])


if __name__ == "__main__":
    unittest.main()
