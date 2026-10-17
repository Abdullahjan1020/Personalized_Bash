# 🐚 Personalized Bash (C++)

A minimal Unix shell built from scratch in C++. It's a small version of the command-line program you use every day in a Linux or macOS terminal — type a command, press Enter, and it runs.

## What it can do

- **Run commands** like `ls`, `pwd`, `echo hello`, and any other program on your system
- **Pipes (`|`)** — send the output of one command into another, e.g. `ls | grep txt`
- **Redirect input and output**
  - `>` saves output into a file (overwrites it)
  - `>>` adds output to the end of a file
  - `<` reads input from a file
- **Background jobs (`&`)** — run a command in the background so you can keep typing, e.g. `sleep 10 &`
- **Built-in commands** — `cd` to change folders and `exit` to leave the shell
- **Ctrl+C handling** — stops a running command without closing the shell itself

## Quick examples

```bash
ls > files.txt            # save the file list into files.txt
cat files.txt | wc -l     # count how many lines it has
sleep 5 &                 # run in the background
cd ..                     # go up one folder
exit                      # leave the shell
```

## Files in this project

| File | What it does |
|------|---------------|
| `main.cpp` | The whole shell — reads your commands and runs them |

## How to run it

1. Make sure you're on Linux or macOS (or WSL on Windows) with a C++ compiler like `g++` installed
2. Download or clone this repository
3. Open a terminal in the project folder and compile it:

```bash
g++ main.cpp -o shell
```

4. Start your shell:

```bash
./shell
```

## Requirements

- Linux, macOS, or WSL (it uses Unix system features, so it won't run natively on Windows)
- A C++ compiler such as `g++`

## About

This project was built to understand what really happens when you type a command in a terminal. It uses the same core building blocks real shells are built on — creating new processes, connecting them together, and redirecting where their input and output go.
