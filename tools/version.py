# PlatformIO pre-script: FW_BUILD = number of commits, so a build from the
# same commit as GitHub's release counts as up to date (src/updater.cpp).
import subprocess

Import("env")

try:
    build = int(subprocess.check_output(["git", "rev-list", "--count", "HEAD"], cwd=env["PROJECT_DIR"]).strip())
except Exception:
    build = 0
print("FW_BUILD=%d" % build)
env.Append(CPPDEFINES=[("FW_BUILD", build)])
