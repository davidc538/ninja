#include <cstdlib>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
	#include <windows.h>
#elif __APPLE__
	#include <sys/sysctl.h>
	#include <mach/mach.h>
#elif __linux__
	#include <fstream>
	#include <string>
#endif

enum class SiPrefix : size_t {
	B = 1,
	KB = 1024,
	MB = 1024 * 1024,
	GB = 1024 * 1024 * 1024
};

template<SiPrefix prefix>
size_t ToUnits(size_t bytes) {
	size_t divisor = static_cast<size_t>(prefix);
	size_t ret = bytes / divisor;
	return ret;
}

template<SiPrefix prefix>
size_t FromUnits(size_t units) {
	size_t multiplier = static_cast<size_t>(prefix);
	size_t ret = units * multiplier;
	return ret;
}

struct MemInfo {
	size_t physical_memory;
	size_t free_memory;

	static void Error(const char* str) { std::cerr << str << std::endl; }

	void Init() {
#ifdef _WIN32
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		if (!GlobalMemoryStatusEx(&memInfo)) throw std::runtime_error("error getting physical memory");
		physical_memory = memInfo.ullTotalPhys;
		free_memory = memInfo.ullAvailPhys;
#elif __APPLE__
		size_t length = sizeof(physical_memory);
		int mib[2] = { CTL_HW, HW_MEMSIZE };
		int error = sysctl(mib, 2, &physical_memory, &length, nullptr, 0);

		if (error != 0) {
      Error("error getting physical memory");
      return;
    }

		mach_port_t host_port = mach_host_self();
		vm_size_t page_size;
		host_page_size(host_port, &page_size);

		vm_statistics64_data_t vm_stats;
		mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
		if (host_statistics64(host_port, HOST_VM_INFO, reinterpret_cast<host_info64_t>(&vm_stats), &count) != KERN_SUCCESS) {
			Error("error getting used memory");
			return;
		}

		size_t active_memory = static_cast<size_t>(vm_stats.active_count) * page_size;
		size_t inactive_memory = static_cast<size_t>(vm_stats.inactive_count) * page_size;
		size_t wired_memory = static_cast<size_t>(vm_stats.wire_count) * page_size;

		free_memory = physical_memory - (active_memory + inactive_memory + wired_memory);
#elif __linux__
		std::ifstream meminfo("/proc/meminfo");

		if (!meminfo) {
			Error("error opening /proc/meminfo");
			return;
		}

		std::string line;
		size_t memFree = 0, buffers = 0, cached = 0;
		while (std::getline(meminfo, line)) {
			if (line.find("MemTotal:") == 0) {
				sscanf(line.c_str(), "MemTotal: %zu kB", &physical_memory);
				physical_memory *= 1024;
			}
			else if (line.find("MemFree:") == 0) {
				sscanf(line.c_str(), "MemFree: %zu kB", &memFree);
				memFree *= 1024;
			}
			else if (line.find("Buffers:") == 0) {
				sscanf(line.c_str(), "Buffers: %zu kB", &buffers);
				buffers *= 1024;
			}
			else if (line.find("Cached:") == 0) {
				sscanf(line.c_str(), "Cached: %zu kB", &cached);
				cached *= 1024;
			}
		}
		free_memory = (physical_memory - memFree - buffers - cached);
#endif
	}

	inline MemInfo() {
		Init();
	}
};
