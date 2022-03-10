#include "cwpch.h"

#include "Crowny/Memory/MemoryManager.h"

#include "Windows.h"

namespace Crowny
{
	SystemMemoryInfo MemoryManager::GetSystemInfo()
	{
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&status);
		SystemMemoryInfo res = { (uint64_t)status.ullAvailPhys, (uint64_t)status.ullTotalPhys, (uint64_t)status.ullAvailVirtual, (uint64_t)status.ullTotalVirtual };
		return res;
	}
}