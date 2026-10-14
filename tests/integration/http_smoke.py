#!/usr/bin/env python3
"""End-to-end checks against the real userver binary; only Python stdlib needed."""
import argparse
import json
import math
import re
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path


def multipart(parts):
    boundary = "local-search-" + uuid.uuid4().hex
    chunks = []
    for name, filename, content in parts:
        disposition = 'Content-Disposition: form-data; name="' + name + '"'
        if filename is not None:
            escaped = filename.replace("\\", "\\\\").replace('"', '\\"')
            disposition += '; filename="' + escaped + '"'
        chunks.append(("--" + boundary + "\r\n" + disposition + "\r\n").encode("utf-8"))
        if filename is not None:
            chunks.append(b"Content-Type: text/plain\r\n")
        chunks.extend([b"\r\n", content, b"\r\n"])
    chunks.append(("--" + boundary + "--\r\n").encode("ascii"))
    return b"".join(chunks), "multipart/form-data; boundary=" + boundary


class Checks:
    def __init__(self, base_url):
        self.base_url = base_url
        self.count = 0

    def request(self, path, status, *, data=None, content_type=None, error=None):
        headers = {}
        if content_type:
            headers["Content-Type"] = content_type
        request = urllib.request.Request(
            self.base_url + path, data=data, headers=headers,
            method="POST" if data is not None else "GET",
        )
        try:
            response = urllib.request.urlopen(request, timeout=30)
        except urllib.error.HTTPError as exc:
            response = exc
        with response:
            body = response.read()
            assert response.status == status, (path, response.status, body)
            assert response.headers.get("Content-Type", "").startswith("application/json"), (
                path, response.headers, body
            )
        result = json.loads(body)
        if error is not None:
            assert set(result) == {"error"}, result
            assert result["error"]["code"] == error, result
            assert isinstance(result["error"]["message"], str), result
        self.count += 1
        return result

    def upload(self, parts, status=201, error=None):
        body, content_type = multipart(parts)
        return self.request(
            "/documents", status, data=body, content_type=content_type, error=error
        )

    def search(self, query, limit=None, status=200, error=None):
        params = [("q", query)]
        if limit is not None:
            params.append(("limit", limit))
        path = "/search?" + urllib.parse.urlencode(params)
        return self.request(path, status, error=error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="build-server/local_search")
    parser.add_argument("--config", default="configs/static_config.yaml")
    args = parser.parse_args()
    binary = Path(args.binary).resolve()
    source_config = Path(args.config).read_text(encoding="utf-8")
    if not binary.is_file():
        parser.error("Build the server first: " + str(binary))

    with tempfile.TemporaryDirectory(prefix="local-search-http-") as temp:
        directory = Path(temp)
        data_directory = directory / "data"
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]

        config, replacements = re.subn(
            r"(?m)^([ \t]*port:)[ \t]*[0-9]+[ \t]*$",
            lambda match: match.group(1) + " " + str(port),
            source_config, count=1,
        )
        assert replacements == 1, "No listener port found in YAML"
        config, replacements = re.subn(
            r"(?m)^([ \t]*data-dir:)[^\r\n]*$",
            lambda match: match.group(1) + " " + json.dumps(str(data_directory)),
            config, count=1,
        )
        assert replacements == 1, "No search-component data-dir found in YAML"
        config_path = directory / "config.yaml"
        config_path.write_text(config, encoding="utf-8")

        base_url = "http://127.0.0.1:" + str(port)
        checks = Checks(base_url)
        process = None
        with (directory / "server.log").open("ab") as log:
            def start():
                nonlocal process
                process = subprocess.Popen(
                    [str(binary), "-c", str(config_path)],
                    stdout=log, stderr=subprocess.STDOUT,
                )
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise AssertionError("Server exited during startup")
                    try:
                        with urllib.request.urlopen(base_url + "/health", timeout=1) as response:
                            if response.status == 200 and response.read() == b"ok\n":
                                return
                    except (OSError, urllib.error.URLError):
                        pass
                    time.sleep(0.1)
                raise AssertionError("Server did not become ready")

            def stop():
                nonlocal process
                if process is not None:
                    if process.poll() is None:
                        process.send_signal(signal.SIGINT)
                        try:
                            process.wait(timeout=20)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
                    process = None

            try:
                start()
                assert checks.search("missing") == {"results": []}
                text = "Умные указатели управляют памятью."
                first = checks.upload([
                    ("file", "../../pointers.TXT", b"\xef\xbb\xbf" + text.encode("utf-8"))
                ])
                assert set(first) == {"document_id", "title", "word_count"}, first
                assert first["title"] == "pointers.TXT" and first["word_count"] == 4, first
                assert type(first["document_id"]) is int and first["document_id"] > 0
                document_id = first["document_id"]

                found = checks.search("умные указатели")["results"]
                assert len(found) == 1, found
                item = found[0]
                assert set(item) == {"document_id", "title", "score", "snippet"}, item
                assert item["document_id"] == document_id and item["snippet"] == text, item
                assert math.isfinite(item["score"]) and item["score"] > 0
                duplicate_query = checks.search("умные умные указатели указатели")["results"]
                assert math.isclose(duplicate_query[0]["score"], item["score"], rel_tol=1e-12)

                checks.request("/search", 400, error="InvalidQuery")
                checks.request("/search?q=a&q=b", 400, error="InvalidQuery")
                for query in ["", " ", "!!!", "я" * 1025]:
                    checks.search(query, status=400, error="InvalidQuery")
                checks.request("/search?q=%FF", 400, error="InvalidQuery")
                for limit in ["", "0", "101", "-1", "+1", "1.5", " 1", "1e2", "9" * 60]:
                    checks.search("умные", limit, status=400, error="InvalidLimit")
                checks.request("/search?q=a&limit=1&limit=2", 400, error="InvalidLimit")

                checks.upload([], 400, "InvalidRequest")
                checks.upload([("other", "a.txt", b"text")], 400, "InvalidRequest")
                checks.upload([("file", None, b"text")], 400, "InvalidRequest")
                checks.upload([
                    ("file", "a.txt", b"text"), ("extra", None, b"x")
                ], 400, "InvalidRequest")
                checks.upload([
                    ("file", "a.txt", b"text"), ("file", "b.txt", b"text")
                ], 400, "InvalidRequest")
                checks.upload([("file", "", b"text")], 400, "InvalidRequest")
                checks.upload([("file", "x" * 252 + ".txt", b"text")], 400, "InvalidRequest")
                checks.upload([("file", "a.pdf", b"text")], 415, "UnsupportedFileType")
                for invalid in [b"", b"   !!!", b"\xff", b"word\x00word", b"\xef\xbb\xbf"]:
                    checks.upload([("file", "a.txt", invalid)], 400, "InvalidText")
                checks.request(
                    "/documents", 400, data=b"not multipart",
                    content_type="application/json", error="InvalidRequest",
                )
                checks.request(
                    "/documents", 400, data=b"bad multipart",
                    content_type="multipart/form-data; boundary=missing",
                    error="InvalidRequest",
                )

                second = checks.upload([("file", "pointers.TXT", text.encode("utf-8"))])
                assert second["document_id"] == document_id + 1, second
                tied = checks.search("умные", "1")["results"]
                assert len(tied) == 1 and tied[0]["document_id"] == document_id, tied
                ids = [entry["document_id"] for entry in checks.search("указатели")["results"]]
                assert ids == [document_id, second["document_id"]], ids

                max_file = b"word" + b" " * (5 * 1024 * 1024 - 4)
                maximum = checks.upload([("file", "maximum.txt", max_file)])
                assert maximum["word_count"] == 1, maximum
                checks.upload([("file", "a.txt", max_file + b"x")], 413, "DocumentTooLarge")
                checks.upload([
                    ("file", "a.txt", b"x" * (6 * 1024 * 1024 + 128 * 1024))
                ], 413, "DocumentTooLarge")

                # Fault injection uses only our temporary data directory.
                next_id = maximum["document_id"] + 1
                blocked = data_directory / "documents" / (str(next_id) + ".json.tmp")
                blocked.mkdir()
                failure = checks.upload(
                    [("file", "failure.txt", b"failuretoken")], 500, "InternalError"
                )
                assert str(data_directory) not in failure["error"]["message"]
                assert checks.search("failuretoken") == {"results": []}
                if blocked.exists():
                    blocked.rmdir()
                recovered = checks.upload([("file", "recovery.txt", b"recoverytoken")])
                assert recovered["document_id"] == next_id, recovered

                stop()
                start()
                restored = checks.search("указатели")["results"]
                assert [item["document_id"] for item in restored] == ids, restored
                assert checks.search("recoverytoken")["results"][0]["document_id"] == next_id
                print("PASS:", checks.count, "HTTP checks, including restart and storage failure")
            except Exception:
                stop()
                print((directory / "server.log").read_text(encoding="utf-8", errors="replace")[-8000:])
                raise
            finally:
                stop()


if __name__ == "__main__":
    main()
