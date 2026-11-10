# PlatformIO pre-script: __FILE__ of the framework, the libraries and the build directory without
# the local path (log and assert texts of the core). The image no longer contains the user's
# home directory and does not depend on where it was built. Forward slashes: on Windows the
# shell drops backslashes in build flags, and GCC compares both separators alike.
Import("env")  # noqa: F821  (SCons)

for path, short in ((env.subst("$PROJECT_PACKAGES_DIR"), "pkg"),  # noqa: F821
                    (env.subst("$PROJECT_WORKSPACE_DIR"), "pio")):  # noqa: F821
    env.Append(CCFLAGS=["-fmacro-prefix-map=%s=%s" % (path.replace("\\", "/"), short)])  # noqa: F821
