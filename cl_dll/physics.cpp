
#include"physics.h"
#include <metahost.h>
#include<stdio.h>
#include<Windows.h>
//#include <mscoree.h> 

#pragma comment(lib, "mscoree.lib")

PhsicsAPI gPhysics;


#ifdef _DEBUG
const wchar_t PhyDllPath[] = L".\\gsphysics\\bin\\GoldsrcPhysics.dll";
#else// _DEBUG
const wchar_t PhyDllPath[] = L"\\gsphysics\\GoldsrcPhysics.dll";
#endif

//globle CLR handle
ICLRMetaHost* pMetaHost = nullptr;
ICLRMetaHostPolicy* pMetaHostPolicy = nullptr;
ICLRRuntimeHost* pRuntimeHost = nullptr;
ICLRRuntimeInfo* pRuntimeInfo = nullptr;

void ReleaseCLR()
{
	if (pRuntimeInfo != nullptr)
	{
		pRuntimeInfo->Release();
		pRuntimeInfo = nullptr;
	}

	if (pRuntimeHost != nullptr)
	{
		pRuntimeHost->Release();
		pRuntimeHost = nullptr;
	}

	if (pMetaHost != nullptr)
	{
		pMetaHost->Release();
		pMetaHost = nullptr;
	}
}

int InitCLR()
{
	if (pRuntimeInfo != nullptr)
		return 0;
	HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (LPVOID*)&pMetaHost);
	hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&pRuntimeInfo));

	if (FAILED(hr)) {
		ReleaseCLR();
	}

	hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&pRuntimeHost));
	hr = pRuntimeHost->Start();
	return (int)hr;
}

int ExitCLR()
{
	pRuntimeHost->Stop();//return HRESULT
	ReleaseCLR();
	return 0;
}

void* GetFunctionPointer(const LPCWSTR name)
{
	void* pfn = NULL;

	wchar_t buffer[64];//marshal args to [0xXXXX|MethodName] format
	swprintf(buffer, 64, L"%p|%s", &pfn, name);

	DWORD dwRet = 0;
	HRESULT hr = pRuntimeHost->ExecuteInDefaultAppDomain(PhyDllPath,
		L"GoldsrcPhysics.ExportAPIs.PhysicsMain",
		L"GetFunctionPointer",
		buffer,
		&dwRet);

	if (hr != S_OK)
		exit(12345);
	if ((DWORD)pfn != dwRet)
		exit(54321);

	return pfn;
}

//auto generated
extern "C" void InitPhysicsInterface(char* msg)
{
	InitCLR();
	gPhysics.Test = (void(_stdcall*)())GetFunctionPointer(L"Test");
	gPhysics.InitSystem = (void(_stdcall*)(void* pStudioRenderer, void* lastFieldAddress, void* engineStudioAPI))GetFunctionPointer(L"InitSystem");
	gPhysics.ChangeLevel = (void(_stdcall*)(const char* mapName))GetFunctionPointer(L"ChangeLevel");
	gPhysics.LevelReset = (void(_stdcall*)())GetFunctionPointer(L"LevelReset");
	gPhysics.Update = (void(_stdcall*)(float delta))GetFunctionPointer(L"Update");
	gPhysics.Pause = (void(_stdcall*)())GetFunctionPointer(L"Pause");
	gPhysics.Resume = (void(_stdcall*)())GetFunctionPointer(L"Resume");
	gPhysics.ShotDown = (void(_stdcall*)())GetFunctionPointer(L"ShotDown");
	gPhysics.ShowConfigForm = (void(_stdcall*)())GetFunctionPointer(L"ShowConfigForm");
	gPhysics.CreateRagdollController = (void(_stdcall*)(int entityId, const char* modelName))GetFunctionPointer(L"CreateRagdollController");
	gPhysics.CreateRagdollControllerIndex = (void(_stdcall*)(int entityId, int index))GetFunctionPointer(L"CreateRagdollControllerIndex");
	gPhysics.CreateRagdollControllerHeader = (void(_stdcall*)(int entityId, void * hdr))GetFunctionPointer(L"CreateRagdollControllerHeader");
	gPhysics.StartRagdoll = (void(_stdcall*)(int entityId))GetFunctionPointer(L"StartRagdoll");
	gPhysics.StopRagdoll = (void(_stdcall*)(int entityId))GetFunctionPointer(L"StopRagdoll");
	gPhysics.SetupBonesPhysically = (void(_stdcall*)(int entityId))GetFunctionPointer(L"SetupBonesPhysically");
	gPhysics.ChangeOwner = (void(_stdcall*)(int oldEntity, int newEntity))GetFunctionPointer(L"ChangeOwner");
	gPhysics.SetVelocity = (void(_stdcall*)(int entityId, Vector3 * v))GetFunctionPointer(L"SetVelocity");
	gPhysics.DisposeRagdollController = (void(_stdcall*)(int entityId))GetFunctionPointer(L"DisposeRagdollController");
	gPhysics.Explosion = (void(_stdcall*)(Vector3 * pos, float intensity))GetFunctionPointer(L"Explosion");
	gPhysics.Shoot = (void(_stdcall*)(Vector3 * from, Vector3 * force))GetFunctionPointer(L"Shoot");
	gPhysics.PickBody = (void(_stdcall*)())GetFunctionPointer(L"PickBody");
	gPhysics.ReleaseBody = (void(_stdcall*)())GetFunctionPointer(L"ReleaseBody");
}