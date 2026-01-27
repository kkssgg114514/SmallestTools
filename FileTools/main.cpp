//
// Created by kkssgg on 2026/1/13.
//
#include <FileOperation.h>
#include <iostream>

int main()
{
	// 测试文件路径
	const char* pSz_FilePath = "D:\\private\\test.txt";
	// 创建文件操作对象
	FileOperation fo_File(pSz_FilePath);

	// 写入数据
	fo_File.write(0, (const uint8_t*)"Hello, World!", 13);
	// 追加写入数据
	fo_File.append((const uint8_t*)"追加写入数据", 10);
	// 刷新数据到文件
	fo_File.flush();

	return 0;
}