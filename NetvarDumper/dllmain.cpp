// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <iostream>
#include <fstream>

void* GetInterface(const char* moduleName, const char* interfaceName)
{
	auto CreateInterface = reinterpret_cast<void* (*)(const char* name, int* returnCode)>(GetProcAddress(GetModuleHandleA(moduleName), "CreateInterface"));
	return CreateInterface(interfaceName, nullptr);
}

typedef enum
{
	DPT_Int = 0,
	DPT_Float,
	DPT_Vector,
	DPT_VectorXY, // Only encodes the XY of a vector, ignores Z
	DPT_String,
	DPT_Array,	// An array of the base types (can't be of datatables).
	DPT_DataTable,
#if 0 // We can't ship this since it changes the size of DTVariant to be 20 bytes instead of 16 and that breaks MODs!!!
	DPT_Quaternion,
#endif

#ifdef SUPPORTS_INT64
	DPT_Int64,
#endif

	DPT_NUMSendPropTypes

} SendPropType;

class RecvTable;
class ClientClass
{
public:
	void*		m_pCreateFn;
	void*			m_pCreateEventFn;	// Only called for event objects.
	const char				*m_pNetworkName;
	RecvTable				*m_pRecvTable;
	ClientClass				*m_pNext;
	int						m_ClassID;	// Managed by the engine.
};

class RecvProp
{
	// This info comes from the receive data table.
public:
	const char              *m_pVarName;
	SendPropType			m_RecvType;
	int						m_Flags;
	int						m_StringBufferSize;

	bool					m_bInsideArray;		// Set to true by the engine if this property sits inside an array.

	// Extra data that certain special property types bind to the property here.
	const void *m_pExtraData;

	// If this is an array (DPT_Array).
	RecvProp				*m_pArrayProp;
	void*					m_ArrayLengthProxy;

	void*					m_ProxyFn;
	void*					m_DataTableProxyFn;	// For RDT_DataTable.

	RecvTable				*m_pDataTable;		// For RDT_DataTable.
	int						m_Offset;

	int						m_ElementStride;
	int						m_nElements;

	// If it's one of the numbered "000", "001", etc properties in an array, then
	// these can be used to get its array property name for debugging.
	const char				*m_pParentArrayPropName;
};


class RecvTable
{
public:
	// Properties described in a table.
	RecvProp		*m_pProps;
	int				m_nProps;

	// The decoder. NOTE: this covers each RecvTable AND all its children (ie: its children
	// will have their own decoders that include props for all their children).
	void			*m_pDecoder;

	const char		*m_pNetTableName;	// The name matched between client and server.

	bool			m_bInitialized;
	bool			m_bInMainList;
};

class IBaseClientDLL
{
public:
	// Called once when the client DLL is loaded
	virtual int				Init(void* appSystemFactory,
		void* physicsFactory,
		void *pGlobals) = 0;

	virtual void			PostInit() = 0;

	// Called once when the client DLL is being unloaded
	virtual void			Shutdown(void) = 0;

	// Called once the client is initialized to setup client-side replay interface pointers
	virtual bool			ReplayInit(void* replayFactory) = 0;
	virtual bool			ReplayPostInit() = 0;

	// Called at the start of each level change
	virtual void			LevelInitPreEntity(char const* pMapName) = 0;
	// Called at the start of a new level, after the entities have been received and created
	virtual void			LevelInitPostEntity() = 0;
	// Called at the end of a level
	virtual void			LevelShutdown(void) = 0;

	// Request a pointer to the list of client datatable classes
	virtual ClientClass		*GetAllClasses(void) = 0;
};

IBaseClientDLL* CHLClient = nullptr;

std::ofstream out;
void DumpNetvar(RecvTable* Table)
{
	for (int i = 0; i < Table->m_nProps; ++i)
	{
		auto Property = &Table->m_pProps[i];
		if (!Property || isdigit(Property->m_pVarName[0]))
			continue;

		if (strcmp(Property->m_pVarName, "baseclass") == 0)
			continue;

		if (Property->m_RecvType == DPT_DataTable && Property->m_pDataTable != nullptr && Property->m_pDataTable->m_pNetTableName[0] == 'D')
		{
			DumpNetvar(Property->m_pDataTable);
		}

		std::string strHash = Table->m_pNetTableName;
		strHash += "->";
		strHash += Property->m_pVarName;
		out << strHash.c_str() << std::endl;
	}
}

DWORD WINAPI MainThread()
{
	CHLClient = (IBaseClientDLL*)GetInterface("client.dll", "VClient017");
	out.open("netvar_dump.txt");
	for (auto pClass = CHLClient->GetAllClasses(); pClass; pClass = pClass->m_pNext)
	{
		if (!pClass->m_pRecvTable)
			continue;

		DumpNetvar(pClass->m_pRecvTable);
	}
	out.close();

	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)MainThread, NULL, NULL, NULL);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

