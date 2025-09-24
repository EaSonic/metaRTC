//
// Copyright (c) 2019-2022 yanggaofeng
//
#include "YangLoadLib.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#define yang_error printf
#ifdef _WIN32
#include <windows.h>
#include <basetyps.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#else
   #include <dlfcn.h>
#endif

int32_t yang_getLibpath(char* path){

	char tmp[255];
	memset(tmp,0,sizeof(tmp));
#ifdef _MSC_VER
	if(_getcwd(tmp, 255)) {

#else
		if(getcwd(tmp, 255)) {
#endif
         
			snprintf(path,255,"%s",tmp);
			return 0;
		}
		return 1;
	}

YangLoadLib::YangLoadLib(){
	m_handle=NULL;
}
YangLoadLib::~YangLoadLib(){
	unloadObject();
}
void* YangLoadLib::loadSysObject(const char *sofile)
{

#ifdef _WIN32
   // LPCSTR
    m_handle = LoadLibraryA(sofile);
#else
    m_handle = dlopen(sofile, RTLD_NOW|RTLD_LOCAL);
#endif

    if (m_handle == 0) {
          	yang_error("Failed loading %s: %s", sofile, (char *) dlerror());
    }
    return (m_handle);
}
#define LENTH 200
void* YangLoadLib::loadObject(const char *sofile)
{


	char file1[LENTH+50];
	char file_path_getcwd[LENTH];
	memset(file1, 0, LENTH+50);
	memset(file_path_getcwd, 0, LENTH);
    if(yang_getLibpath(file_path_getcwd)!=0){
		yang_error( "Failed loading shared obj %s: %s,getcwd error!", sofile, (char *) dlerror());
		return NULL;
	}

#ifdef _WIN32
	sprintf(file1, "%s/lib/%s.dll", file_path_getcwd, sofile);
	m_handle =  LoadLibraryA(file1);
	if (m_handle == 0) {
		yang_error( "Failed loading shared obj %s: %s", sofile, (char *) dlerror());
	}
	return (m_handle);
#else
	const char* env_dir = getenv("YANG_LIBDIR");
	const char* brew_dir = "/opt/homebrew/lib";
	const char* local_dir = "/usr/local/opt/ffmpeg/lib";

#if defined(__APPLE__)
	const char* exts[] = { ".dylib", ".so" };
#else
	const char* exts[] = { ".so" };
#endif

	auto try_open = [&](const char* fullpath) -> void* {
		if(!fullpath || !fullpath[0]) return (void*)NULL;
		void* h = dlopen(fullpath, RTLD_NOW|RTLD_LOCAL);
		if(h) return h;
		return (void*)NULL;
	};

	// 1) try by bare name with extensions
	for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
		snprintf(file1, sizeof(file1), "%s%s", sofile, exts[i]);
		m_handle = try_open(file1);
		if(m_handle) return m_handle;
	}

	// 2) env override
	if(env_dir && env_dir[0]){
		for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
			snprintf(file1, sizeof(file1), "%s/%s%s", env_dir, sofile, exts[i]);
			m_handle = try_open(file1);
			if(m_handle) return m_handle;
		}
	}

	// 3) macOS common locations
#if defined(__APPLE__)
	for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
		snprintf(file1, sizeof(file1), "%s/%s%s", brew_dir, sofile, exts[i]);
		m_handle = try_open(file1);
		if(m_handle) return m_handle;
	}
	for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
		snprintf(file1, sizeof(file1), "%s/%s%s", local_dir, sofile, exts[i]);
		m_handle = try_open(file1);
		if(m_handle) return m_handle;
	}
#endif

	// 4) fallback to <cwd>/lib
	for(size_t i=0;i<sizeof(exts)/sizeof(exts[0]);++i){
		snprintf(file1, sizeof(file1), "%s/lib/%s%s", file_path_getcwd, sofile, exts[i]);
		m_handle = try_open(file1);
		if(m_handle) return m_handle;
	}

	yang_error( "Failed loading shared obj %s: %s", sofile, (char *) dlerror());
	return NULL;
#endif
}
#ifdef _WIN32
char *YangLoadLib::dlerror(){
    return (char*)"loadlib error";
}
#endif

void* YangLoadLib::loadFunction( const char *name)
{

#ifdef _WIN32
	void *symbol = (void *) GetProcAddress(m_handle, name);
#else
	void *symbol = dlsym(m_handle, name);
#endif

    if (symbol == NULL) {
        	yang_error("Failed loading function %s: %s", name,        (const char *) dlerror());
    }
    return (symbol);
}

void YangLoadLib::unloadObject()
{
    if (m_handle) {
#ifdef _WIN32
    	 FreeLibrary( m_handle);
#else
	 dlclose(m_handle);;
#endif
	 m_handle=NULL;

    }
}

//#endif

