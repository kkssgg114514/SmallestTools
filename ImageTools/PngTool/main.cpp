#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Sources/public/ST_png.h"

namespace
{
// 生成示例 PNG 时复用与库内一致的 CRC32 计算逻辑
uint32_t computeCrc32(const uint8_t* pU8t_Data, size_t st_Length)
{
	uint32_t u32t_Crc = 0xFFFFFFFFU;
	for (size_t st_Index = 0; st_Index < st_Length; ++st_Index)
	{
		u32t_Crc ^= pU8t_Data[st_Index];
		for (int i = 0; i < 8; ++i)
		{
			const bool b_ShouldXor = (u32t_Crc & 1U) != 0U;
			u32t_Crc >>= 1U;
			if (b_ShouldXor)
			{
				u32t_Crc ^= 0xEDB88320U;
			}
		}
	}
	return ~u32t_Crc;
}

// 生成最小 zlib 流时需要补充 Adler32 校验和
uint32_t computeAdler32(const uint8_t* pU8t_Data, size_t st_Length)
{
	constexpr uint32_t kModAdler = 65521U;
	uint32_t u32t_A = 1U;
	uint32_t u32t_B = 0U;
	for (size_t st_Index = 0; st_Index < st_Length; ++st_Index)
	{
		u32t_A = (u32t_A + pU8t_Data[st_Index]) % kModAdler;
		u32t_B = (u32t_B + u32t_A) % kModAdler;
	}
	return (u32t_B << 16U) | u32t_A;
}

// 以大端序写入 PNG 块中的 32 位字段
void appendBigEndianUint32(std::vector<uint8_t>& refVecU8t_OutData, uint32_t u32t_Value)
{
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 24U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 16U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 8U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>(u32t_Value & 0xFFU));
}

// 追加一个完整 PNG 数据块：长度 + 类型 + 数据 + CRC
void appendChunk(std::vector<uint8_t>& refVecU8t_PngData,
	const std::array<char, 4>& arrC_Type,
	const std::vector<uint8_t>& vecU8t_ChunkData)
{
	appendBigEndianUint32(refVecU8t_PngData, static_cast<uint32_t>(vecU8t_ChunkData.size()));
	for (const char c_Byte : arrC_Type)
	{
		refVecU8t_PngData.push_back(static_cast<uint8_t>(c_Byte));
	}
	refVecU8t_PngData.insert(refVecU8t_PngData.end(), vecU8t_ChunkData.begin(), vecU8t_ChunkData.end());

	std::vector<uint8_t> vecU8t_CrcData;
	vecU8t_CrcData.reserve(arrC_Type.size() + vecU8t_ChunkData.size());
	for (const char c_Byte : arrC_Type)
	{
		vecU8t_CrcData.push_back(static_cast<uint8_t>(c_Byte));
	}
	vecU8t_CrcData.insert(vecU8t_CrcData.end(), vecU8t_ChunkData.begin(), vecU8t_ChunkData.end());
	appendBigEndianUint32(refVecU8t_PngData, computeCrc32(vecU8t_CrcData.data(), vecU8t_CrcData.size()));
}

// demo 不依赖外部压缩库，直接生成“未压缩 DEFLATE 块”的 zlib 数据
std::vector<uint8_t> createStoredZlibStream(const std::vector<uint8_t>& vecU8t_RawData)
{
	std::vector<uint8_t> vecU8t_ZlibStream;
	vecU8t_ZlibStream.push_back(0x78U);
	vecU8t_ZlibStream.push_back(0x01U);

	size_t st_Offset = 0U;
	while (st_Offset < vecU8t_RawData.size())
	{
		const size_t st_Remaining = vecU8t_RawData.size() - st_Offset;
		const uint16_t u16t_BlockLength = static_cast<uint16_t>(std::min<size_t>(st_Remaining, 0xFFFFU));
		const bool b_IsFinalBlock = (st_Offset + u16t_BlockLength) == vecU8t_RawData.size();
		vecU8t_ZlibStream.push_back(static_cast<uint8_t>(b_IsFinalBlock ? 0x01U : 0x00U));
		vecU8t_ZlibStream.push_back(static_cast<uint8_t>(u16t_BlockLength & 0xFFU));
		vecU8t_ZlibStream.push_back(static_cast<uint8_t>((u16t_BlockLength >> 8U) & 0xFFU));
		const uint16_t u16t_NLength = static_cast<uint16_t>(~u16t_BlockLength);
		vecU8t_ZlibStream.push_back(static_cast<uint8_t>(u16t_NLength & 0xFFU));
		vecU8t_ZlibStream.push_back(static_cast<uint8_t>((u16t_NLength >> 8U) & 0xFFU));
		vecU8t_ZlibStream.insert(vecU8t_ZlibStream.end(),
			vecU8t_RawData.begin() + static_cast<std::ptrdiff_t>(st_Offset),
			vecU8t_RawData.begin() + static_cast<std::ptrdiff_t>(st_Offset + u16t_BlockLength));
		st_Offset += u16t_BlockLength;
	}

	appendBigEndianUint32(vecU8t_ZlibStream, computeAdler32(vecU8t_RawData.data(), vecU8t_RawData.size()));
	return vecU8t_ZlibStream;
}

