#include <FileOperation.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace
{
	const char* fileHintCodeToString(FileHintCode fhc_Code)
	{
		switch (fhc_Code)
		{
		case FileHintCode::Success:
			return "成功";
		case FileHintCode::NotFound:
			return "文件不存在";
		case FileHintCode::ReadError:
			return "读取失败";
		case FileHintCode::OpenError:
			return "打开失败";
		case FileHintCode::WriteError:
			return "写入失败";
		case FileHintCode::InvalidParameter:
			return "参数无效";
		default:
			return "未知错误";
		}
	}

	bool checkCode(const char* pSz_StepName, FileHintCode fhc_Code)
	{
		if (fhc_Code == FileHintCode::Success)
		{
			std::cout << pSz_StepName << "：成功" << std::endl;
			return true;
		}

		std::cout << pSz_StepName << "：失败，错误码为 " << fileHintCodeToString(fhc_Code) << std::endl;
		return false;
	}

	void printBytes(const char* pSz_Title, const uint8_t* pU8t_Data, size_t st_Length)
	{
		std::cout << pSz_Title << "：";
		for (size_t st_Index = 0; st_Index < st_Length; ++st_Index)
		{
			std::cout << static_cast<char>(pU8t_Data[st_Index]);
		}
		std::cout << std::endl;
	}
}

int main()
{
	try
	{
		// 使用当前工作目录下的示例文件，避免依赖固定绝对路径
		const std::filesystem::path fsPath_FilePath = std::filesystem::current_path() / "FileTools_demo.bin";
		const std::string str_FilePath = fsPath_FilePath.string();

		// 先准备一个初始文件，便于演示构造时读取现有内容
		{
			std::ofstream fs_InitFile(str_FilePath, std::ios::binary | std::ios::trunc);
			const std::string str_InitText = "InitData";
			fs_InitFile.write(str_InitText.data(), static_cast<std::streamsize>(str_InitText.size()));
		}

		FileOperation fo_File(str_FilePath.c_str());

		size_t st_FileSize = 0;
		if (!checkCode("读取初始文件大小", fo_File.size(st_FileSize)))
		{
			return 1;
		}
		std::cout << "初始文件大小：" << st_FileSize << std::endl;

		const uint8_t* pU8t_AllData = nullptr;
		if (!checkCode("获取完整缓冲区", fo_File.data(&pU8t_AllData, st_FileSize)))
		{
			return 1;
		}
		printBytes("初始文件内容", pU8t_AllData, st_FileSize);

		uint8_t u8t_ReadBuffer[4] = {};
		if (!checkCode("读取前 4 个字节", fo_File.read(0, u8t_ReadBuffer, sizeof(u8t_ReadBuffer))))
		{
			return 1;
		}
		printBytes("读取结果", u8t_ReadBuffer, sizeof(u8t_ReadBuffer));

		const std::string str_WriteText = "Hello";
		if (!checkCode("从偏移 0 覆盖写入", fo_File.write(0,
			reinterpret_cast<const uint8_t*>(str_WriteText.data()),
			str_WriteText.size())))
		{
			return 1;
		}

		const std::string str_AppendText = "_Append";
		if (!checkCode("追加写入", fo_File.append(
			reinterpret_cast<const uint8_t*>(str_AppendText.data()),
			str_AppendText.size())))
		{
			return 1;
		}

		if (!checkCode("刷新到文件", fo_File.flush()))
		{
			return 1;
		}

		if (!checkCode("刷新后获取完整缓冲区", fo_File.data(&pU8t_AllData, st_FileSize)))
		{
			return 1;
		}
		printBytes("写入并追加后的内容", pU8t_AllData, st_FileSize);

		const std::string str_RewriteText = "RewriteData";
		if (!checkCode("重写全部内容", fo_File.rewrite(
			reinterpret_cast<const uint8_t*>(str_RewriteText.data()),
			str_RewriteText.size())))
		{
			return 1;
		}

		if (!checkCode("重写后再次刷新", fo_File.flush()))
		{
			return 1;
		}

		if (!checkCode("重写后再次获取完整缓冲区", fo_File.data(&pU8t_AllData, st_FileSize)))
		{
			return 1;
		}
		printBytes("重写后的内容", pU8t_AllData, st_FileSize);

		// 演示移动构造，确认对象所有权可以安全转移
		FileOperation fo_MovedFile(std::move(fo_File));
		if (!checkCode("移动构造后读取大小", fo_MovedFile.size(st_FileSize)))
		{
			return 1;
		}
		std::cout << "移动构造后的文件大小：" << st_FileSize << std::endl;

		// 演示移动赋值，确认已存在对象也可以接管目标文件
		FileOperation fo_AssignedFile("FileTools_move_assign_demo.bin");
		fo_AssignedFile = std::move(fo_MovedFile);
		if (!checkCode("移动赋值后读取大小", fo_AssignedFile.size(st_FileSize)))
		{
			return 1;
		}
		std::cout << "移动赋值后的文件大小：" << st_FileSize << std::endl;

		if (!checkCode("移动赋值后最终刷新", fo_AssignedFile.flush()))
		{
			return 1;
		}
		std::cout << "FileTools 全部示例执行完成，目标文件位于：" << str_FilePath << std::endl;
	}
	catch (const std::exception& e_Exception)
	{
		std::cout << "发生异常：" << e_Exception.what() << std::endl;
		return 1;
	}

	return 0;
}
