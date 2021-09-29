#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <tinydir.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lualib.h>
#include <lua5.3/lua.h>

// TODO: Windows/Non-Unix or create wrapper.
#ifndef WIN32
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#else
#include <wrap_win.h>
#endif

bool FORCE_REBUILD = false;

char *cpmstr_n(const char *c, size_t n)
{
	char *s = malloc(n + 1);
	s[n] = 0;
	memcpy(s, c, n + 1);
	return s;
}

char *cpmstr(const char *c) { return cpmstr_n(c, strlen(c)); }

//><><><><><><>< DYNAMIC ARRAY ><><><><><><><><><><><><><><><><><><//
typedef struct
{
	size_t length, reserved, size;
	void **data;
} Dyna;

void Dyna_newr(Dyna *dyna, size_t reserved)
{
	dyna->reserved = reserved;
	dyna->length = 0;
	dyna->size = 0;
	dyna->data = NULL;
}

void Dyna_new(Dyna *dyna) { Dyna_newr(dyna, 0); }
size_t Dyna_reserve(Dyna *dyna, size_t res)
{
	dyna->reserved += res;
	dyna->size = dyna->reserved * sizeof(void*);
	if(dyna->data == NULL)
		dyna->data = malloc(dyna->size);
	else
		dyna->data = realloc(dyna->data, dyna->size);
	return dyna->reserved;
}

void Dyna_push(Dyna *dyna, void *val)
{
	if(dyna->reserved != 0)
	{
		dyna->data[dyna->length++] = val;
		--dyna->reserved;
	}
	else
	{
		if(dyna->data == NULL)
			dyna->data = malloc(dyna->size = 1 * sizeof(void*));
		else
			dyna->data = realloc(dyna->data, dyna->size = (dyna->length+1) * sizeof(void*));

		dyna->data[dyna->length++] = val;
	}
}

typedef void (*DynaFunc)(void*);
void Dyna_foreach(Dyna *dyna, DynaFunc f)
{
	for(size_t i = 0; i < dyna->length; ++i)
		f(dyna->data[i]);
}

void Dyna_free(Dyna *dyna) { free(dyna->data); Dyna_new(dyna); }

#define List(T) Dyna

//><><><><><><>< DYNAMIC STRING ><><><><><><><><><><><><><><><><><><//

typedef struct
{
	size_t length, reserved, size;
	char *data;
} Dyns;

void Dyns_newr(Dyns *dyna, size_t reserved)
{
	dyna->reserved = reserved;
	dyna->length = 0;
	dyna->size = 0;
	dyna->data = NULL;
}

void Dyns_new(Dyns *dyna) { Dyns_newr(dyna, 0); }
size_t Dyns_reserve(Dyns *dyna, size_t res)
{
	dyna->reserved += res + 1;
	if(dyna->size == 0) dyna->size = 1 * sizeof(char); // sizeof(char) is for clarity.
	dyna->size += dyna->reserved * sizeof(char);
	if(dyna->data == NULL)
		dyna->data = malloc(dyna->size);
	else
		dyna->data = realloc(dyna->data, dyna->size);
	return dyna->reserved;
}

void Dyns_push(Dyns *dyna, char val)
{
	if(dyna->reserved != 0)
	{
		dyna->data[dyna->length++] = val;
		dyna->data[dyna->length] = '\0';
		--dyna->reserved;
	}
	else
	{
		if(dyna->size == 0) dyna->size = 1 * sizeof(char);
		if(dyna->data == NULL)
			dyna->data = malloc(dyna->size += 1 * sizeof(char));
		else
			dyna->data = realloc(dyna->data, dyna->size += 1 * sizeof(char));

		dyna->data[dyna->length++] = val;
		dyna->data[dyna->length] = '\0';
	}
}

void Dyns_concat(Dyns *dyna, const char *vals)
{
	Dyns_reserve(dyna, strlen(vals));
	while(*vals) Dyns_push(dyna, *vals++);
}

typedef void (*DynsFunc)(char);
void Dyns_foreach(Dyns *dyna, DynsFunc f)
{
	for(size_t i = 0; i < dyna->length; ++i)
		f(dyna->data[i]);
}

void Dyns_free(Dyns *dyna) { free(dyna->data); Dyns_new(dyna); }

//><><><><><><>< DATA TYPES ><><><><><><><><><><><><><><><><><><//
typedef struct
{
	List(char*) defs;
	List(char*) incs;
	List(char*) libs;
	List(char*) opts;
	List(char*) srcs;
	List(char*) ldrs;
	List(char*) objs;
	char *name;
	char *lang;
	char *build_dir;
	char *bin_dir;
	char *bin;
	char *comp;
	char *kind;
	char *std;
} Project;

