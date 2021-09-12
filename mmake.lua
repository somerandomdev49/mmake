local OBJS = { "build/main.o" }

--------------------------------
project "MMake"
	lang "c"
	defines { "TEST_DEFINE" }
	includes { "include/" }
	sources { "src/**.c" }
	libraries { "lua5.3", "dl", "m" }
	compiler "gcc-10"
	standard "c11"
	build_dir "build"
	bin_dir "."
	bin "mmake"
--------------------------------


