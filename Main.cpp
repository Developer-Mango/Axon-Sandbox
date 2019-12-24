
#include "globals.h"
#include "Bridge.h"
#include <string>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <iterator>
#include <fstream>
#include <intrin.h>
#include <Tlhelp32.h>
#include <CommCtrl.h>
#include <Wininet.h>
#pragma comment(lib, "wininet.lib")
using namespace std;


DWORD ScriptContext;
DWORD ScriptContextVFTable = x(0x1A75F84);

using Bridge::m_rL;
using Bridge::m_L;

void PushGlobal(DWORD rL, lua_State* L, const char* s)
{
	printf("PushGlobal %s\r\n", s);
	r_lua_getglobal(rL, s);
	Bridge::wrap(rL, L, -1);
	lua_setglobal(L, s);
	r_lua_pop(rL, 1);
}

DWORD WINAPI input(PVOID lvpParameter)
{
	string WholeScript = "";
	HANDLE hPipe;
	char buffer[999999];
	DWORD dwRead;
	hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\Axon"),
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
		PIPE_WAIT,
		1,
		999999,
		999999,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);
	while (hPipe != INVALID_HANDLE_VALUE)
	{
		if (ConnectNamedPipe(hPipe, NULL) != FALSE)
		{
			while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
			{
				buffer[dwRead] = '\0';
				try {
					WholeScript = WholeScript + buffer;
				}
				catch (std::exception e) {
					MessageBox(NULL, e.what(), "uh oh", MB_OK);
				}
				catch (...) {
					MessageBox(NULL, "An Unhandled Error Has Occured!", "uh oh", MB_OK);
				}
			}
			WholeScript = "spawn(function()\r\n" + WholeScript + "\r\nend)";
			int ref = luaL_dostring(m_L, WholeScript.c_str());
			if (ref)
			{
				printf("Error: %s\n", lua_tostring(m_L, -1));
				lua_pop(m_L, 1);
			}
			lua_pop(m_L, 1);
			WholeScript = "";
		}
		DisconnectNamedPipe(hPipe);
	}
}

namespace Memory {
	bool Compare(const char* pData, const char* bMask, const char* szMask)
	{
		while (*szMask) {
			__try {
				if (*szMask != '?') {
					if (*pData != *bMask) return 0;
				}
				++szMask, ++pData, ++bMask;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return 0;
			}
		}
		return 1;
	}

	DWORD Scan()
	{
		MEMORY_BASIC_INFORMATION MBI = { 0 };
		SYSTEM_INFO SI = { 0 };
		GetSystemInfo(&SI);
		DWORD Start = (DWORD)SI.lpMinimumApplicationAddress;
		DWORD End = (DWORD)SI.lpMaximumApplicationAddress;
		do
		{
			while (VirtualQuery((void*)Start, &MBI, sizeof(MBI))) {
				if ((MBI.Protect & PAGE_READWRITE) && !(MBI.Protect & PAGE_GUARD) && !(MBI.Protect & 0x90))
				{
					for (DWORD i = (DWORD)(MBI.BaseAddress); i - (DWORD)(MBI.BaseAddress) < MBI.RegionSize; ++i)
					{
						if (Compare((const char*)i, (char *)&ScriptContextVFTable, "xxxx"))
							return i;
					}
				}
				Start += MBI.RegionSize;
			}
		} while (Start < End);
		return 0;
	}
}

BOOL WINAPI CONSOLEBYPASS()
{
	DWORD nOldProtect;
	if (!VirtualProtect(FreeConsole, 1, PAGE_EXECUTE_READWRITE, &nOldProtect))
		return FALSE;
	*(BYTE*)(FreeConsole) = 0xC3;
	if (!VirtualProtect(FreeConsole, 1, nOldProtect, &nOldProtect))
		return FALSE;
	AllocConsole();
}

int getRawMetaTable(lua_State *L) {
	Bridge::unwrap(L, m_rL, 1);

	if (r_lua_getmetatable(m_rL, -1) == 0) {
		lua_pushnil(L);
		return 0;
	}
	Bridge::wrap(m_rL, L, -1);

	return 1;
}

static int UserDataGC(lua_State *Thread) {
	void *UD = lua_touserdata(Thread, 1);
	if (m_rL) {

		r_lua_rawgeti(m_rL, LUA_REGISTRYINDEX, (int)UD);
		if (r_lua_type(m_rL, -1) <= R_LUA_TNIL) {
			lua_pushnil(Thread);
			lua_rawseti(Thread, LUA_REGISTRYINDEX, (int)UD);
		}
	}
	return 0;
}

