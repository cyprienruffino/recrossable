#!/usr/bin/env python3
"""Builds the per-difficulty word lists from the xd crossword corpus.

Download https://xd.saul.pw/xd-clues.zip, then run:
    python3 tools/build_wordlists.py xd-clues.zip wordlists/

Difficulty is approximated by publication: USA Today and Universal run easy
puzzles, the LA Times and Newsday medium ones, the NYT and WSJ hard ones.
Each answer gets the clue most often used for it in that difficulty's
publications, which tends to be the most straightforward one.
"""

import collections
import io
import re
import sys
import zipfile

LEVELS = {
    "easy": {"usa", "up"},
    "normal": {"lat", "nw", "cs"},
    "hard": {"nyt", "wsj"},
}
MIN_YEAR = 2000
# Answers must have been used this often; higher keeps rare fill out of easy puzzles
MIN_USES = {"easy": 6, "normal": 3, "hard": 2}

ANSWER = re.compile(r"^[A-Z]{3,15}$")
# Clues that only make sense inside their original puzzle
CROSS_REFERENCE = re.compile(r"\d+-(Across|Down)|\b(Across|Down)\b|^See |starred|\*|circled|shaded|theme", re.IGNORECASE)


def main(zip_path, output_dir):
    clues = {level: collections.defaultdict(collections.Counter) for level in LEVELS}
    level_of = {pub: level for level, pubs in LEVELS.items() for pub in pubs}

    with zipfile.ZipFile(zip_path) as archive:
        with io.TextIOWrapper(archive.open("xd/clues.tsv"), encoding="utf-8", errors="replace") as tsv:
            next(tsv)
            for line in tsv:
                parts = line.rstrip("\n").split("\t")
                if len(parts) != 4:
                    continue
                pub, year, answer, clue = parts
                level = level_of.get(pub)
                if level is None or not year.isdigit() or int(year) < MIN_YEAR:
                    continue
                answer = answer.upper()
                clue = clue.strip()
                if not ANSWER.match(answer) or not clue or "\t" in clue or CROSS_REFERENCE.search(clue):
                    continue
                clues[level][answer][clue] += 1

    for level, answers in clues.items():
        path = f"{output_dir}/{level}.tsv"
        count = 0
        with open(path, "w", encoding="utf-8") as out:
            for answer in sorted(answers):
                usages = answers[answer]
                if sum(usages.values()) < MIN_USES[level]:
                    continue
                clue = max(usages, key=lambda c: (usages[c], -len(c)))
                out.write(f"{clue}\t{answer}\n")
                count += 1
        print(f"{path}: {count} answers")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
