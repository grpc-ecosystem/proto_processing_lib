workspace(name = "proto_processing_lib")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "bazel_skylib",
    sha256 = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68acee3cb94",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
    ],
)

http_archive(
    name = "com_google_googletest",
    sha256 = "730215d76eace9dd49bf74ce044e8daa065d175f1ac891cc1d6bb184ef94e565",
    strip_prefix = "googletest-f53219cdcb7b084ef57414efea92ee5b71989558",
    urls = [
        "https://github.com/google/googletest/archive/f53219cdcb7b084ef57414efea92ee5b71989558.tar.gz",  # 2023-03-16
    ],
)

load("@com_google_googletest//:googletest_deps.bzl", "googletest_deps")

googletest_deps()

# Archive building rules.
http_archive(
    name = "rules_pkg",
    sha256 = "038f1caa773a7e35b3663865ffb003169c6a71dc995e39bf4815792f385d837d",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
    ],
)

# For status_macros
http_archive(
    name = "ocp",
    strip_prefix = "ocp-diag-core-e965ac0ac6db6686169678e2a6c77ede904fa82c/apis/c++",
    url = "https://github.com/opencomputeproject/ocp-diag-core/archive/e965ac0ac6db6686169678e2a6c77ede904fa82c.tar.gz",
)

http_archive(
    name = "grpc_httpjson_transcoding",
    strip_prefix = "grpc-httpjson-transcoding-ff41eb3fc9209e6197595b54f7addfa244c0bdb6",  # June 7, 2023
    url = "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding/archive/ff41eb3fc9209e6197595b54f7addfa244c0bdb6.tar.gz",
    #    commit = "ff41eb3fc9209e6197595b54f7addfa244c0bdb6",  # June 7, 2023
    #    remote = "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding.git",
)

http_archive(
    name = "com_google_protofieldextraction",
    strip_prefix = "proto-field-extraction-d5d39f0373e9b6691c32c85929838b1006bcb3fb",  # July 9, 2024
    url = "https://github.com/grpc-ecosystem/proto-field-extraction/archive/d5d39f0373e9b6691c32c85929838b1006bcb3fb.tar.gz",
)

# -------- Load and call dependencies of underlying libraries --------

load("@grpc_httpjson_transcoding//:repositories.bzl", "absl_repositories", "googleapis_repositories", "io_bazel_rules_docker", "protobuf_repositories", "protoconverter_repositories", "zlib_repositories")

protoconverter_repositories()

googleapis_repositories()

protobuf_repositories()

zlib_repositories()

absl_repositories()

io_bazel_rules_docker()

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()
