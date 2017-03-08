#include "pch.h"
#include "pch.h"
#include "Art.h"
#include "Messageprint.h"
#include <dlfcn.h>
#include "Hook.h"
#include <fcntl.h>
#include <cstdlib>

//大前提采用系统dex2oat 自定义oat 暂时没能力搞
static int (*dex2oatoldopen)(const char* file, int oflag, ...);

static int (*dex2oatoldfstat)(int, struct stat*);
static ssize_t (*dex2oatoldread)(int fd, void* des, size_t request);
static ssize_t (*dex2oatoldwrite)(int, const void*, size_t);
static void* (*dex2oatoldmmap)(void*, size_t, int, int, int, off_t);
static int (*dex2oatoldmprotect)(const void*, size_t, int);
static int (*dex2oatoldmunmap)(void*, size_t);

//传入dex文件路径
static char* dexFilePath;
//传入oat文件路径
static char* oatFilePath;
//传入解密dex key
static char* rc4Key;
//dex 文件fd;
static int dexfd = 0;
//oat 文件fd 在write方法中用到
static int oatfd = 0;

int dex2oatfstat(int fd, struct stat* st)
{
	//正常方法

	return dex2oatoldfstat(fd, st);
}

ssize_t dex2oatwrite(int fd, const void* dec, size_t request)
{
	//正常方法 开始写oat文件
	/*
	 *
	 */
	if (oatfd==-1||fd!=oatfd)
	{
		return  dex2oatoldwrite(fd, dec, request);
	}
	ssize_t res = dex2oatoldwrite(fd, dec, request);
	Messageprint::printinfo("loaddex", "write oatfile fd:%d  request:%d writeed:%d", fd, request, res);
	return res;
}

int dex2oatmprotect(const void* des, size_t size, int s)
{
	return dex2oatoldmprotect(des, size, s);
}

int dex2oatmunmap(void* des, size_t size)
{
	//set mummap fail
	return 0;
}

void* dex2oatmmap(void* start, size_t len, int prot, int flags, int fd, off_t offset)
{
	//这里对原来的dex文件进行mmap 到内存
	//判断是否 没mmap dex文件 
	/* 1. fd为null
	 * 2. dexfd=-1
	 * 3. fd不等于dexfd
	 */
	if (fd== 0xFFFFFFFF||dexfd==-1||fd!=dexfd)
	{
		return  dex2oatoldmmap(start, len, prot, flags, fd, offset);
	}
	/*
	 * 这里可以做相关的加密 暂时不做 其中fd是重点
	 */
	void* result = dex2oatoldmmap(start, len, prot, flags, fd, offset);
	Messageprint::printinfo("loaddex", "mmap dex start:0x%08x port:%d len:0x%08x fd:0x%X offset:0x%x", start, prot, len, fd, offset);
	return result;
}


ssize_t dex2oatread(int fd, void* dest, size_t request)
{
	//dex2oat在优化过程中 会读取一个"dex\n"文件头 过滤掉其他读取
	if (dexfd==-1||request!=4||fd!=dexfd)
	{
		return dex2oatoldread(fd, dest, request);
	}
	memcpy(dest, "dex\n", 4u);
	Messageprint::printinfo("loaddex", "read dex magic success");
	return 4;
}

static int dex2oatopen(const char* pathname, int flags, ...)
{
	//正常方法
	mode_t mode = 0;
	if ((flags & O_CREAT) != 0)
	{
		va_list args;
		va_start(args, flags);
		mode = static_cast<mode_t>(va_arg(args, int));
		va_end(args);
	}
	//对dexfd 进行判断
	if (dexfd!=0&&oatfd!=0)
	{
		return  dex2oatoldopen(pathname, flags, mode);;
	}
	if (strcmp(dexFilePath, pathname) == 0)
	{
		dexfd = dex2oatoldopen(pathname, flags, mode);
		Messageprint::printinfo("loaddex", "open dex:%s fd:%d", pathname, dexfd);
		return dexfd;
	}
	else if (strcmp(oatFilePath, pathname) == 0)
	{
		oatfd = dex2oatoldopen(pathname, flags, mode);
		Messageprint::printinfo("loaddex", "open oat:%s fd:%d", pathname, oatfd);
		return oatfd;
	}
	else
	{
		return  dex2oatoldopen(pathname, flags, mode);;
	}
}


void getEnvText()
{
	dexFilePath = getenv("DEX_PATH");
	rc4Key = getenv("key");
	oatFilePath = getenv("OAT_PATH");
	Messageprint::printinfo("loaddex", "dexpath:%s", dexFilePath);
}

void art::InitLogging(char* argv[])
{
	if (stophook)
	{
		oldInitLogging(argv);
	}
	getEnvText();
	void* arthandle = dlopen("libart.so", 0);
	oldInitLogging = (void(*)(char**))dlsym(arthandle, "_ZN3art11InitLoggingEPPc");
	Hook::hookMethod(arthandle, "open", (void*)dex2oatopen, (void**)(&dex2oatoldopen));
	Hook::hookMethod(arthandle, "read", (void*)dex2oatread, (void**)(&dex2oatoldread));
	//Hook::hookMethod(arthandle, "fstat", (void*)dex2oatfstat, (void**)(&dex2oatoldfstat));
	Hook::hookMethod(arthandle, "mmap", (void*)dex2oatmmap, (void**)(&dex2oatoldmmap));
	//Hook::hookMethod(arthandle, "mprotect", (void*)dex2oatmprotect, (void**)(&dex2oatoldmprotect));
	Hook::hookMethod(arthandle, "write", (void*)dex2oatwrite, (void**)(&dex2oatoldwrite));
#if defined(__arm__)
	Hook::hookAllRegistered();
#endif
	dlclose(arthandle);
	return oldInitLogging(argv);
}
