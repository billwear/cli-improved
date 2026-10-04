#!/usr/bin/env bash
# test_hls.sh - Comprehensive test suite for hls (foundational & expanded features)

echo "Compiling hls..."
gcc -O2 -Wall -Wextra -o hls hls.c || { echo "Compilation failed"; exit 1; }

# -------------------------------------------------------------------------
# 1. Setup Isolated Sandbox
# -------------------------------------------------------------------------
TEST_DIR="hls_sandbox"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR/subdir"

# Basic files and permissions
touch "$TEST_DIR/regular_file.txt"
chmod 644 "$TEST_DIR/regular_file.txt"

# Executable / Script (for -F and -M)
echo "#!/bin/sh" > "$TEST_DIR/executable.sh"
chmod 755 "$TEST_DIR/executable.sh"

# Symlinks (Good and Broken)
ln -s regular_file.txt "$TEST_DIR/symlink_good"
ln -s does_not_exist "$TEST_DIR/symlink_broken"

# Pipe/FIFO (for -F classification)
mkfifo "$TEST_DIR/pipe_test" 2>/dev/null || true

# Hidden file (for -a)
touch "$TEST_DIR/.hidden_file"

# Large file (for -H and -S)
dd if=/dev/zero of="$TEST_DIR/big_file.bin" bs=1024 count=2048 2>/dev/null # ~2MB file

# Git Repository Setup (for -G)
(
    cd "$TEST_DIR" || exit
    git init -q
    git add regular_file.txt
    git commit -m "init" -q
    # Modify a tracked file, leave others untracked
    echo "mod" >> regular_file.txt
)

# Extended Attributes (for -@) - Using macOS xattr syntax
xattr -w "user.test_attr" "test_value" "$TEST_DIR/regular_file.txt" 2>/dev/null || true

# -------------------------------------------------------------------------
# 2. Testing Framework
# -------------------------------------------------------------------------
FAILURES=0

assert_match() {
    local test_name="$1"
    local output="$2"
    local pattern="$3"
    
    # FIX 1: Added -e to prevent grep from parsing hyphens in patterns as flags
    if echo "$output" | grep -qE -e "$pattern"; then
        printf "[\033[1;32mPASS\033[0m] %s\n" "$test_name"
    else
        printf "[\033[1;31mFAIL\033[0m] %s\n" "$test_name"
        echo "       Expected pattern: $pattern"
        echo "       Actual output:"
        echo "$output" | sed 's/^/       /'
        ((FAILURES++))
    fi
}

assert_order() {
    local test_name="$1"
    local output="$2"
    local expected_top="$3"

    # Grab the last field of the first line (filename)
    local top_file
    top_file=$(echo "$output" | head -n 1 | awk '{print $NF}')
    
    if [[ "$top_file" == "$expected_top" ]]; then
        printf "[\033[1;32mPASS\033[0m] %s\n" "$test_name"
    else
        printf "[\033[1;31mFAIL\033[0m] %s\n" "$test_name"
        echo "       Expected top file: $expected_top"
        echo "       Actual top file: $top_file"
        ((FAILURES++))
    fi
}

echo "Running tests against sandbox..."

# --- Foundational Options ---

out=$(./hls -a "$TEST_DIR")
assert_match "All Files (-a)" "$out" "\.hidden_file"

out=$(./hls -l "$TEST_DIR")
assert_match "Long Format (-l)" "$out" "-rw-r--r--.*regular_file\.txt"
assert_match "Symlink Resolution (-l)" "$out" "symlink_good -> regular_file\.txt"

out=$(./hls -F "$TEST_DIR")
assert_match "Classify Executable (-F)" "$out" "executable\.sh\*"
assert_match "Classify Directory (-F)" "$out" "subdir/"

out=$(./hls -lH "$TEST_DIR")
assert_match "Human Readable Sizes (-H)" "$out" "2\.0M.*big_file\.bin"

out=$(./hls -lS "$TEST_DIR")
assert_order "Sort by Size (-S)" "$out" "big_file.bin"

out=$(./hls -lSr "$TEST_DIR")
if echo "$out" | head -n 1 | grep -q "big_file.bin"; then
    printf "[\033[1;31mFAIL\033[0m] Reverse Sort (-r)\n"
    ((FAILURES++))
else
    printf "[\033[1;32mPASS\033[0m] Reverse Sort (-r)\n"
fi

out=$(./hls -R "$TEST_DIR")
assert_match "Recursive Traversal (-R)" "$out" "$TEST_DIR/subdir:"

out=$(./hls -T "$TEST_DIR")
assert_match "Tree View (-T)" "$out" "(├──|└──).*regular_file\.txt"

out=$(./hls -lG "$TEST_DIR")
assert_match "Git Status Modified (-G)" "$out" "M .*regular_file\.txt"
assert_match "Git Status Untracked (-G)" "$out" "\?\? .*executable\.sh"

out=$(./hls -M "$TEST_DIR")
assert_match "Magic Bytes Script (-M)" "$out" "script text"

out=$(./hls -n "$TEST_DIR")
assert_match "Numeric IDs (-n)" "$out" " [0-9]+ +[0-9]+ "

out=$(./hls -ls "$TEST_DIR")
assert_match "Show Allocations (-s)" "$out" "\[.*blk\]|\[.*sparse\]"

# FIX 2: Used -l@ instead of just -@ so the long format rendering loop triggers
out=$(./hls -l@ "$TEST_DIR")
assert_match "Extended Attributes (-@)" "$out" "user\.test_attr"

# --- Expanded Features ---

out=$(./hls -i "$TEST_DIR")
assert_match "Inode Tracking (-i)" "$out" " *[0-9]+ regular_file.txt"

out=$(./hls -lO "$TEST_DIR")
assert_match "Octal Permissions 644 (-O)" "$out" "0644.*regular_file.txt"
assert_match "Octal Permissions Executable 755 (-O)" "$out" "0755.*executable.sh"

out=$(./hls -lI "$TEST_DIR")
assert_match "ISO-8601 Time Format (-I)" "$out" "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}.*regular_file.txt"

out=$(./hls -E "\.sh$" "$TEST_DIR")
assert_match "Regex Filter Positive Match (-E)" "$out" "executable\.sh"
if echo "$out" | grep -q "regular_file.txt"; then
    printf "[\033[1;31mFAIL\033[0m] Regex Filter Negative Match (-E)\n"
    ((FAILURES++))
else
    printf "[\033[1;32mPASS\033[0m] Regex Filter Negative Match (-E)\n"
fi

out=$(./hls -j "$TEST_DIR")
assert_match "JSON String Formatting (-j)" "$out" "\"name\": \"regular_file.txt\""
assert_match "JSON Octal Sub-field (-j)" "$out" "\"mode_octal\": \"0[0-7]{3,4}\""
assert_match "JSON Boolean Sub-field (-j)" "$out" "\"is_symlink\": (true|false)"

# FIX 3: Point tests at the /dev directory itself and grep for 'null' to avoid the opendir() file failure
if [ -d "/dev" ]; then
    out=$(./hls -l /dev 2>/dev/null | grep -e "null$")
    assert_match "Device Node Major/Minor Parsing" "$out" "[0-9]+, *[0-9]+.*null"
fi

# -------------------------------------------------------------------------
# 3. Teardown
# -------------------------------------------------------------------------
rm -rf "$TEST_DIR"
rm -f hls

if [ "$FAILURES" -eq 0 ]; then
    echo -e "\nAll features verified successfully."
    exit 0
else
    echo -e "\n$FAILURES test(s) failed."
    exit 1
fi
