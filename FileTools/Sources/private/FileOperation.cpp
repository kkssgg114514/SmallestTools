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
	// 文件路径
	this->pSz_FilePath = strdup(pSz_FilePath);
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
	delete pSz_FilePath;
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

FileHintCode FileOperation::write(size_t st_Offset, const uint8_t *pU8t_Data, size_t st_Length)
{
    if (!p_Impl || !pU8t_Data)
    {
        return FileHintCode::InvalidParameter;
    }
    // 即使超出长度也可以写入,会自动扩容
    p_Impl->vecU8t_Buffer.resize(st_Offset + st_Length);
    // 写入数据
    std::memcpy(p_Impl->vecU8t_Buffer.data() + st_Offset, pU8t_Data, st_Length);
	return FileHintCode::Success;
}

FileHintCode FileOperation::rewrite(const uint8_t *pU8t_Data, size_t st_Length)
{
    // 删除原有所有数据,重新写入
    if (!p_Impl || !pU8t_Data)
    {
        return FileHintCode::InvalidParameter;
    }
    p_Impl->vecU8t_Buffer.clear();
    // 写入新数据
    return write(0, pU8t_Data, st_Length);
}

FileHintCode FileOperation::append(const uint8_t *pU8t_Data, size_t st_Length)
{
    if (!p_Impl || !pU8t_Data)
    {
        return FileHintCode::InvalidParameter;
    }
    // 追加写入数据
    p_Impl->vecU8t_Buffer.insert(p_Impl->vecU8t_Buffer.end(), pU8t_Data, pU8t_Data + st_Length);
	return FileHintCode::Success;
}

FileHintCode FileOperation::flush()
{
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    // 将数据写入文件
    std::ofstream fs_File(pSz_FilePath, std::ios::binary);
    fs_File.write(reinterpret_cast<char*>(p_Impl->vecU8t_Buffer.data()), p_Impl->vecU8t_Buffer.size());
    fs_File.close();
	return FileHintCode::Success;
}
