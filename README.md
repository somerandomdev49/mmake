# MMake Build Tool

I got bored with Makefiles and premake/xmake don't have stuff I need, so I made a lua build tool myself.

## Building/Installing
```sh
$ sudo apt-get install liblua5.3-dev # Intall lua dev library
$ git clone https://github.com/somerandomdev49/mmake # Clone the repo
$ cd mmake
$ make # Build mmake (change the Makefile's CXX variable for a different compiler)
$ ./mmake # self-build, check for validity
$ sudo ln -s $(realpath ./mmake) /usr/local/bin/mmake # Optional, install to /usr/local
```

# Using
In your project directory a file called `mmake.lua` should exist for mmake to work. To build, simply run `mmake`.

# TODO:
- Choose which project to build from command line arguments e.g. `mmake -p docs`
- Set the filepath in the command line arguments e.g. `mmake -f ../mmake2.lua`
