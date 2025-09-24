//
// Copyright (c) 2019-2025 yanggaofeng
//
#include <yangutil/sys/YangLoadLib.h>
#include <yangutil/sys/YangLog.h>

extern "C"{
#include <yangutil/sys/YangFile.h>
}

#if Yang_OS_WIN
#include <windows.h>
#include <basetyps.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#else
   #include <dlfcn.h>
#endif

#include <stdlib.h>



YangLoadLib::YangLoadLib(){
	m_handle=NULL;
}
YangLoadLib::~YangLoadLib(){
	unloadObject();
}
void* YangLoadLib::loadSysObject(const char *sofile)
{

#if Yang_OS_WIN
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

#if Yang_OS_WIN
    if(yang_getLibpath(file_path_getcwd)!=Yang_Ok){
        yang_error( "Failed loading shared obj %s: getcwd error!", sofile);
        return NULL;
    }
    sprintf(file1, "%s/%s.dll", file_path_getcwd, sofile);
    m_handle =  LoadLibraryA(file1);
    if (m_handle == 0) {
    	yang_error( "Failed loading shared obj %s: %s", sofile, (char *) dlerror());
    }
    return (m_handle);
#else
    // macOS/Linux: build a list of candidate paths and extensions
    const char* env_dir = getenv("YANG_LIBDIR");
    const char* brew_dir = "/opt/homebrew/lib"; // Apple Silicon Homebrew default
    const char* local_dir = "/usr/local/opt/ffmpeg/lib";   // Intel Homebrew default

    // Try bare names first to honor system/lib paths if configured
#if Yang_OS_APPLE
    const char* names[] = {
        ".dylib",
        ".so"
    };
#else
    const char* names[] = {
        ".so"
    };
#endif

    // Helper lambda to attempt dlopen for a constructed full path
    auto try_open = [&](const char* fullpath) -> void* {
        if(!fullpath || !fullpath[0]) return (void*)NULL;
        void* h = dlopen(fullpath, RTLD_NOW|RTLD_LOCAL);
        if(h) return h;
        return (void*)NULL;
    };

    // 1) Try dlopen with just the filename + extension (dyld will use its search paths)
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i){
        snprintf(file1, sizeof(file1), "%s%s", sofile, names[i]);
        m_handle = try_open(file1);
        if(m_handle) return m_handle;
    }

    // 2) Try YANG_LIBDIR override
    if(env_dir && env_dir[0]){
        for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i){
            snprintf(file1, sizeof(file1), "%s/%s%s", env_dir, sofile, names[i]);
            m_handle = try_open(file1);
            if(m_handle) return m_handle;
        }
    }

    // 3) Try Homebrew/common locations on macOS
#if Yang_OS_APPLE
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i){
        snprintf(file1, sizeof(file1), "%s/%s%s", brew_dir, sofile, names[i]);
        m_handle = try_open(file1);
        if(m_handle) return m_handle;
    }
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i){
        snprintf(file1, sizeof(file1), "%s/%s%s", local_dir, sofile, names[i]);
        m_handle = try_open(file1);
        if(m_handle) return m_handle;
    }
#endif

    // 4) Fallback to historical getcwd-based lib path (e.g., <cwd>/lib)
    if(yang_getLibpath(file_path_getcwd)==Yang_Ok){
        for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i){
            snprintf(file1, sizeof(file1), "%s/%s%s", file_path_getcwd, sofile, names[i]);
            m_handle = try_open(file1);
            if(m_handle) return m_handle;
        }
    }

    // If still not found, emit a detailed error
    yang_error("Failed loading shared obj %s: %s", sofile, (char*)dlerror());
    return NULL;
#endif
}
#if Yang_OS_WIN
char *YangLoadLib::dlerror(){
    return (char*)"loadlib error";
}
#endif

void* YangLoadLib::loadFunction( const char *name)
{

#if Yang_OS_WIN
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
#if Yang_OS_WIN
    	 FreeLibrary( m_handle);
#else
	 dlclose(m_handle);;
#endif
	 m_handle=NULL;

    }
}

//#endif

