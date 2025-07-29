# Connect 4 — Kernel Module Character Device Driver

This project focuses on kernel modules and character device drivers. The module implements a Connect 4 game that can be dynamically inserted and removed from a running Linux kernel. Operating in kernel space allows the module to execute tasks with high privileges and at a low level. The assignment emphasizes the application of kernel modules for implementing a device driver.

- **Kernel Module**: Dynamically insertable and removable  
- **Character Device Driver**: Creates a device node in `/dev`  
- **Game Logic**: Implements a fully functional Connect 4 game  
- **I/O Operations**: Uses `echo` for input and `cat` for output via `read()`  
- **Boolean Flags**: Used internally to track game prompts and responses  

## How to Build?

The project uses standard C and Linux libraries. A provided `Makefile` handles compilation, producing a `.ko` kernel object file.

```bash
# Step 1: Compile the module
make

# Step 2: Insert the module into the kernel
sudo insmod fourinarow.ko

# Step 3: Interact with the module
# Send commands using echo:
echo "<command>" > /dev/fourinarow

# View output using cat:
cat /dev/fourinarow

# Step 4: Remove the module when done
sudo rmmod fourinarow