Project *Project_new()
{
	Project *proj = calloc(1, sizeof(Project));
	proj->bin       = cpmstr("a.out");
	proj->bin_dir   = cpmstr("bin");
	proj->build_dir = cpmstr("build");
	proj->comp      = cpmstr("gcc");
	proj->lang      = cpmstr("c");
	proj->std       = cpmstr("c11");
	proj->name      = cpmstr("default");
	proj->kind      = cpmstr("executable");
	Dyna_new(&proj->defs);
	Dyna_new(&proj->incs);
	Dyna_new(&proj->libs);
	Dyna_new(&proj->objs);
	Dyna_new(&proj->opts);
	Dyna_new(&proj->srcs);
	Dyna_new(&proj->ldrs);
	return proj;
}

void Project_free(Project *p)
{
	free(p->name);
	free(p->lang);
	free(p->kind);
	free(p->build_dir);
	free(p->bin_dir);
	free(p->bin);
	free(p->comp);
	free(p->std);
	Dyna_free(&p->defs);
	Dyna_free(&p->incs);
	Dyna_free(&p->libs);
	Dyna_free(&p->opts);
	Dyna_free(&p->srcs);
	Dyna_free(&p->ldrs);
	Dyna_free(&p->objs);
	free(p);

}

static struct {
	List(Project) projs;
} G;

//><><><><><><>< API FUNCTIONS ><><><><><><><><><><><><><><><><><><//

#define APIF() Project *P = G.projs.data[G.projs.length - 1]; // API Function

int api__project(lua_State *l)
{
	// if(G.projs.length != 0)
	// {
	// 	Project *prev = G.projs.data[G.projs.length - 1];
	// 	printf("[lua] prev project: '%s'!\n", prev->name);
	// }
	Dyna_push(&G.projs, Project_new());
	APIF();
	P->name = cpmstr(luaL_checkstring(l, 1));
	// printf("[lua] new project: '%s'!\n", P->name);
	return 0;
}

#define SETSTR_API_FUNC(FIELD, NAME) int api__##NAME(lua_State *l) \
	{ APIF(); P->FIELD = cpmstr(luaL_checkstring(l, 1)); return 0; }

#define SETLIST_API_FUNC(FIELD, NAME) int api__##NAME(lua_State *l) {\
	APIF(); luaL_checktype(l, 1, LUA_TTABLE); \
	size_t n = Dyna_reserve(&P->FIELD, lua_rawlen(l, 1)); \
	for(int i = 1; i <= n; ++i) { \
		int vt = lua_rawgeti(l, 1, i); \
		if(LUA_TSTRING != vt) { \
			luaL_error(l, "Expected a list of strings (item #%d has type %s)", i, lua_typename(l, vt)); \
			return 0; \
		} \
		const char *c = lua_tostring(l, -1); \
		Dyna_push(&P->FIELD, cpmstr(c)); \
	}}

SETSTR_API_FUNC(lang, lang);
SETSTR_API_FUNC(comp, compiler);
SETSTR_API_FUNC(std, standard);
SETSTR_API_FUNC(build_dir, build_dir);
SETSTR_API_FUNC(bin_dir, bin_dir);
SETSTR_API_FUNC(bin, bin);
SETSTR_API_FUNC(kind, kind);
SETLIST_API_FUNC(defs, defines);
SETLIST_API_FUNC(incs, includes);
SETLIST_API_FUNC(libs, libraries);
SETLIST_API_FUNC(ldrs, libdirs);
SETLIST_API_FUNC(opts, options);
SETLIST_API_FUNC(srcs, sources);
SETLIST_API_FUNC(objs, objects);

#undef APIF
#undef SETSTR_API_FUNC
#undef SETLIST_API_FUNC

//><><><><><><>< LUA INTERFACE ><><><><><><><><><><><><><><><><><><//

