#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "../public/FileOperation.h"

struct FileOperation::Impl
{
    // 使用连续缓冲区缓存完整文件内容，便于随机读写
    std::vector<uint8_t> vecU8t_Buffer;
    // 保存文件路径，刷新时直接写回原目标文件
    std::string str_FilePath;
};

// 匿名命名空间中的工具函数仅在当前源文件内可见
namespace
{
    // 安全计算两个长度之和，避免无符号整数溢出
    bool addSize(size_t st_Left, size_t st_Right, size_t &refSt_OutResult)
    {
        if (st_Left > std::numeric_limits<size_t>::max() - st_Right)
        {
            return false;
        }
        refSt_OutResult = st_Left + st_Right;
        return true;
    }
}

FileOperation::FileOperation(const char *pSz_FilePath)
{
    if (!pSz_FilePath)
    {
        throw std::invalid_argument("FilePath is null");
    }

    std::unique_ptr<Impl> pTmp_Impl = std::make_unique<Impl>();
    pTmp_Impl->str_FilePath = pSz_FilePath;

    // 先检查路径状态，确保后续行为明确可控
    std::error_code ec_ErrorCode;
    const std::filesystem::path fsPath_FilePath(pSz_FilePath);
    const bool b_FileExists = std::filesystem::exists(fsPath_FilePath, ec_ErrorCode);
    if (ec_ErrorCode)
    {
        throw std::runtime_error("Get file status failed");
    }

    if (!b_FileExists)
    {
        // 文件不存在时保留空缓冲区，等待外部写入后再刷新到磁盘
        p_Impl = std::move(pTmp_Impl);
        return;
    }

    // 仅允许操作普通文件，避免目录等路径被误当成文件处理
    ec_ErrorCode.clear();
    const bool b_IsRegularFile = std::filesystem::is_regular_file(fsPath_FilePath, ec_ErrorCode);
    if (ec_ErrorCode)
    {
        throw std::runtime_error("Get file type failed");
    }
    if (!b_IsRegularFile)
    {
        throw std::runtime_error("File path is not a regular file");
    }

    // 先定位到文件尾获取大小，再回到文件头读取完整内容
    std::ifstream fs_File(pSz_FilePath, std::ios::binary | std::ios::ate);
    if (!fs_File)
    {
        throw std::runtime_error("Open file failed");
    }

    const std::streamoff so_FileSize = fs_File.tellg();
    if (so_FileSize < 0)
    {
        throw std::runtime_error("Get file size failed");
    }
    fs_File.seekg(0, std::ios::beg);
    if (!fs_File)
    {
        throw std::runtime_error("Seek file failed");
    }

    // 将现有文件完整读入内存缓冲区，后续所有写操作都先修改缓冲区
    pTmp_Impl->vecU8t_Buffer.resize(static_cast<size_t>(so_FileSize));
    if (!pTmp_Impl->vecU8t_Buffer.empty() &&
        !fs_File.read(reinterpret_cast<char *>(pTmp_Impl->vecU8t_Buffer.data()), so_FileSize))
    {
        throw std::runtime_error("Read file failed");
    }

    p_Impl = std::move(pTmp_Impl);
}

FileOperation::~FileOperation() = default;

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
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    if (!pU8t_Buffer && st_Length > 0)
    {
        return FileHintCode::InvalidParameter;
    }
    // 通过先比较偏移、再比较剩余长度的方式避免加法溢出
    const size_t st_BufferSize = p_Impl->vecU8t_Buffer.size();
    if (st_Offset > st_BufferSize)
    {
        return FileHintCode::ReadError;
    }
    if (st_Length > st_BufferSize - st_Offset)
    {
        return FileHintCode::ReadError;
    }
    if (st_Length == 0)
    {
        return FileHintCode::Success;
    }
    std::memcpy(pU8t_Buffer, p_Impl->vecU8t_Buffer.data() + st_Offset, st_Length);
    return FileHintCode::Success;
}

FileOperation::FileOperation(FileOperation &&fo_Other) noexcept = default;

FileOperation &FileOperation::operator=(FileOperation &&fo_Other) noexcept = default;

FileHintCode FileOperation::write(size_t st_Offset, const uint8_t *pU8t_Data, size_t st_Length)
{
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    if (!pU8t_Data && st_Length > 0)
    {
        return FileHintCode::InvalidParameter;
    }
    size_t st_NewSize = 0;
    if (!addSize(st_Offset, st_Length, st_NewSize))
    {
        return FileHintCode::InvalidParameter;
    }
    // 即使超出当前长度也允许写入，缓冲区会自动扩容
    p_Impl->vecU8t_Buffer.resize(st_NewSize);
    if (st_Length == 0)
    {
        return FileHintCode::Success;
    }
    // 写入数据
    std::memcpy(p_Impl->vecU8t_Buffer.data() + st_Offset, pU8t_Data, st_Length);
    return FileHintCode::Success;
}

FileHintCode FileOperation::rewrite(const uint8_t *pU8t_Data, size_t st_Length)
{
    // 删除原有所有数据，再从头写入新内容
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    if (!pU8t_Data && st_Length > 0)
    {
        return FileHintCode::InvalidParameter;
    }
    p_Impl->vecU8t_Buffer.clear();
    if (st_Length == 0)
    {
        return FileHintCode::Success;
    }
    // 新数据仍复用 write 接口，保持逻辑一致
    return write(0, pU8t_Data, st_Length);
}

FileHintCode FileOperation::append(const uint8_t *pU8t_Data, size_t st_Length)
{
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    if (!pU8t_Data && st_Length > 0)
    {
        return FileHintCode::InvalidParameter;
    }
    size_t st_NewSize = 0;
    if (!addSize(p_Impl->vecU8t_Buffer.size(), st_Length, st_NewSize))
    {
        return FileHintCode::InvalidParameter;
    }
    if (st_Length == 0)
    {
        return FileHintCode::Success;
    }
    // 将新数据直接追加到缓冲区末尾
    p_Impl->vecU8t_Buffer.insert(p_Impl->vecU8t_Buffer.end(), pU8t_Data, pU8t_Data + st_Length);
    return FileHintCode::Success;
}

FileHintCode FileOperation::flush()
{
    if (!p_Impl)
    {
        return FileHintCode::InvalidParameter;
    }
    // 将缓冲区完整写回文件，写入失败时返回明确错误码
    std::ofstream fs_File(p_Impl->str_FilePath, std::ios::binary | std::ios::trunc);
    if (!fs_File)
    {
        return FileHintCode::OpenError;
    }
    if (!p_Impl->vecU8t_Buffer.empty())
    {
        fs_File.write(reinterpret_cast<const char *>(p_Impl->vecU8t_Buffer.data()),
                      static_cast<std::streamsize>(p_Impl->vecU8t_Buffer.size()));
        if (!fs_File)
        {
            return FileHintCode::WriteError;
        }
    }
    fs_File.close();
    if (!fs_File)
    {
        return FileHintCode::WriteError;
    }
    return FileHintCode::Success;
}
