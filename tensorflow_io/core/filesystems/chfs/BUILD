licenses(["notice"])  # Apache 2.0

package(default_visibility = ["//visibility:public"])

load(
    "//:tools/build/tensorflow_io.bzl",
    "tf_io_copts",
)

cc_library(
    name = "chfs",
    srcs = [] + select({
        "//conditions:default": [
            "chfs_filesystem.cc",
            "chfs.cc",
            "chfs.h",
        ],
    }),
    copts = tf_io_copts(),
    linkstatic = True,
    deps = [
        "//tensorflow_io/core/filesystems:filesystem_plugins_header",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ] + select({
        "@bazel_tools//src/conditions:windows": [],
        "@bazel_tools//src/conditions:darwin": [],
        "//conditions:default": [
            "@chfs-lib",
        ],
    }),
    alwayslink = 1,
)
