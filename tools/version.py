# PlatformIO pre-script: FW_BUILD = number of the GitHub Actions run, so the
# board, the settings page and the release "build-<N>" show the same number as
# the Actions tab. In CI it comes from GITHUB_RUN_NUMBER. Locally it is taken
# from the newest build-* tag in HEAD's history (git pull fetches it), so a
# build from the same commit as GitHub's release counts as up to date
# (src/core/updater.cpp).
import os
import subprocess

Import("env")

def local_build():
    try:
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0", "--match", "build-*"],
            cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()
        return int(tag[len("build-"):])
    except Exception:
        return 0

build = int(os.environ.get("GITHUB_RUN_NUMBER") or local_build())
print("FW_BUILD=%d" % build)
env.Append(CPPDEFINES=[("FW_BUILD", build)])
