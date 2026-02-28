// LoggerDemo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "Logger.h"

int main()
{
	std::cout << "hello" << std::endl;
	Logger::Initialize();
	Logger::Log("Hello");
	std::cout << "hello" << std::endl;
}

