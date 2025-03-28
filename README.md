# CurseMiner

## Overview
CurseMiner is a lightweight execution framework for running multiple tasks concurrently. It also provides a bare-minimum 2D grid-based game engine and a procedural world generator.

## Components
### Stack64
Contains all the basic datastructures used in this project; list, stack, queue and a str-int hashtable. All datastructures store a 64-bit unsigned integer allowing for storing either numbers or pointers to other data.

### Timer
An interface for time-related operations. Provides comparison functions and useful global variables such as INIT\_TIME, TIMER\_NOW and TIMER\_NEVER.

### Scheduler
A simple task scheduler with the following architecture:

**RunQueue**: Tasks are stored on a RunQueue (RQ) and are executed in sequence. Once an RQ execution starts it always completes in full.

**RunQueueList**: RQs are stored on a linked list called RunQueueList (RQLL). If the current runqueue has taken more time than allowed per frame then execution terminates. Otherwise continue executing the next RQ in RQLL.

**List**: All RQLLs are also stored on a generic linked list. In the original design notes each RQLL spun it's own thread, but this has not been implemented as the system's lightweight nature reduces need for parallelism.

Overall this design heavily discourages threading (once implemented) and each task is assumed to execute quickly through simple code and asynchronous system calls. This has the benefit of avoiding overhead and race conditions.

As of the heavy optimizations release sleeping tasks are stored on a minheap, taking almost cpu.

### Files
An interface for asynchronous file IO. Current implementation uses aio.h so IO operations are not truly asynchronous. This component is an artifact from the very first development stage and is not yet used anywhere in the project.

### Input
A framework-agnostic event-driven input handler. It provides a universal generic interface for interacting with input devices regardless of actual implementation. The design is loosely based on hardware drivers employed by operating systems. Currently only supports ncurses mode, however SDL2 mode is upcoming along with a pure hardware GPIO interface from ESPIDF framework for embedded devices.

### UI
Provides a basic ncurses interface for drawing game and widget windows. Also features a window manager for easy window creation and management. Each window stores draw calls on a queue so a window is not limited to displaying only one view.

### Game
Core game component. Tracks entities and provides a framework-agnostic way for querying game world data. The game world to be displayed is available through game\_world\_getxy(). This function returns game the tile/entity ID at specified screen (x,y) coordinates. This means the game doesn't know it's being rendered giving amazing flexibility with regards to UI frontends. Entities are stored on a minheap and use no CPU most of the time. Entity tick rate is determined by attribute Entity.speed. The attibute works such that Entity.speed=100 means the entity will tick once per second. This value can be scaled.

### World
Manages infinite world generation. World is segmented into chunks that are allocated on a list of memory arenas. Each chunk is a node in the world graph and extra work must be done to give the illusion of a cartesian coordinate system. This was a design decision and will allow for interesting functionality in the future, such as traversal-based rendering and chunk mapping/swapping. Function world\_getxy() allows for efficient hash-based lookups using Cantor's pairing function for hashing. Function world\_getxy() works on-demand, if no chunk exists at position (x,y) then it is dynamically inserted into the world graph. While this implementation is slow, it has no real effect on actual performance as new chunks are inserted very rarely.

## Requirements
- Any Linux OS: at the moment compiling this project requires unistd.h header which are only available on linux.
- 64-bit CPU: while the program will compile and run on a 32-bit architecture, this is not intended use as all core components assume 64-bit pointers.

## Installation
Clone the repository and compile the source using:
```
$ bash
$ git clone https://github.com/djabko/curseminer.git
$ cd curseminer
$ make && ./curseminer
```

## License
Distributed under the GPL 2.0 License. See LICENSE for more information.

## Contact
Email: daniel@djabko.com
