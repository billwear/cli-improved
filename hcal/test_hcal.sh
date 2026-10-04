#!/usr/bin/env bash
# test_hcal.sh - Sandbox testing for hcal

echo "Compiling hcal..."
gcc -O2 -Wall -Wextra -o hcal hcal.c || { echo "Compilation failed"; exit 1; }

# Force hcal to use a local sandbox file by hijacking HOME
export HOME="$(pwd)/hcal_sandbox_home"
mkdir -p "$HOME"
rm -f "$HOME/.hcal_data"

# Test 1: Add a rapid inbox task (default priority 4)
./hcal -A "Buy batteries"

# Test 2: Add a GTD Next Action in a specific project/context (priority 2)
./hcal -2 -t -p "HomeRepair" -c "hardware_store" -A "Buy 5/8-inch plywood and mastic"

# Test 3: Add a Habit to track
./hcal -1 -h -A "Drink water (alkaline)"

# Test 4: Agenda Rendering (with colors)
echo -e "\nRunning Default Agenda:"
./hcal -a

# Test 5: Mark the Habit done (checking mutation engine)
./hcal -x 3
echo -e "\nAgenda after checking off Habit (ID 3 should disappear):"
./hcal -a

# Test 6: Project Filtering
echo -e "\nFiltering for Project 'HomeRepair':"
./hcal -p "HomeRepair"

# Test 7: JSON Pipeline output
echo -e "\nJSON Serialization:"
./hcal -j | head -n 12

echo -e "\nTests complete. Clean up sandbox by deleting $HOME"
