#!/usr/bin/env python3
import json
import plumbum
from pathlib import Path

THIS_DIR = Path(__file__).parent.resolve()
ROOT_DIR = THIS_DIR.parent.resolve()

CONAN = plumbum.local["conan"]


def get_profiles():
    profiles = CONAN("profile", "list", "--format=json")
    return json.loads(profiles)


def rebuild():
    profiles = get_profiles()

    for build_type in ["Release", "Debug"]:
        for profile in profiles:
            print(f"Rebuilding {profile} with build type {build_type}")
            with plumbum.local.cwd(ROOT_DIR):
                CONAN[
                    "install", ".",
                    "--build=missing",
                    f"--output-folder=build_{profile}_{build_type}",
                    "-s", f"build_type={build_type}",
                    "-o", "&:tests=True",
                    "-o", "&:benchmark=True",
                    "--profile:all", profile
                ] & plumbum.FG


if __name__ == "__main__":
    rebuild()
