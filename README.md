# CurseMiner

## Overview
CurseMiner is a game engine prototype for creating 2D tile-based games. At its core, it features a job scheduler, timer, and custom data structures, aimed at facilitating efficient game mechanics.

## Components
### Stack64
Contains all the basic datastructures used in this project; list, stack, queue and a str-int hashtable. All datastructures store a 64-bit unsigned integer allowing for storing either numbers or pointers to other data.

### Timer
Updates wall time every tick and ensures program doesn't run faster than allowed. It provides useful time-related typedefs and allows for time checks with functions such as timer_diff_milisec(), timer_ready() and variables like INIT_TIME, TIMER_NOW and TIMER_NEVER.

### Scheduler
A simple task scheduler with the following architecture:

**RunQueue**: Tasks are stored on a RunQueue (RQ) and are executed in sequence. Once an RQ execution starts it always completes in full.

**RunQueueList**: RQs are stored on a linked list called RunQueueList (RQLL). If the current runqueue has taken more time than allowed per frame then execution terminates. Otherwise continue executing the next RQ in RQLL.

**List**: All RQLLs are also stored on a generic linked list. In the future each RQLL will run on a seperate thread, however this is not yet implemented.

Overall this design heavily discourages threading and each task is assumed to execute quickly through simple code and asynchronous system calls. This has the benefit of avoiding overhead and race conditions.

### Files
An interface for asynchronous file IO. Current implementation uses aio.h, so IO operations are not truly asynchronous. This component is not yet used anywhere in the project.

### Keyboard
Defines keymaps, polls ncurses getch() and tracks keyboard state. In the future keyboard polling will be independent of frameworks like ncurses.

### UI
Provides a basic ncurses interface for drawing game and widget windows. a window manager for defining new windows.

### Game
Core game component. Tracks entities and provides a framework-agnostic way for querying game world data. The game world to be displayed is available through game_world_getxy(). This function returns game the tile/entity ID at specified screen (x,y) coordinates.

### World
Manages infinite world generation. World is segmented into chunks that are allocated on a list of memory arenas. Each chunk is a node in the world graph and extra work must be done to give the illusion of a cartesian coordinate system. This was a design decision and will allow for interesting functionality in the future, such as traversal-based rendering and chunk mapping/swapping. Function world_getxy() allows for efficient hash-based lookups using Cantor's pairing function for hashing. Function world_getxy() works on-demand, if no chunk exists at position (x,y) then it is dynamically inserted into the world graph.

## Requirements
- Any Linux OS: at the moment compiling this project requires unistd.h header which are only available on linux.
- 64-bit CPU: while the program will compile and run on a 32-bit architecture, this is not technically supported and all core components assume 64-bit pointers.

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
