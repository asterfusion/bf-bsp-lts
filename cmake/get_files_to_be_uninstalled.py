#!/usr/bin/env python3


import argparse
import os


def get_files_to_be_uninstalled(install_manifest_entries):
    candidates = get_candidates_to_be_uninstalled(install_manifest_entries)
    to_be_removed = []

    finished = False
    while not finished:
        finished = True
        for candidate in candidates:
            if can_be_removed(candidate, to_be_removed):
                to_be_removed.append(candidate)
                candidates.remove(candidate)
                finished = False

    return to_be_removed


def get_candidates_to_be_uninstalled(install_manifest_entries):
    candidates = install_manifest_entries.copy()
    for filename in install_manifest_entries:
        for parent in get_all_parent_directories(filename):
            candidates.append(parent)
    return sorted(list(set(candidates)), key=len, reverse=True)


def can_be_removed(candidate, to_be_removed):
    return os.path.isfile(candidate) or all(e in to_be_removed for e in abs_listdir(candidate))


def abs_listdir(path):
    return [os.path.abspath(os.path.join(path, x)) for x in os.listdir(path)]


def get_all_parent_directories(filename):
    items = filename.split("/")
    return ["/".join(items[0:n]) for n in range(2, len(items))]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Return list of files and directories that should be removed",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("install_manifest", help="path to install_manifest.txt")
    args = parser.parse_args()

    with open(args.install_manifest, 'r') as file:
        installed_files = [str(line) for line in file.read().splitlines()]

    for file in get_files_to_be_uninstalled(installed_files):
        print(file)
