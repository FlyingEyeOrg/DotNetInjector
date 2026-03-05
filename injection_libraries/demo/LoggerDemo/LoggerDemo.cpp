// LoggerDemo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "Logger.h"
#include "injection_parameters.h"
#include "string_converter.h"
#include "shared_memory.h"

int main()
{
	std::cout << "hello" << std::endl;
	Logger::Initialize();
	Logger::Log("Hello");
	std::cout << "hello" << std::endl;

	//SharedMemory mem;
	//// Global\[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-MyFileMappingObject
	//// Global\[b62658dd-18f4-4de3-a09c-53c6c6cbf7d4]-MyFileMappingObject
	//if (mem.Open(L"FrameworkVersion", 256))
	//{
	//	auto value = mem.Read();
	//	return 0;
	//}

	InjectionParameters parameters;

	auto isOpen = parameters.Open();

	if (!isOpen)
	{
		std::cout << "open shared memory failed." << std::endl;
		return 1;
	}

	std::cout << "parameters: " << std::endl;
	auto wstr = parameters.GetDebugSummary();
	auto gbk = ::string_convertor::wstring_to_gbk(wstr);
	std::cout << gbk << std::endl;
}

