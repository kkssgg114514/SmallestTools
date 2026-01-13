//
// Created by kkssgg on 2026/1/13.
//
#include <FileOperation.h>
#include <iostream>

int main()
{
	// 测试文件路径
	const char* pSz_FilePath = "D:\\private\\test.png";
	// 创建文件操作对象
	FileOperation fo_File(pSz_FilePath);
	// 获取文件大小
	size_t st_FileSize = 0;
	fo_File.size(st_FileSize);
	std::cout << "File size: " << st_FileSize << std::endl;
	// 获取前十个字节
	uint8_t* pU8t_Buffer = new uint8_t[10];
	// 读取前十个字节
	fo_File.read(0, pU8t_Buffer, 10);
	for (size_t i = 0; i < 10; i++)
	{
		std::cout << std::hex << (int)pU8t_Buffer[i] << " ";
	}
	std::cout << std::endl;
	// 释放内存
	delete[] pU8t_Buffer;

	return 0;
}