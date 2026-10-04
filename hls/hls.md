# hls: The Systems Programmer's Directory Visualizer

`hls` is a modern, systems-level directory browser built for text-as-data workflows and Unix-first environments. Designed to eliminate the friction of piping output through `awk`, `grep`, or `stat`, `hls` brings kernel-level filesystem telemetry, Git awareness, and structured JSON rendering directly into your initial directory interrogation. 

## Display, Traversal, and Layout

By default, `hls` presents a clean, columnar layout that automatically scales to your terminal width. It handles complex directory structures safely, natively trapping infinite symbolic link cycles during deep tree traversals.

| Flag | Function | Corner-Case Behavior & Details |
| :--- | :--- | :--- |
| **`-l`** | Long Format | Renders permission matrices, links, ownership, size, and modification time. Auto-pivots size column to `major, minor` pairs for block/character devices. |
| **`-a`** | All Files | Includes dotfiles (hidden files). |
| **`-H`** | Human Sizes | Scales byte counts to Base-1024 suffixes (B, K, M, G, T, P). |
| **`-F`** | Classify | Appends standard type indicators (`/` dir, `*` exec, `@` symlink, `\|` FIFO, `=` socket). |
| **`-R`** | Recursive | Walks directories deeply, printing each sub-directory block. |
| **`-T`** | Tree View | Renders a hierarchical graph. Safely halts on cyclic symlink loops or `EACCES` blocks. |
| **`-S` / `-t`** | Sorting | Sorts listings by logical size (`-S`) or modification time (`-t`) descending. |
| **`-r`** | Reverse | Inverts the active sort logic (e.g., smallest first, oldest first). |

## Systems Telemetry & Deep Inspection

Where standard utilities require chaining multiple shell tools to audit low-level file states, `hls` interrogates the `stat` struct natively, exposing raw device and inode data for immediate debugging.

| Flag | Function | Corner-Case Behavior & Details |
| :--- | :--- | :--- |
| **`-O`** | Octal Modes | Renders the raw integer permission mask (e.g., `0755`) alongside the human-readable `rwx` string. Forces `-l`. |
| **`-i`** | Inode Tracking | Prepends the `st_ino` hardware index. Crucial for tracing hardlinks across filesystems. |
| **`-s`** | Allocation | Calculates true disk block footprint. Detects sparse files and renders the percentage of physical allocation versus logical size (`[12% sparse]`). |
| **`-n`** | Numeric IDs | Bypasses `/etc/passwd` and `/etc/group` resolution, rendering raw UID/GID integers. |
| **`-I`** | ISO-8601 | Bypasses localized time formatting in favor of strict `YYYY-MM-DD HH:MM:SS` standard output. |

## Extended Metadata & Analysis

File extensions are arbitrary. `hls` can look past the filename and interrogate the underlying bytes and filesystem metadata to tell you exactly what a file is and how the OS treats it.

| Flag | Function | Corner-Case Behavior & Details |
| :--- | :--- | :--- |
| **`-M`** | Magic Bytes | Opens the file and sniffs the first 32 bytes to identify the true payload (e.g., "ELF binary", "Mach-O 64-bit", "SQLite database", "PDF document"). Identifies spoofed extensions. Forces `-l`. |
| **`-@`** | Extended Attrs | Enumerates macOS and Linux `xattr` keys attached to the file, printing them nested beneath the file entry. |

## Context Filtering & Version Control

To support automated pipelines and developer environments, `hls` integrates data-filtering and serialization natively, preventing the data loss that occurs when parsing ANSI-colored text output.

| Flag | Function | Corner-Case Behavior & Details |
| :--- | :--- | :--- |
| **`-E`** | Regex Filter | Takes a POSIX extended regular expression (`-E "\.c$"`) and filters files *before* sorting or rendering. Far more efficient than `hls | grep`. |
| **`-G`** | Git Status | Interrogates the working tree cache via asynchronous batching. Prepends standard porcelain status codes (`M `, `??`, `A `) to files tracked or ignored by Git. |
| **`-j`** | JSON Output | Bypasses all visual rendering flags to emit a strictly typed JSON array. Preserves booleans for symlink states and integers for bytes/timestamps. |

---

**Common Workflows**

**The Full Context View**  
Retrieve a visual tree of your repository, complete with human-readable sizes and Git modification badges:
```bash
hls -TGH
```

**Security & Binary Audit**  
Examine raw octal permissions, check sparse allocations, and sniff the magic bytes of binaries in a directory:
```bash
hls -lOMs /usr/local/bin
```

**Toolchain Ingestion**  
Extract all C header files as a structured JSON array, ready to be passed to `jq` or Python without string-parsing fragile terminal output:
```bash
hls -j -E "\.h$" ./src
```
