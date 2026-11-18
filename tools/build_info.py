# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

# PlatformIO pre-script: version, git branch, commit and build time for src/utils/build_info.cpp.
# The defines go to that one file only (build middleware), so each build recompiles just it
# instead of everything. Without git (e.g. a ZIP download) branch and commit are "unknown".
#
# Reproducible stamps (AF25): two builds of the same commit give the same text.
# - Version: APP_VERSION, or the tag of a release (candidate) built from it, e.g. v2.0.0-rc1 ->
#   "2.0.0-rc1" (GITHUB_REF_NAME in the release workflow, otherwise a tag v* exactly at HEAD).
# - Branch: the branch; at a detached HEAD the tag, else the branch of the pull request
#   (GITHUB_HEAD_REF) or "HEAD".
# - Time: SOURCE_DATE_EPOCH if set, otherwise the commit time of HEAD; always UTC. Only a build
#   with uncommitted changes (commit "+") uses the current time.
import datetime
import os
import re
import subprocess

Import("env")


def git(*args):
    try:
        out = subprocess.check_output(
            ["git"] + list(args), cwd=env.subst("$PROJECT_DIR"), stderr=subprocess.DEVNULL
        )
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return None


def app_version():
    path = os.path.join(env.subst("$PROJECT_DIR"), "include", "config.h")
    with open(path, encoding="utf-8") as f:
        m = re.search(r'^#define APP_VERSION "([^"]+)"', f.read(), re.M)
    return m.group(1) if m else None


commit = git("rev-parse", "--short", "HEAD") or "unknown"
dirty = commit != "unknown" and bool(git("status", "--porcelain", "--untracked-files=no"))
if dirty:
    commit += "+"  # uncommitted changes

if os.environ.get("GITHUB_REF_TYPE") == "tag":
    tag = os.environ.get("GITHUB_REF_NAME")
else:
    tag = git("describe", "--tags", "--exact-match", "--match", "v*", "HEAD")
app = app_version()
version = None
if tag and app and not dirty and (tag == "v" + app or tag.startswith("v" + app + "-")):
    if re.match(r"^[0-9A-Za-z.+-]{1,31}$", tag[1:]):
        version = tag[1:]

branch = git("rev-parse", "--abbrev-ref", "HEAD") or "unknown"
if branch == "HEAD":
    branch = tag or os.environ.get("GITHUB_HEAD_REF") or "HEAD"
if not re.match(r"^[0-9A-Za-z._/+-]{1,100}$", branch):
    branch = "unknown"

stamp = None
sde = os.environ.get("SOURCE_DATE_EPOCH", "").strip()
if sde.isdigit():
    stamp = int(sde)
elif not dirty:
    ct = git("log", "-1", "--format=%ct")
    stamp = int(ct) if ct and ct.isdigit() else None
if stamp is None:
    when = datetime.datetime.now(datetime.timezone.utc)
else:
    when = datetime.datetime.fromtimestamp(stamp, datetime.timezone.utc)
build_time = when.strftime("%Y-%m-%d %H:%M UTC")


def add_build_info(env, node):
    defines = [
        ("BUILD_INFO_BRANCH", env.StringifyMacro(branch)),
        ("BUILD_INFO_COMMIT", env.StringifyMacro(commit)),
        ("BUILD_INFO_TIME", env.StringifyMacro(build_time)),
    ]
    if version:
        defines.append(("BUILD_INFO_VERSION", env.StringifyMacro(version)))
    return env.Object(node, CPPDEFINES=list(env.get("CPPDEFINES", [])) + defines)


env.AddBuildMiddleware(add_build_info, "*/utils/build_info.cpp")
