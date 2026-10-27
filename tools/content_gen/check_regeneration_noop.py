#!/usr/bin/env python3
"""Regeneration is a no-op: re-running gen_content.py must reproduce the committed data.

The generator's own docstring says the emitted files are the source of truth and that the
contributing workflow is "edit intent, re-run, review the diff" - so a re-run that changes
bytes means anyone following that workflow silently loses content. A scratch copy of the data
is made, the generator is run over it, and every file it emits is compared byte for byte
against what it started with.

The same run plants content the generator does not emit and checks the diff is reported, so
the comparison cannot pass by comparing nothing, and so the class of bug - a field added to
the data by hand after the generator last ran - fails loudly rather than only the instances
seen today.

    python3 tools/content_gen/check_regeneration_noop.py [repo-root]

Registered in the gate as the CTest case `content_generator_no_op`.
"""
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PLANTED_FILE = "materials.json"


def generated_names():
    """Every file the generator writes, read off the script itself so an emitted file added
    later is covered by the same comparison instead of needing the test to be told."""
    with open(os.path.join(HERE, "gen_content.py"), encoding="utf-8") as f:
        return sorted(set(re.findall(r'^write\("([^"]+\.json)"', f.read(), re.M)))


def scratch_tree(root):
    """A repo-shaped scratch copy, because the generator resolves its output from its own
    location: tools/content_gen/ (the script plus vein_tuning.json) over data/."""
    scratch = tempfile.mkdtemp(prefix="deepforge-regen-")
    shutil.copytree(os.path.join(root, "tools", "content_gen"),
                    os.path.join(scratch, "tools", "content_gen"))
    shutil.copytree(os.path.join(root, "data"), os.path.join(scratch, "data"))
    return scratch


def run_generator(scratch):
    run = subprocess.run([sys.executable, os.path.join(scratch, "tools", "content_gen",
                                                        "gen_content.py")],
                      cwd=scratch, capture_output=True, text=True)
    if run.returncode != 0:
        raise RuntimeError(f"the generator exited {run.returncode}\n{run.stdout}{run.stderr}")


def snapshot(data_dir):
    return {name: open(os.path.join(data_dir, name), "rb").read() if
            os.path.isfile(os.path.join(data_dir, name)) else None
            for name in generated_names()}


def report(before, after):
    """Files whose bytes changed under the generator, each with its diff, so a failure says
    which file and which field instead of only that something moved."""
    changed = []
    for name in sorted(before):
        if before[name] == after[name]:
            continue
        changed.append(name)
        print(f"  {name}")
        if before[name] is None or after[name] is None:
            print(f"    present before: {before[name] is not None}; after: "
                  f"{after[name] is not None}")
            continue
        diff = "".join(difflib.unified_diff(
            before[name].decode("utf-8").splitlines(True),
            after[name].decode("utf-8").splitlines(True),
            fromfile=f"before/{name}", tofile=f"after/{name}", n=1))
        print(diff or "  (same text, different bytes)")
    return changed


def main():
    root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(
        os.path.join(HERE, "..", ".."))
    if not generated_names():
        print("FAIL: no file in gen_content.py is written by write(), so there is nothing to "
              "compare")
        return 1

    # 1. The documented workflow over the shipped data must produce no diff at all.
    scratch = scratch_tree(root)
    try:
        before = snapshot(os.path.join(scratch, "data"))
        run_generator(scratch)
        drift = report(before, snapshot(os.path.join(scratch, "data")))
        if drift:
            print(f"FAIL: regeneration is not a no-op: {', '.join(drift)}")
            return 1
        print(f"ok: regeneration reproduces all {len(before)} files byte for byte")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    # 2. The control: content the generator does not emit must show up as a named diff, so a
    #    green run above means the comparison really compared something.
    scratch = scratch_tree(root)
    try:
        path = os.path.join(scratch, "data", PLANTED_FILE)
        planted = json.load(open(path, encoding="utf-8"))
        victim = planted["materials"][1]["id"]
        del planted["materials"][1]["description_key"]
        with open(path, "w", encoding="utf-8") as f:
            json.dump(planted, f, indent=2, ensure_ascii=False)
            f.write("\n")
        before = snapshot(os.path.join(scratch, "data"))
        run_generator(scratch)
        caught = report(before, snapshot(os.path.join(scratch, "data")))
        if caught != [PLANTED_FILE]:
            print(f"FAIL: the planted hand-edited field on {victim} was not reported "
                  f"(got {caught or 'nothing'})")
            return 1
        print(f"ok: the planted hand-edited field on {victim} is reported")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
