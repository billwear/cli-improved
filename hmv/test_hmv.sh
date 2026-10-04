#!/usr/bin/env bash
# test_hmv.sh - Comprehensive test suite for hmv

# -------------------------------------------------------------------------
# 1. Compilation Check
# -------------------------------------------------------------------------
if [ ! -x "./hmv" ]; then
    echo "Compiling hmv..."
    make hmv || { echo "Compilation failed"; exit 1; }
fi

# -------------------------------------------------------------------------
# 2. Setup Isolated Sandbox
# -------------------------------------------------------------------------
TEST_DIR="hmv_sandbox"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR/src_dir"
mkdir -p "$TEST_DIR/dest_dir"

# Basic files
touch "$TEST_DIR/src_dir/file1.txt"
touch "$TEST_DIR/src_dir/file2.txt"
touch "$TEST_DIR/src_dir/rename_test.c"

# Set up timeline correctly for Update and Collision tests
echo "old collision data" > "$TEST_DIR/dest_dir/collision.txt"
echo "older data" > "$TEST_DIR/src_dir/older_src.txt"

# Force filesystem clock tick
sleep 1

echo "new collision data" > "$TEST_DIR/src_dir/newer_src.txt"
echo "newer data" > "$TEST_DIR/dest_dir/newer_target.txt"

# Git Repository Setup (for -G)
(
    cd "$TEST_DIR/src_dir" || exit
    git init -q
    git add file1.txt
    git commit -m "init" -q
)

# -------------------------------------------------------------------------
# 3. Testing Framework
# -------------------------------------------------------------------------
FAILURES=0

assert_match() {
    local test_name="$1"
    local output="$2"
    local pattern="$3"
    
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

assert_exists() {
    local test_name="$1"
    local filepath="$2"
    local should_exist="$3"

    if [ -e "$filepath" ] && [ "$should_exist" == "true" ]; then
        printf "[\033[1;32mPASS\033[0m] %s\n" "$test_name"
    elif [ ! -e "$filepath" ] && [ "$should_exist" == "false" ]; then
        printf "[\033[1;32mPASS\033[0m] %s\n" "$test_name"
    else
        printf "[\033[1;31mFAIL\033[0m] %s\n" "$test_name"
        echo "       File state mismatch: $filepath"
        ((FAILURES++))
    fi
}

echo "Running tests against sandbox..."

# Test 1: Dry Run (-d)
out=$(./hmv -d "$TEST_DIR/src_dir/file2.txt" "$TEST_DIR/dest_dir")
assert_match "Dry Run Output (-d)" "$out" "rename.*file2\.txt -> .*dest_dir/file2\.txt.*dry-run"
assert_exists "Dry Run Safety (Target missing)" "$TEST_DIR/dest_dir/file2.txt" "false"
assert_exists "Dry Run Safety (Source remains)" "$TEST_DIR/src_dir/file2.txt" "true"

# Test 2: Substring Rename in Transit (-S and -R)
out=$(./hmv -v -S ".c" -R ".o" "$TEST_DIR/src_dir/rename_test.c" "$TEST_DIR/dest_dir")
assert_match "Substring Rename Stdout (-S / -R)" "$out" "rename_test\.c -> .*dest_dir/rename_test\.o"
assert_exists "Substring Rename (Target created)" "$TEST_DIR/dest_dir/rename_test.o" "true"
assert_exists "Substring Rename (Source removed)" "$TEST_DIR/src_dir/rename_test.c" "false"

# Test 3: Standard Move Validation
out=$(./hmv -v "$TEST_DIR/src_dir/file2.txt" "$TEST_DIR/dest_dir")
assert_match "Standard Rename Output" "$out" "rename.*file2\.txt -> .*dest_dir/file2\.txt"
assert_exists "Standard Rename (Target created)" "$TEST_DIR/dest_dir/file2.txt" "true"

# Test 4: Git-Aware Move (-G) 
# CRITICAL TEST FIX: We must run hmv from INSIDE the git repository
out=$(cd "$TEST_DIR/src_dir" && ../../hmv -vG file1.txt file1_moved.txt)
assert_match "Git-Aware Invocation (-G)" "$out" "git mv.*file1\.txt -> .*file1_moved\.txt"
git_status=$(cd "$TEST_DIR/src_dir" && git status --porcelain=v1)
assert_match "Git Index Validated" "$git_status" "R  file1\.txt -> file1_moved\.txt"

# Test 5: Smart Backups (-b)
out=$(./hmv -vb "$TEST_DIR/src_dir/newer_src.txt" "$TEST_DIR/dest_dir/collision.txt")
assert_match "Smart Backups Output (-b)" "$out" "backup.*collision\.txt -> .*collision\.txt\.~[0-9]{4}-[0-9]{2}-[0-9]{2}T"
backup_file=$(ls "$TEST_DIR/dest_dir/collision.txt."~* 2>/dev/null | head -n 1)
if [ -n "$backup_file" ]; then
    printf "[\033[1;32mPASS\033[0m] Smart Backups (Backup file exists)\n"
else
    printf "[\033[1;31mFAIL\033[0m] Smart Backups (Backup file exists)\n"
    ((FAILURES++))
fi

# Test 6: Update Only (-u)
out=$(./hmv -vu "$TEST_DIR/src_dir/older_src.txt" "$TEST_DIR/dest_dir/newer_target.txt")
assert_match "Update Skip Output (-u)" "$out" "skip.*older_src\.txt -> .*newer_target\.txt.*destination newer"
assert_exists "Update Skip Safety (Source remains)" "$TEST_DIR/src_dir/older_src.txt" "true"

# Test 7: JSON Output Format (-j)
touch "$TEST_DIR/src_dir/json_test.txt"
out=$(./hmv -j "$TEST_DIR/src_dir/json_test.txt" "$TEST_DIR/dest_dir")
assert_match "JSON Key/Value serialization (-j)" "$out" "\"status\": \"ok\""

# -------------------------------------------------------------------------
# 4. Teardown
# -------------------------------------------------------------------------
rm -rf "$TEST_DIR"

if [ "$FAILURES" -eq 0 ]; then
    echo -e "\nAll hmv features verified successfully."
    exit 0
else
    echo -e "\n$FAILURES test(s) failed."
    exit 1
fi
