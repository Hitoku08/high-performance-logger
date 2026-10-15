# coding=utf-8
import shutil
import os
import sys
import glob
import subprocess
import argparse

SCRIPT_PATH = os.path.split(os.path.realpath(__file__))[0]
BUILD_DIR_PATH = SCRIPT_PATH + "/../build"


def clear():
    if os.path.exists(BUILD_DIR_PATH):
        shutil.rmtree(BUILD_DIR_PATH)


def get_project_path():
    project_path = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
    return project_path


def gene_proto_buf_source_file(out_dir):
    proto_dir = get_project_path() + "/logger/proto"
    proto_files = glob.glob(proto_dir + "/*.proto")
    if not proto_files:
        print("no .proto file yet, skip generating pb source")
        return True

    protoc_path = get_project_path() + "/script/bin/protoc"
    if sys.platform.startswith("win"):
        protoc_path = get_project_path() + "/script/bin/protoc.exe"

    build_cmd = [protoc_path, "-I=" + proto_dir, "--cpp_out=" + out_dir] + proto_files
    print("protoc cmd:" + " ".join(build_cmd))
    ret = subprocess.call(build_cmd)
    if ret != 0:
        print("!!!generate pb source file fail")
        return False
    return True


def run_tests(config):
    build_cmd = "ctest -C %s --output-on-failure" % (config)
    print("test cmd:" + build_cmd)
    ret = os.system(build_cmd)
    if ret != 0:
        print("test fail!!!!!!!!!!!!!!!!!!!!")
        return False
    print("all tests passed")
    return True


def build_linux(config="Release", args=None):
    platform_dir = "%s/%s" % (BUILD_DIR_PATH, config)
    os.makedirs(platform_dir, exist_ok=True)
    if not gene_proto_buf_source_file(platform_dir):
        return False
    os.chdir(platform_dir)

    build_cmd = "cmake ../..  -DCMAKE_BUILD_TYPE=%s -DCMAKE_SYSTEM_NAME=Linux" % (
        config
    )
    build_cmd += " -DBUILD_STL_TEST=%s" % ("ON" if args.test else "OFF")

    print("build cmd:" + build_cmd)
    ret = os.system(build_cmd)
    if ret != 0:
        print("!!!!!!!!!!!!!!!!!!build fail")
        return False

    build_cmd = "cmake --build . --config %s --parallel 8" % (config)
    ret = os.system(build_cmd)
    if ret != 0:
        print("build fail!!!!!!!!!!!!!!!!!!!!")
        return False

    if args.test:
        return run_tests(config)
    return True


def main():
    parser = argparse.ArgumentParser(description="build linux")
    parser.add_argument(
        "--test", action="store_true", default=False, help="build and run unittest"
    )
    parser.add_argument(
        "--clean", action="store_true", default=False, help="remove build dir first"
    )
    parser.add_argument(
        "--config", default="Debug", choices=["Debug", "Release"], help="build config"
    )
    args = parser.parse_args()

    if args.clean:
        clear()
    os.makedirs(BUILD_DIR_PATH, exist_ok=True)

    if not build_linux(config=args.config, args=args):
        exit(1)


if __name__ == "__main__":
    main()
