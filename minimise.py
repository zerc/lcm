import re


def main(filename: str, target: str):
    with open(filename, 'r') as source:
        content = source.read()

    content = re.sub(r'\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'\s+', ' ', content)
    content = re.sub(r'"', '\\"', content)

    content = f"""
#include <aWOT.h>

P(index) = "{content}";
""".strip()

    with open(target, 'w') as f:
        f.write(content)


if __name__ == "__main__":
    main("index.html", "index.cpp")