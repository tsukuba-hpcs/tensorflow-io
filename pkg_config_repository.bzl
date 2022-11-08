BUILD_TEMPLATE = """
package(
    default_visibility = ["//visibility:public"],
)
cc_library(
    name = "{name}",
    hdrs = glob([
        {hdrs}
    ]),
    copts = [
        {copts}
    ],
    includes = [
        {includes}
    ],
    linkopts = [
        {linkopts}
    ],
)
"""

EXPORTS_FILES_TEMPLATE = """
exports_files(glob(["bin/*"]))
"""

def _exec_pkg_config(repo_ctx, flag):
    binary = repo_ctx.which("pkg-config")
    result = repo_ctx.execute([binary, flag] + repo_ctx.attr.packages)
    items = result.stdout.strip().split(" ")
    uniq_items = sorted({key: None for key in items}.keys())
    return uniq_items

def _make_strlist(list):
    return "\"" + "\",\n        \"".join(list) + "\""

def _symlinks(repo_ctx, paths):
    for path in paths:
        if repo_ctx.path(path).exists:
            continue
        repo_ctx.symlink("/" + path, path)

def _pkg_config_repository_impl(repo_ctx):
    includes = _exec_pkg_config(repo_ctx, "--cflags-only-I")
    includes = [item[len("-I/"):] for item in includes]
    _symlinks(repo_ctx, includes)
    data = {
        "name": repo_ctx.attr.name,
        "hdrs": _make_strlist([item + "/**" for item in includes]),
        "copts": _make_strlist(_exec_pkg_config(repo_ctx, "--cflags-only-other")),
        "includes": _make_strlist(includes),
        #"linkopts": _make_strlist(_exec_pkg_config(repo_ctx, "--libs-only-l")),
        "linkopts": _make_strlist(_exec_pkg_config(repo_ctx, "--libs")),
    }
    build_file_data = BUILD_TEMPLATE.format(**data)

    # host_bins
    host_bins = _exec_pkg_config(repo_ctx, "--variable=host_bins")
    if len(host_bins) == 1:
        repo_ctx.symlink(host_bins[0], "bin")
        build_file_data += EXPORTS_FILES_TEMPLATE

    repo_ctx.file("BUILD.bazel", build_file_data)

pkg_config_repository = repository_rule(
    implementation = _pkg_config_repository_impl,
    attrs = {
        "packages": attr.string_list(),
    },
)
