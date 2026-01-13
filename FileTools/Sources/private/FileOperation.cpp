//
// Created by kkssgg on 2026/1/13.
//

#include <vector>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include "../public/FileOperation.h"

struct FileOperation::Impl {
	std::vector<uint8_t> vecU8t_Buffer;
};

FileOperation::FileOperation(const char *pSz_FilePath) : p_Impl(new Impl)
{
	if (!pSz_FilePath)
	{
		throw std::invalid_argument("FilePath is null");
	}
	// 以现代方式读取
	std::ifstream fs_File(pSz_FilePath, std::ios::binary | std::ios::ate);
	if (!fs_File)
	{
		throw std::runtime_error("File not found");
	}

	std::streamsize ss_FileSize = fs_File.tellg();
	fs_File.seekg(0, std::ios::beg);

	// 读取文件内容到缓冲区
	p_Impl->vecU8t_Buffer.resize(ss_FileSize);
	if (!fs_File.read(reinterpret_cast<char*>(p_Impl->vecU8t_Buffer.data()), ss_FileSize))
	{
		throw std::runtime_error("Read file failed");
	}
}

FileOperation::~FileOperation()
{
	// 通过析构函数删除
	delete p_Impl;
}

FileHintCode FileOperation::size(size_t &refSt_OutSize) const
{
	if (!p_Impl)
	{
		return FileHintCode::InvalidParameter;
	}
	refSt_OutSize = p_Impl->vecU8t_Buffer.size();
	return FileHintCode::Success;
}

FileHintCode FileOperation::data(const uint8_t **ppU8t_OutData, size_t &refSt_OutSize) const
{
	if (!p_Impl || !ppU8t_OutData)
	{
		return FileHintCode::InvalidParameter;
	}
	*ppU8t_OutData = p_Impl->vecU8t_Buffer.data();
	refSt_OutSize = p_Impl->vecU8t_Buffer.size();
	return FileHintCode::Success;
}

FileHintCode FileOperation::read(size_t st_Offset, uint8_t *pU8t_Buffer, size_t st_Length) const
{
	if (!p_Impl || !pU8t_Buffer)
	{
		return FileHintCode::InvalidParameter;
	}
	if (st_Offset + st_Length > p_Impl->vecU8t_Buffer.size())
	{
		return FileHintCode::ReadError;
	}
	std::memcpy(pU8t_Buffer, p_Impl->vecU8t_Buffer.data() + st_Offset, st_Length);
	return FileHintCode::Success;
}

FileOperation::FileOperation(FileOperation && fo_Other) noexcept : p_Impl(fo_Other.p_Impl)
{
	// 移动构造
	fo_Other.p_Impl = nullptr;
}

FileOperation &FileOperation::operator=(FileOperation && fo_Other) noexcept
{
	// 移动赋值运算符
	if (this != &fo_Other)
	{
		delete p_Impl;
		p_Impl = fo_Other.p_Impl;
		fo_Other.p_Impl = nullptr;
	}
	return *this;
}
