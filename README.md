# CFAT File System

## Purpose

The CFAT File System is a custom file system based on the FAT32 specification implemented using FUSE (Filesystem in Userspace). It aims to provide a simple yet functional file system for educational purposes. This file system supports basic file operations such as creating, reading, writing, and deleting files and directories. It also includes an interactive shell for easy manipulation and management of the file system. This has been created for the CS314 class, project 1 assignment.

## Features

- Create and manage files and directories.
- Support for reading, writing, and listing directory contents.
- File extraction to the local filesystem.
- Interactive shell for direct file system manipulation.
- Mount the custom file system to a directory.

## Requirements

- **Libraries**:
  - FUSE (`libfuse-dev`)
  - pkg-config
- **Compiler**:
  - GCC or any compatible C compiler

### Installation of Requirements

On a Debian-based system, you can install the required library using:

```sh
sudo apt-get install libfuse-dev pkg-config
```

## Compilation

### Using Make

A `Makefile` is provided for your convenience. To compile the project, simply run:

```sh
make
```

### Using GCC

Alternatively, you can compile the program manually with:

```sh
gcc cfs.c -o cfs `pkg-config fuse --cflags --libs` --libs`
```

## Usage Instructions

### Running the Program

The program can be run with various options. Below are some common commands:

- **Create a new file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -c
  ```

- **Load an existing file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT
  ```

- **List the contents of the file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -l
  ```

- **Add a file to the file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -a myfile.txt -i /myfolder/
  ```

- **Add a directory to the file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -d /myfolder
  ```

- **Remove a file or directory from the file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -r /myfolder/myfile.txt
  ```

- **Extract a file from the file system**:
  ```sh
  ./cfs -f myfilesystem.CFAT -e /myfolder/myfile.txt
  ```

- **Mount the file system to a directory**:
  ```sh
  ./cfs -f myfilesystem.CFAT -m /mnt/myfilesystem
  ```

- **Launch interactive mode**:
  ```sh
  ./cfs -f myfilesystem.CFAT -I
  ```

### Interactive Shell

Once in the interactive shell, you can use the following commands:

- `help` - Show available commands.
- `exit` - Exit the shell.
- `ls` - List the contents of the current directory.
- `cd <internal path>` - Change the current directory.
- `cat <internal path>` - Display the contents of a file.
- `rm <internal path>` - Remove a file or directory.
- `mkdir <internal path>` - Add a directory.
- `tree` - Display the directory tree.
- `addfile <path> <internal path>` - Add a file.
- `touch <internal path>` - Create/update the timestamp of a file.
- `extract <internal path>` - Extract a file.
- `createfs <fsname>` - Create a new file system.
- `loadfs <fsname>` - Load a file system.
- `mount <mountpath>` - Mount the file system at the specified point.

## Potential Problems

- **Fle Name Limits**: As this is based on the FAT32 spec, filenames are limited in size to 11 characters, including extension.
- **File Size Limits**: Reading large files (>131KB) may have undocumented behavior.
- **Stability**: As this is an educational project, some edge cases might not be handled perfectly.
- **Mounting Issues**: Ensure that the mount path exists and you have the necessary permissions.

## Author

**Charlie Whittleman**
Email: [brennwh@siue.edu](mailto:brennwh@siue.edu)

## License

This project is licensed under the MIT License. See the LICENSE file for more details.
