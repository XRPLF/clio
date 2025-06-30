#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


def fix_json_style(cpp_content: str) -> str:
    cpp_content = cpp_content.replace('R"json(', 'R"JSON(').replace(')json"', ')JSON"')

    pattern = r'R"JSON\((.*?)\)JSON"'

    def replace_json(match):
        raw_json = match.group(1)

        raw_json = (
            raw_json.replace(" :", ":")
            .replace(" ,", ",")
            .replace(" null", "null")
            .replace(':"', ': "')
            .replace(',"', ', "')
            .replace('":{', '": {')
            .replace('":[', '": [')
            .replace('":true', '": true')
            .replace('":false', '": false')
            .replace('":null', '": null')
        )
        for digit in range(10):
            raw_json = raw_json.replace(f'":{digit}', f'": {digit}')
        return f'R"JSON({raw_json})JSON"'

    return re.sub(pattern, replace_json, cpp_content, flags=re.DOTALL)


def process_file(file_path: Path, dry_run: bool) -> bool:
    content = file_path.read_text(encoding="utf-8")
    new_content = fix_json_style(content)

    if new_content != content:
        print(f"Processing file: {file_path}")
        if dry_run:
            print("Dry run: changes won't be written to the file.")
        else:
            print("Writing changes to file.")
            file_path.write_text(new_content, encoding="utf-8")
    return new_content == content


def main():
    parser = argparse.ArgumentParser(
        description="Fix JSON style in C++ files",
    )
    parser.add_argument(
        "--dry-run",
        default=False,
        action="store_true",
        help="Don't modify files, just print what would be changed",
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="Specific files to process",
    )

    args = parser.parse_args()

    success = True
    for file in args.files:
        success = success and process_file(Path(file), dry_run=args.dry_run)
    if not success:
        print("Errors occurred while processing files.")
        exit(1)


if __name__ == "__main__":
    main()
