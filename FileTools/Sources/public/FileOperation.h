//
// Created by kkssgg on 2026/1/13.
//

#ifndef SMALLESTTOOLS_FILEOPERATION_H
#define SMALLESTTOOLS_FILEOPERATION_H

#include <cstddef>
#include <cstdint>

// 文件操作提示码
enum class FileHintCode : uint8_t {
	Success = 0,
	NotFound,
	ReadError,
	InvalidParameter,
	Unknown
};

class FileOperation
{
public:
	explicit FileOperation(const char* pSz_FilePath);
	~FileOperation();
	// 不允许拷贝,只允许移动,防止指针悬空
	FileOperation(const FileOperation&) = delete;
	FileOperation& operator=(const FileOperation&) = delete;
	FileOperation(FileOperation&&) noexcept;
	FileOperation& operator=(FileOperation&&) noexcept;
public:
	// 获取文件大小
	FileHintCode size(size_t& refSt_OutSize) const;

	// 获取完整数据指针(只读)
	FileHintCode data(const uint8_t** ppU8t_OutData, size_t& refSt_OutSize) const;

	// 从偏移读取指定长度
	FileHintCode read(size_t st_Offset, uint8_t* pU8t_Buffer, size_t st_Length) const;

private:
	// 为了 ABI 稳定,采取不透明实现,将使用 STL 的地方包装在 cpp 里面
	struct Impl;
	Impl* p_Impl;
};


#endif //SMALLESTTOOLS_FILEOPERATION_H