// 生成一张 2x2 的索引色 PNG，作为读取、改点和裁剪演示输入
std::vector<uint8_t> createDemoPngData()
{
	const std::array<uint8_t, 8> arrU8t_Signature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	std::vector<uint8_t> vecU8t_PngData(arrU8t_Signature.begin(), arrU8t_Signature.end());

	std::vector<uint8_t> vecU8t_IhdrData;
	appendBigEndianUint32(vecU8t_IhdrData, 2U);
	appendBigEndianUint32(vecU8t_IhdrData, 2U);
	vecU8t_IhdrData.push_back(8U);
	vecU8t_IhdrData.push_back(3U);
	vecU8t_IhdrData.push_back(0U);
	vecU8t_IhdrData.push_back(0U);
	vecU8t_IhdrData.push_back(0U);
	appendChunk(vecU8t_PngData, {'I', 'H', 'D', 'R'}, vecU8t_IhdrData);

	const std::vector<uint8_t> vecU8t_PlteData = {
		0xFFU, 0x00U, 0x00U,
		0x00U, 0xFFU, 0x00U,
		0x00U, 0x00U, 0xFFU,
		0xFFU, 0xFFU, 0xFFU
	};
	appendChunk(vecU8t_PngData, {'P', 'L', 'T', 'E'}, vecU8t_PlteData);

	const std::vector<uint8_t> vecU8t_RawScanlines = {
		0x00U, 0x00U, 0x01U,
		0x00U, 0x02U, 0x03U
	};
	appendChunk(vecU8t_PngData, {'I', 'D', 'A', 'T'}, createStoredZlibStream(vecU8t_RawScanlines));
	appendChunk(vecU8t_PngData, {'I', 'E', 'N', 'D'}, {});
	return vecU8t_PngData;
}

// 将内存中的 PNG 字节写到磁盘，供 ST_png 读取
void writeBytesToFile(const std::filesystem::path& fsPath_FilePath, const std::vector<uint8_t>& vecU8t_FileData)
{
	std::ofstream fs_OutFile(fsPath_FilePath, std::ios::binary | std::ios::trunc);
	if (!fs_OutFile)
	{
		throw std::runtime_error("Failed to create demo PNG file");
	}
	fs_OutFile.write(reinterpret_cast<const char*>(vecU8t_FileData.data()),
		static_cast<std::streamsize>(vecU8t_FileData.size()));
	if (!fs_OutFile)
	{
		throw std::runtime_error("Failed to write demo PNG file");
	}
}

// 以行列形式打印当前解码后的 RGBA 像素
void printPixels(const ST_png& stPng_File)
{
	const auto& vecSt_Pixels = stPng_File.pixels();
	const uint32_t u32t_Width = stPng_File.header().u32t_Width;
	const uint32_t u32t_Height = stPng_File.header().u32t_Height;

	for (uint32_t u32t_Y = 0U; u32t_Y < u32t_Height; ++u32t_Y)
	{
		std::cout << "  Row " << u32t_Y << ": ";
		for (uint32_t u32t_X = 0U; u32t_X < u32t_Width; ++u32t_X)
		{
			const ST_png::RgbaPixel& st_Pixel = vecSt_Pixels[static_cast<size_t>(u32t_Y) * u32t_Width + u32t_X];
			std::cout << '('
				<< static_cast<int>(st_Pixel.u8t_Red) << ','
				<< static_cast<int>(st_Pixel.u8t_Green) << ','
				<< static_cast<int>(st_Pixel.u8t_Blue) << ','
				<< static_cast<int>(st_Pixel.u8t_Alpha) << ')';
			if (u32t_X + 1U < u32t_Width)
			{
				std::cout << ' ';
			}
		}
		std::cout << std::endl;
	}
}

