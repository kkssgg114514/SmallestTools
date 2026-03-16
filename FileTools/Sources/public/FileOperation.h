#ifndef SMALLESTTOOLS_FILEOPERATION_H
#define SMALLESTTOOLS_FILEOPERATION_H

#include <cstddef>
#include <cstdint>
#include <memory>

// 文件操作提示码
enum class FileHintCode : uint8_t {
	Success = 0,
	NotFound,
	ReadError,
	OpenError,
	WriteError,
	InvalidParameter,
	Unknown
};

class FileOperation
{
public:
	// 传入目标文件路径，若文件已存在则加载其内容，否则初始化为空缓冲区
	explicit FileOperation(const char* pSz_FilePath);
	// 析构时自动释放内部资源
	~FileOperation();
	// 不允许拷贝，只允许移动，防止资源所有权混乱
	FileOperation(const FileOperation&) = delete;
	FileOperation& operator=(const FileOperation&) = delete;
	// 允许移动，便于在外部转移文件对象所有权
	FileOperation(FileOperation&&) noexcept;
	FileOperation& operator=(FileOperation&&) noexcept;
public:
	// 获取文件大小
	FileHintCode size(size_t& refSt_OutSize) const;

	// 获取完整数据指针（只读），写入类接口后该指针可能失效
	FileHintCode data(const uint8_t** ppU8t_OutData, size_t& refSt_OutSize) const;

	// 从偏移读取指定长度
	FileHintCode read(size_t st_Offset, uint8_t* pU8t_Buffer, size_t st_Length) const;

	// 覆盖写入数据
	FileHintCode write(size_t st_Offset, const uint8_t* pU8t_Data, size_t st_Length);

	// 重新写入指定长度数据，删除原有数据
	FileHintCode rewrite(const uint8_t* pU8t_Data, size_t st_Length);

	// 追加写入数据
	FileHintCode append(const uint8_t* pU8t_Data, size_t st_Length);

	// 刷新缓冲区，将数据写入文件
	FileHintCode flush();

private:
	// 为了保持二进制接口稳定，使用不透明实现隐藏标准库细节
	struct Impl;
	std::unique_ptr<Impl> p_Impl;
};


#endif // 文件操作头文件结束
