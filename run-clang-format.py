import subprocess
import sys

for file in sys.argv[1:]:
    print(f"Formatting {file}")
    subprocess.run(["clang-format", "-i", file])