// 汇总展示当前 PNG 的尺寸、格式、是否有未保存修改以及像素内容
void printSummary(const std::string& str_Title, const ST_png& stPng_File)
{
	size_t st_FileSize = 0U;
	if (stPng_File.size(st_FileSize) != FileHintCode::Success)
	{
		throw std::runtime_error("Failed to query PNG file size");
	}

	std::cout << str_Title << std::endl;
	std::cout << "  Path: " << stPng_File.filePath() << std::endl;
	std::cout << "  File size: " << st_FileSize << " bytes" << std::endl;
	std::cout << "  Width x Height: " << stPng_File.header().u32t_Width << " x "
		<< stPng_File.header().u32t_Height << std::endl;
	std::cout << "  Dirty in memory: " << (stPng_File.isDirty() ? "true" : "false") << std::endl;
	std::cout << "  Bit depth / Color type: "
		<< static_cast<int>(stPng_File.header().u8t_BitDepth) << " / "
		<< static_cast<int>(stPng_File.header().u8t_ColorType) << std::endl;
	std::cout << "  Palette entries: " << stPng_File.paletteEntries().size() << std::endl;
	std::cout << "  Combined IDAT size: " << stPng_File.imageData().size() << " bytes" << std::endl;
	std::cout << "  Pixels:" << std::endl;
	printPixels(stPng_File);
}
}

int main()
{
	try
	{
		// 1. 先生成一张可控的小 PNG，避免依赖仓库外资源文件
		const std::filesystem::path fsPath_SourceFile = std::filesystem::current_path() / "PngTool_demo_source.png";
		const std::filesystem::path fsPath_ModifiedFile = std::filesystem::current_path() / "PngTool_demo_modified.png";
		const std::filesystem::path fsPath_CroppedFile = std::filesystem::current_path() / "PngTool_demo_cropped.png";

		writeBytesToFile(fsPath_SourceFile, createDemoPngData());

		// 2. 读取原始 PNG，展示解码后的像素内容
		ST_png stPng_Source(fsPath_SourceFile.string().c_str());
		printSummary("Original PNG", stPng_Source);

		// 3. 仅在内存里修改指定坐标，不会影响源图文件
		ST_png::RgbaPixel st_BlackPixel;
		st_BlackPixel.u8t_Red = 0U;
		st_BlackPixel.u8t_Green = 0U;
		st_BlackPixel.u8t_Blue = 0U;
		st_BlackPixel.u8t_Alpha = 255U;
		stPng_Source.setPixel(1U, 0U, st_BlackPixel);
		printSummary("After setPixel in memory only", stPng_Source);

		// 4. 显式另存为新图，原图路径与原图文件保持不变
		stPng_Source.saveAs(fsPath_ModifiedFile.string().c_str());
		ST_png stPng_Modified(fsPath_ModifiedFile.string().c_str());
		printSummary("Reloaded saveAs result", stPng_Modified);

		// 5. 重新读取源图，确认原图文件没有被破坏
		ST_png stPng_SourceReloaded(fsPath_SourceFile.string().c_str());
		printSummary("Reloaded original PNG", stPng_SourceReloaded);

		// 6. 对当前内存工作副本进行裁剪，并再次另存输出
		stPng_Source.crop(0U, 1U, 2U, 1U);
		stPng_Source.saveAs(fsPath_CroppedFile.string().c_str());
		printSummary("After crop in memory only", stPng_Source);

		// 7. 重新读取裁剪后的结果，确认另存文件可再次正确解析
		ST_png stPng_Reloaded(fsPath_CroppedFile.string().c_str());
		printSummary("Reloaded cropped PNG", stPng_Reloaded);
	}
	catch (const std::exception& e_Exception)
	{
		std::cout << "PngToolDemo failed: " << e_Exception.what() << std::endl;
		return 1;
	}

	return 0;
}
