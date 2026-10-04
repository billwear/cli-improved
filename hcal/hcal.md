# hcal: The Hacker's Calendar & Productivity Engine

`hcal` is a zero-dependency, C-based monolithic productivity system designed to outperform bloated desktop clients and mobile apps natively from the command line. It unifies Getting Things Done (GTD), timeblocking, habit tracking, and scheduling into a strict text-as-data pipeline.

## The Architecture (Text-as-Data)

`hcal` operates entirely on a structured flat file (`~/.hcal_data`). There are no SQLite dependencies, daemon processes, or proprietary blobs. You can natively pipe, `grep`, or `sed` your entire life schedule, or export it instantaneously via `-j` to integrate with `jq`, Neovim dashboards, or custom shell prompt wrappers.

## Core Feature Set

### 1. Unified Capture (GTD)
Add tasks effortlessly. Define the item type, set priorities (1-4), and categorize by project and context simultaneously.
```bash
# Rapid inbox capture (defaults to Priority 4 / Someday)
hcal -A "File taxes"

# Add a high-priority (1) Next Action Task (t) with metadata
hcal -1 -t -p "Finances" -c "computer" -A "Complete W2 forms"

# Add a daily Habit (h)
hcal -2 -h -A "Read 10 pages"
