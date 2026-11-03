# PlatformIO pre-script: git branch, commit and build time for src/utils/build_info.cpp.
# The defines go to that one file only (build middleware), so each build recompiles just it
# instead of everything. Without git (e.g. a ZIP download) the values are "unknown".
import datetime
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


branch = git("rev-parse", "--abbrev-ref", "HEAD") or "unknown"
commit = git("rev-parse", "--short", "HEAD") or "unknown"
if commit != "unknown" and git("status", "--porcelain"):
    commit += "+"  # uncommitted changes
build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")


def add_build_info(env, node):
    defines = [
        ("BUILD_INFO_BRANCH", env.StringifyMacro(branch)),
        ("BUILD_INFO_COMMIT", env.StringifyMacro(commit)),
        ("BUILD_INFO_TIME", env.StringifyMacro(build_time)),
    ]
    return env.Object(node, CPPDEFINES=list(env.get("CPPDEFINES", [])) + defines)


env.AddBuildMiddleware(add_build_info, "*/utils/build_info.cpp")