#define CFUNC(NAME) lua_pushcfunction(l, api__##NAME); \
                    lua_setglobal(l, #NAME);
void create_default_funcs(lua_State *l)
{
	CFUNC(project);
	CFUNC(lang);
	CFUNC(defines);
	CFUNC(includes);
	CFUNC(libraries);
	CFUNC(options);
	CFUNC(objects);
	CFUNC(libdirs);
	CFUNC(sources);
	CFUNC(compiler);
	CFUNC(standard);
	CFUNC(build_dir);
	CFUNC(bin_dir);
	CFUNC(bin);
	CFUNC(kind);
}
#undef CFUNC

//><><><><><><>< MAIN ><><><><><><><><><><><><><><><><><><//

void create_dir(const char *path)
{
	struct stat st = {0};
	if(stat(path, &st) == -1) mkdir(path, 0700);
}

void build_project(int i)
{
	Project *P = G.projs.data[i];
	printf("Building '%s'...\n", P->name);
	create_dir(P->build_dir);
	
	//:========----- BUILDING -----========://
	// printf(":--- building -----------:\n");

	// TODO: A list of .o files
	
	int filecount = 0;
	for(int i = 0; i < P->srcs.length; ++i)
	{
		glob_t globbuf;
		if(!glob(P->srcs.data[i], GLOB_NOCHECK, NULL, &globbuf))
		{
			for(int j = 0; j < globbuf.gl_pathc; ++j)
			{
				char out_fn[64];
				snprintf(out_fn, 64, "%s/%d_%s.o", P->build_dir, filecount++, basename(globbuf.gl_pathv[j]));

				if(!FORCE_REBUILD)
				{
					struct stat source_stat, built_stat;
					bool should_test_time = true;
					if(stat(globbuf.gl_pathv[j], &source_stat) != 0)
					{
						should_test_time = false;
						printf("[stat] '%s'\n", globbuf.gl_pathv[j]);
						perror("[stat] error");
					}

					if(stat(out_fn, &built_stat) != 0)
					{
						should_test_time = false;
						// printf("[stat] '%s'\n", out_fn);
						// perror("[stat] error");
						// Don't need to error if the output does not exist!
					}

					if(built_stat.st_mtime > source_stat.st_mtime) continue; // Built before...
				}

				Dyns cmd; Dyns_new(&cmd);
				Dyns_reserve(&cmd, 128);
				Dyns_concat(&cmd, P->comp);

				Dyns_concat(&cmd, " -c ");
				Dyns_concat(&cmd, globbuf.gl_pathv[j]);

				Dyns_concat(&cmd, " -o ");
				// Dyns_concat(&cmd, P->build_dir);
				// if(P->build_dir[strlen(P->build_dir)-1] != '/') Dyns_concat(&cmd, "/");

				Dyns_concat(&cmd, out_fn);

				Dyns_concat(&cmd, " -std="); Dyns_concat(&cmd, P->std);

				for(int k = 0; k < P->defs.length; ++k)
				{
					Dyns_concat(&cmd, " -D");
					Dyns_concat(&cmd, P->defs.data[k]);
				}

				for(int k = 0; k < P->incs.length; ++k)
				{
					Dyns_concat(&cmd, " -I");
					Dyns_concat(&cmd, P->incs.data[k]);
				}

				printf("%s\n", cmd.data);
				int ec;
				if((ec = system(cmd.data)) != 0)
				{
					fprintf(stderr, "Command exited with code %d!\n", ec);
					exit(1);
				}
				Dyns_free(&cmd);
			}
		}
		else
		{
			printf("[glob] error!\n");
		}
	}

	Dyns cmd; Dyns_new(&cmd);
	Dyns_reserve(&cmd, 128); // Important!!


	//:========----- EXECUTABLE LINKING -----========://
	if(strcmp(P->kind, "executable") == 0)
	{
		create_dir(P->bin_dir);
		// printf(":--- linking -----------:\n");
		Dyns_concat(&cmd, P->comp); Dyns_push(&cmd, ' ');

		{
			tinydir_dir d;
			tinydir_open(&d, P->build_dir);
			while(d.has_next)
			{
				tinydir_file file;
				tinydir_readfile(&d, &file);
				if(!file.is_dir)
				{
					Dyns_concat(&cmd, file.path);
					Dyns_push(&cmd, ' ');
				}

				tinydir_next(&d);
			}
		}
		for(int k = 0; k < P->objs.length; ++k)
		{
			Dyns_push(&cmd, ' ');
			Dyns_concat(&cmd, P->objs.data[k]);
		}

		Dyns_concat(&cmd, " -o ");
		Dyns_concat(&cmd, P->bin_dir);
		if(P->bin_dir[strlen(P->bin_dir)-1] != '/') Dyns_concat(&cmd, "/");
		Dyns_concat(&cmd, P->bin);

		for(int k = 0; k < P->ldrs.length; ++k)
		{
			Dyns_concat(&cmd, " -L");
			Dyns_concat(&cmd, P->ldrs.data[k]);
		}
		for(int k = 0; k < P->libs.length; ++k)
		{
			Dyns_concat(&cmd, " -l");
			Dyns_concat(&cmd, P->libs.data[k]);
		}
	}
	//:========----- STATIC LIBRARY ARCHIVING -----========://
	else if(strcmp(P->kind, "static library") == 0)
	{
		create_dir(P->bin_dir);
#if defined(DRAWIN)
		Dyns_concat(&cmd, "libtool -static -o ");
#elif defined(__linux__)
		Dyns_concat(&cmd, "ar -rcs ");
#else
#pragma error "Bruh"
#endif
		Dyns_concat(&cmd, P->bin_dir);
		if(P->bin_dir[strlen(P->bin_dir)-1] != '/') Dyns_concat(&cmd, "/");
		Dyns_concat(&cmd, P->bin);
		Dyns_push(&cmd, ' ');

		{
			tinydir_dir d;
			tinydir_open(&d, P->build_dir);
			while(d.has_next)
			{
				tinydir_file file;
				tinydir_readfile(&d, &file);
				if(!file.is_dir)
				{
					Dyns_concat(&cmd, file.path);
					Dyns_push(&cmd, ' ');
				}

				tinydir_next(&d);
			}
		}
	}
	//:========----- BUILD ONLY -----========://
	else if(strcmp(P->kind, "build only") == 0)
	{
		Dyns_free(&cmd);
		printf("Done '%s'!\n", P->name);
		return;
	}

	printf("%s\n", cmd.data);
	int ec;
	if((ec = system(cmd.data)) != 0)
	{
		fprintf(stderr, "Command exited with code %d!\n", ec);
		exit(1);
	}
	Dyns_free(&cmd);
	printf("Done '%s'!\n", P->name);
} 

int main(int argc, char *argv[])
{
	int opt;
	const char *FILENAME_DEFAULT = "mmake.lua";
	const char *PROJECT_DEFAULT = "";
	char *file_name = malloc(64);
	char *project_name = malloc(32);

	strncpy(file_name   ,FILENAME_DEFAULT, 64);
	strncpy(project_name, PROJECT_DEFAULT, 32);

	while((opt = getopt(argc, argv, "p:f:r")) != -1) 
	{
		switch(opt) 
		{
			case 'f':
			{
				if(optarg == NULL) return fprintf(stderr, "-f requires an argument!\n"), 1;
				strncpy(file_name, optarg, 64);
				break;
			}
			case 'p':
			{
				if(optarg == NULL) return fprintf(stderr, "-p requires an argument!\n"), 1;
				strncpy(project_name, optarg, 32);
				break;
			}
			case 'r':
			{
				// if(optarg == NULL) return fprintf(stderr, "-r requires an argument!\n"), 1;
				// strncpy(project_name, optarg, 32);
				FORCE_REBUILD = true;
				break;
			}
			default:
				puts("Unknown option");
				break;
		}
	}

	Dyna_new(&G.projs);

	lua_State *l = luaL_newstate();
	// luaopen_base(l);
	// luaopen_math(l);
	// luaopen_string(l);
	// luaopen_table(l);
	luaL_openlibs(l); // TODO!!!! DO NOT OPEN IO!!
	create_default_funcs(l);
	
	if(luaL_dofile(l, file_name))
	{
		fprintf(stderr, "Error:\n  %s\n", lua_tostring(l, -1));
		return 1;
	}
	
	// Project *p0 = G.projs.data[0];
	// printf("[0] name: '%s'\n", p0 ->name);
	// printf("[1] name: '%s'\n"), ((Project*)(G.projs.data[1]))->name;
	// printf("[2] name: '%s'\n"), ((Project*)(G.projs.data[2]))->name;

	// printf("- Projects: -----------\n");

	// for(unsigned i = 0; i < G.projs.length; ++i)
	// {
	// 	Project *P = G.projs.data[i];
	// 	printf("Project: %s\n", P->name);
	// }


	int project_index = -1;
	if(strlen(project_name) != 0)
	{
		for(unsigned i = 0; i < G.projs.length; ++i)
		{
			Project *P = G.projs.data[i];
			if(strcmp(P->name, project_name) == 0)
			{
				project_index = i;
				break;
			}
		}
	
		if(project_index == -1)
		{
			fprintf(stderr, "Error: No such project: '%s'!\n", project_name);
			lua_close(l);
			Dyna_foreach(&G.projs, (DynaFunc)Project_free);
			Dyna_free(&G.projs);
			free(project_name);
			free(file_name);
			return 1;
		}
	}
	else project_index = 0;

	build_project(project_index);
	
	Dyna_foreach(&G.projs, (DynaFunc)Project_free);
	Dyna_free(&G.projs);
	free(project_name);
	free(file_name);
	lua_close(l);
}