void main()
{
	/*CONSOLEBYPASS();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	HWND ConsoleHandle = GetConsoleWindow();
	SetWindowPos(ConsoleHandle, HWND_TOPMOST, 50, 20, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	ShowWindow(ConsoleHandle, 1);
	SetConsoleTitle("Axon - DEBUG CONSOLE.");*/
	printf("ScriptContext\r\n");
	ScriptContext = Memory::Scan();
	if (!ScriptContext)
	{
		MessageBoxA(NULL, "Scan failed ScriptContextVirtualTable", "Error", MB_OK);
	}
	printf("ScriptContext passed\r\n");
	printf("globalindex\r\n");
	m_rL = hookStateIndex(ScriptContext, 43);
	if (!m_rL)
	{
		MessageBoxA(NULL, "Scan failed Globalstateindex", "Error", MB_OK);
	}
	printf("globalindex passed\r\n");
	printf("newstate\r\n");
	m_L = luaL_newstate();
	printf("newstate passed\r\n");
	printf("identity\r\n");
	seti(m_rL);
	int unk[] = { 0, 0 };
	SandboxThread(m_rL, 6, (int)unk);
	printf("identity passed\r\n");
	printf("vehhand\r\n");
	Bridge::VehHandlerpush();
	printf("vehhand passed\r\n");
	printf("openlibs\r\n");
	luaL_openlibs(m_L);
	printf("openlibs passed\r\n");
	printf("initialziing shit\r\n");
	luaL_newmetatable(m_L, "garbagecollector");
	printf("success 1\r\n");
	lua_pushcfunction(m_L, UserDataGC);
	printf("success 2\r\n");
	lua_setfield(m_L, -2, "__gc");
	printf("success 3\r\n");
	lua_pushvalue(m_L, -1);
	printf("success 4\r\n");
	lua_setfield(m_L, -2, "__index");
	printf("success 5\r\n");
	printf("does pushglobal work?\r\n");
	PushGlobal(m_rL, m_L, "printidentity");
	PushGlobal(m_rL, m_L, "game");
	PushGlobal(m_rL, m_L, "Game");
	PushGlobal(m_rL, m_L, "workspace");
	PushGlobal(m_rL, m_L, "Workspace");
	printf("yep they do\r\n");
	PushGlobal(m_rL, m_L, "Axes");
	PushGlobal(m_rL, m_L, "BrickColor");
	PushGlobal(m_rL, m_L, "CFrame");
	PushGlobal(m_rL, m_L, "Color3");
	PushGlobal(m_rL, m_L, "ColorSequence");
	PushGlobal(m_rL, m_L, "ColorSequenceKeypoint");
	PushGlobal(m_rL, m_L, "NumberRange");
	PushGlobal(m_rL, m_L, "NumberSequence");
	PushGlobal(m_rL, m_L, "NumberSequenceKeypoint");
	PushGlobal(m_rL, m_L, "PhysicalProperties");
	PushGlobal(m_rL, m_L, "Ray");
	PushGlobal(m_rL, m_L, "Rect");
	PushGlobal(m_rL, m_L, "Region3");
	PushGlobal(m_rL, m_L, "Region3int16");
	PushGlobal(m_rL, m_L, "TweenInfo");
	PushGlobal(m_rL, m_L, "UDim");
	PushGlobal(m_rL, m_L, "UDim2");
	PushGlobal(m_rL, m_L, "Vector2");
	PushGlobal(m_rL, m_L, "Vector2int16");
	PushGlobal(m_rL, m_L, "Vector3");
	PushGlobal(m_rL, m_L, "Vector3int16");
	PushGlobal(m_rL, m_L, "Enum");
	PushGlobal(m_rL, m_L, "Faces");
	PushGlobal(m_rL, m_L, "Instance");
	PushGlobal(m_rL, m_L, "math");
	PushGlobal(m_rL, m_L, "warn");
	PushGlobal(m_rL, m_L, "typeof");
	PushGlobal(m_rL, m_L, "type");
	PushGlobal(m_rL, m_L, "spawn");
	PushGlobal(m_rL, m_L, "Spawn");
	PushGlobal(m_rL, m_L, "print");
	PushGlobal(m_rL, m_L, "printidentity");
	PushGlobal(m_rL, m_L, "ypcall");
	PushGlobal(m_rL, m_L, "Wait");
	PushGlobal(m_rL, m_L, "wait");
	PushGlobal(m_rL, m_L, "delay");
	PushGlobal(m_rL, m_L, "Delay");
	PushGlobal(m_rL, m_L, "tick");
	PushGlobal(m_rL, m_L, "LoadLibrary");
	lua_register(m_L, "getrawmetatable", getRawMetaTable);
	lua_newtable(m_L);
	lua_setglobal(m_L, "_G");
	printf("DONE SHIT\r\n");
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)input, NULL, NULL, NULL);
	//MessageBoxA(NULL, "Ready to Use!", "Ready to Use!", MB_TOPMOST);
	printf("Axon INJECTED!\n");
}

BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, void* Reserved)
{
	switch (Reason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(Module);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)main, NULL, NULL, NULL);
		break;
	case DLL_PROCESS_DETACH:
		break;
	default: break;
	}

	return TRUE;
}