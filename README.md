# MMake Build Tool

I got bored with Makefiles and premake/xmake don't have stuff I need, so I made a lua build tool myself.

> Currently doesn't work on Windows, but everything except for `glob` should be easy to port.

## Building/Installing
```sh
$ sudo apt-get install liblua5.3-dev # intall the lua development library
$ git clone https://github.com/somerandomdev49/mmake # clone the repo
$ cd mmake
$ make # build mmake (change the Makefile's CXX variable for a different compiler)
$ ./mmake # self-build, check for validity
$ sudo ln -s $(realpath ./mmake) /usr/local/bin/mmake # optional, install to /usr/local
```

# Using
In your project directory a file called `mmake.lua` should exist for mmake to work. To build, simply run `mmake`.

Example `mmake.lua` for some project you might have
```lua
project "My Cool Game"               -- project name
  lang "c"                           -- doesn't do anything for now (TODO: change default compiler based on lang)
  defines { "NTHREADS=4", "DEBUG" }  -- defines DEBUG and NTHREADS
  includes { "include/" }            -- included directories
  sources { "src/**.c" }             -- source files
  libraries { "glfw", "dl", "m" }    -- linked libraries (TODO: frameworks)
  compiler "gcc"                     -- the compiler to use
  standard "c11"                     -- -std=c11
  build_dir "build"                  -- all the .o files go into build/
  bin_dir "bin"                      -- the binary goes in the bin/ folder
  bin "mycoolgame"                   -- binary, bin/mycoolgame

project "My Cool Game II" -- another project
  lang "cpp"
  defines { "NTHREADS=8" }
  includes { "include", "newinclude" }
  sources { "v2/src/**.cpp", "src/keymapper.c" }
  libraries { "glfw", "spdlog" }
  compiler "g++-10"
  standard "c++20"
  build_dir "build/v2"
  bin_dir "bin/v2"
  bin "mcg2"
```

# TODO:
- Choose which project to build from command line arguments e.g. `mmake -p docs`
- Set the filepath in the command line arguments e.g. `mmake -f ../mmake2.lua`
