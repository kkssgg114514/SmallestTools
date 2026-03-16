#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../public/ST_png.h"

namespace
{
// PNG 规范中的固定签名与关键块类型常量
constexpr std::array<uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::array<char, 4> kIhdrChunkType = {'I', 'H', 'D', 'R'};
constexpr std::array<char, 4> kPlteChunkType = {'P', 'L', 'T', 'E'};
constexpr std::array<char, 4> kIdatChunkType = {'I', 'D', 'A', 'T'};
constexpr std::array<char, 4> kIendChunkType = {'I', 'E', 'N', 'D'};
constexpr std::array<char, 4> kTrnsChunkType = {'t', 'R', 'N', 'S'};
constexpr size_t kIhdrDataSize = 13;

const int kCodeLengthOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
const int kLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLengthExtraBits[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistanceBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int kDistanceExtraBits[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// 将 FileTools 的错误码转换成异常文本，便于统一抛错
const char* fileHintCodeToString(FileHintCode fhc_Code)
{
	switch (fhc_Code)
	{
	case FileHintCode::Success:
		return "success";
	case FileHintCode::NotFound:
		return "not found";
	case FileHintCode::ReadError:
		return "read error";
	case FileHintCode::OpenError:
		return "open error";
	case FileHintCode::WriteError:
		return "write error";
	case FileHintCode::InvalidParameter:
		return "invalid parameter";
	default:
		return "unknown error";
	}
}

// 读取文件类返回错误码时，统一升级为异常
void throwIfFileError(FileHintCode fhc_Code, const char* pSz_Context)
{
	if (fhc_Code == FileHintCode::Success)
	{
		return;
	}

	throw std::runtime_error(std::string(pSz_Context) + ": " + fileHintCodeToString(fhc_Code));
}

// 校验输入 PNG 的路径、文件存在性和扩展名
std::filesystem::path validateInputFilePath(const char* pSz_FilePath)
{
	if (pSz_FilePath == nullptr || pSz_FilePath[0] == '\0')
	{
		throw std::invalid_argument("PNG file path is empty");
	}

	const std::filesystem::path fsPath_FilePath(pSz_FilePath);
	std::error_code ec_ErrorCode;
	if (!std::filesystem::exists(fsPath_FilePath, ec_ErrorCode))
	{
		if (ec_ErrorCode)
		{
			throw std::runtime_error("Failed to query PNG file path");
		}
		throw std::runtime_error("PNG file does not exist");
	}
	if (!std::filesystem::is_regular_file(fsPath_FilePath, ec_ErrorCode))
	{
		if (ec_ErrorCode)
		{
			throw std::runtime_error("Failed to query PNG file type");
		}
		throw std::runtime_error("PNG file path is not a regular file");
	}

	std::string str_Extension = fsPath_FilePath.extension().string();
	std::transform(str_Extension.begin(), str_Extension.end(), str_Extension.begin(),
		[](unsigned char u8t_Char)
		{
			return static_cast<char>(std::tolower(u8t_Char));
		});
	if (str_Extension != ".png")
	{
		throw std::runtime_error("PNG file extension is invalid");
	}

	return fsPath_FilePath;
}

// 校验输出路径，确保保存目标仍然是 PNG 文件
std::filesystem::path validateOutputFilePath(const char* pSz_FilePath)
{
	if (pSz_FilePath == nullptr || pSz_FilePath[0] == '\0')
	{
		throw std::invalid_argument("Output PNG file path is empty");
	}

	const std::filesystem::path fsPath_FilePath(pSz_FilePath);
	std::string str_Extension = fsPath_FilePath.extension().string();
	std::transform(str_Extension.begin(), str_Extension.end(), str_Extension.begin(),
		[](unsigned char u8t_Char)
		{
			return static_cast<char>(std::tolower(u8t_Char));
		});
	if (str_Extension != ".png")
	{
		throw std::runtime_error("Output PNG file extension is invalid");
	}
	if (!fsPath_FilePath.has_parent_path())
	{
		return fsPath_FilePath;
	}

	std::error_code ec_ErrorCode;
	const std::filesystem::path fsPath_Parent = fsPath_FilePath.parent_path();
	if (!fsPath_Parent.empty() && !std::filesystem::exists(fsPath_Parent, ec_ErrorCode))
	{
		if (ec_ErrorCode)
		{
			throw std::runtime_error("Failed to query output PNG directory");
		}
		throw std::runtime_error("Output PNG directory does not exist");
	}
	if (!fsPath_Parent.empty() && !std::filesystem::is_directory(fsPath_Parent, ec_ErrorCode))
	{
		if (ec_ErrorCode)
		{
			throw std::runtime_error("Failed to query output PNG directory type");
		}
		throw std::runtime_error("Output PNG parent path is not a directory");
	}

	return fsPath_FilePath;
}

// 仅用于日志和异常信息中的块类型展示
std::string chunkTypeToString(const std::array<char, 4>& arrC_Type)
{
	return std::string(arrC_Type.begin(), arrC_Type.end());
}

// PNG 文件内所有多字节整数字段均采用大端序
uint32_t readBigEndianUint32(const uint8_t* pU8t_Data)
{
	return (static_cast<uint32_t>(pU8t_Data[0]) << 24U) |
		(static_cast<uint32_t>(pU8t_Data[1]) << 16U) |
		(static_cast<uint32_t>(pU8t_Data[2]) << 8U) |
		static_cast<uint32_t>(pU8t_Data[3]);
}

uint16_t readBigEndianUint16(const uint8_t* pU8t_Data)
{
	return static_cast<uint16_t>((static_cast<uint16_t>(pU8t_Data[0]) << 8U) |
		static_cast<uint16_t>(pU8t_Data[1]));
}

// 编码 PNG 时复用的大端写入工具
void appendBigEndianUint32(std::vector<uint8_t>& refVecU8t_OutData, uint32_t u32t_Value)
{
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 24U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 16U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>((u32t_Value >> 8U) & 0xFFU));
	refVecU8t_OutData.push_back(static_cast<uint8_t>(u32t_Value & 0xFFU));
}

// 用于校验 PNG 数据块的 CRC32
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

// zlib 尾部使用 Adler32 保存原始数据校验和
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

// PNG 块类型码必须全部由字母组成
bool isChunkTypeBytesValid(const std::array<char, 4>& arrC_Type)
{
	for (const char c_Byte : arrC_Type)
	{
		if (!std::isalpha(static_cast<unsigned char>(c_Byte)))
		{
			return false;
		}
	}
	return true;
}

// 根据颜色类型推导每个像素包含的样本数量
size_t sampleCountForColorType(uint8_t u8t_ColorType)
{
	switch (u8t_ColorType)
	{
	case 0:
		return 1;
	case 2:
		return 3;
	case 3:
		return 1;
	case 4:
		return 2;
	case 6:
		return 4;
	default:
		throw std::runtime_error("Unsupported PNG color type");
	}
}

// 一个像素总共占用多少 bit，用于解码和反滤波计算
size_t bitsPerPixel(const ST_png::HeaderChunkData& st_HeaderChunkData)
{
	return sampleCountForColorType(st_HeaderChunkData.u8t_ColorType) * st_HeaderChunkData.u8t_BitDepth;
}

// 每一行图像数据实际占用多少字节，不包含过滤器字节
size_t bytesPerScanline(const ST_png::HeaderChunkData& st_HeaderChunkData)
{
	const uint64_t u64t_BitsPerRow = static_cast<uint64_t>(st_HeaderChunkData.u32t_Width) *
		static_cast<uint64_t>(bitsPerPixel(st_HeaderChunkData));
	if (u64t_BitsPerRow > (std::numeric_limits<size_t>::max() - 7U))
	{
		throw std::runtime_error("PNG row size is too large");
	}
	return static_cast<size_t>((u64t_BitsPerRow + 7U) / 8U);
}

// PNG 反滤波需要的“前一个像素”跨度，低位深时至少按 1 字节处理
size_t bytesPerPixelForFiltering(const ST_png::HeaderChunkData& st_HeaderChunkData)
{
	const size_t st_BitsPerPixel = bitsPerPixel(st_HeaderChunkData);
	return std::max<size_t>(1U, (st_BitsPerPixel + 7U) / 8U);
}

// 仅接受规范允许的位深与颜色类型组合
bool isSupportedBitDepth(uint8_t u8t_BitDepth, uint8_t u8t_ColorType)
{
	switch (u8t_ColorType)
	{
	case 0:
		return u8t_BitDepth == 1 || u8t_BitDepth == 2 || u8t_BitDepth == 4 ||
			u8t_BitDepth == 8 || u8t_BitDepth == 16;
	case 2:
	case 4:
	case 6:
		return u8t_BitDepth == 8 || u8t_BitDepth == 16;
	case 3:
		return u8t_BitDepth == 1 || u8t_BitDepth == 2 || u8t_BitDepth == 4 || u8t_BitDepth == 8;
	default:
		return false;
	}
}

// 将不同位深的样本值统一缩放到 8 位通道
uint8_t scaleSampleToByte(uint32_t u32t_Sample, uint8_t u8t_BitDepth)
{
	if (u8t_BitDepth == 8U)
	{
		return static_cast<uint8_t>(u32t_Sample);
	}
	if (u8t_BitDepth == 16U)
	{
		return static_cast<uint8_t>(u32t_Sample >> 8U);
	}

	const uint32_t u32t_MaxSample = (1U << u8t_BitDepth) - 1U;
	return static_cast<uint8_t>((u32t_Sample * 255U + u32t_MaxSample / 2U) / u32t_MaxSample);
}

// 读取 1/2/4 bit 打包样本，用于索引色和低位深灰度图
uint32_t readPackedSample(const uint8_t* pU8t_RowData, size_t st_PixelIndex, uint8_t u8t_BitDepth)
{
	if (u8t_BitDepth >= 8U)
	{
		throw std::runtime_error("Packed sample reader only supports bit depths below 8");
	}

	const size_t st_BitOffset = st_PixelIndex * static_cast<size_t>(u8t_BitDepth);
	const size_t st_ByteIndex = st_BitOffset / 8U;
	const size_t st_BitIndexInByte = st_BitOffset % 8U;
	const uint8_t u8t_Shift = static_cast<uint8_t>(8U - u8t_BitDepth - st_BitIndexInByte);
	const uint8_t u8t_Mask = static_cast<uint8_t>((1U << u8t_BitDepth) - 1U);
	return static_cast<uint32_t>((pU8t_RowData[st_ByteIndex] >> u8t_Shift) & u8t_Mask);
}

// 先用签名快速确认这确实是一份 PNG 文件
void validateSignature(const uint8_t* pU8t_FileData, size_t st_FileSize)
{
	if (st_FileSize < kPngSignature.size())
	{
		throw std::runtime_error("PNG file is smaller than the PNG signature");
	}
	if (!std::equal(kPngSignature.begin(), kPngSignature.end(), pU8t_FileData))
	{
		throw std::runtime_error("PNG signature mismatch");
	}
}

// 从字节流中依次取出一个完整的 PNG 块
ST_png::ChunkData readChunk(const uint8_t* pU8t_FileData, size_t st_FileSize, size_t& refSt_Offset)
{
	if (st_FileSize - refSt_Offset < 12U)
	{
		throw std::runtime_error("PNG chunk is truncated");
	}

	ST_png::ChunkData st_ChunkData;
	st_ChunkData.u32t_Length = readBigEndianUint32(pU8t_FileData + refSt_Offset);
	refSt_Offset += 4U;
	for (size_t st_Index = 0U; st_Index < st_ChunkData.arrC_Type.size(); ++st_Index)
	{
		st_ChunkData.arrC_Type[st_Index] = static_cast<char>(pU8t_FileData[refSt_Offset + st_Index]);
	}
	refSt_Offset += st_ChunkData.arrC_Type.size();

	if (!isChunkTypeBytesValid(st_ChunkData.arrC_Type))
	{
		throw std::runtime_error("PNG chunk type is invalid");
	}
	if (st_ChunkData.u32t_Length > st_FileSize - refSt_Offset - 4U)
	{
		throw std::runtime_error("PNG chunk length exceeds file size");
	}

	const uint8_t* const pU8t_DataBegin = pU8t_FileData + refSt_Offset;
	st_ChunkData.vecU8t_Data.assign(pU8t_DataBegin, pU8t_DataBegin + st_ChunkData.u32t_Length);
	refSt_Offset += st_ChunkData.u32t_Length;
	st_ChunkData.u32t_Crc32 = readBigEndianUint32(pU8t_FileData + refSt_Offset);
	refSt_Offset += 4U;
	return st_ChunkData;
}

// 校验块的 CRC32，避免后续解析建立在坏数据上
void validateChunkCrc(const ST_png::ChunkData& st_ChunkData)
{
	std::vector<uint8_t> vecU8t_CrcBuffer;
	vecU8t_CrcBuffer.reserve(st_ChunkData.arrC_Type.size() + st_ChunkData.vecU8t_Data.size());
	for (const char c_Byte : st_ChunkData.arrC_Type)
	{
		vecU8t_CrcBuffer.push_back(static_cast<uint8_t>(c_Byte));
	}
	vecU8t_CrcBuffer.insert(vecU8t_CrcBuffer.end(),
		st_ChunkData.vecU8t_Data.begin(),
		st_ChunkData.vecU8t_Data.end());

	const uint32_t u32t_ExpectedCrc = computeCrc32(vecU8t_CrcBuffer.data(), vecU8t_CrcBuffer.size());
	if (u32t_ExpectedCrc != st_ChunkData.u32t_Crc32)
	{
		throw std::runtime_error("PNG chunk CRC32 mismatch: " + chunkTypeToString(st_ChunkData.arrC_Type));
	}
}

// DEFLATE 是按 bit 读取的，这里提供最小位流读取器
struct BitReader
{
	const std::vector<uint8_t>& vecU8t_Data;
	size_t st_BitPosition = 0U;

	explicit BitReader(const std::vector<uint8_t>& vecU8t_DataRef)
		: vecU8t_Data(vecU8t_DataRef)
	{
	}

	uint32_t readBits(int i_BitCount)
	{
		if (i_BitCount < 0 || i_BitCount > 24)
		{
			throw std::runtime_error("Invalid DEFLATE bit count");
		}

		uint32_t u32t_Value = 0U;
		for (int i = 0; i < i_BitCount; ++i)
		{
			if ((st_BitPosition / 8U) >= vecU8t_Data.size())
			{
				throw std::runtime_error("Unexpected end of DEFLATE stream");
			}
			const uint8_t u8t_CurrentByte = vecU8t_Data[st_BitPosition / 8U];
			const uint8_t u8t_CurrentBit = static_cast<uint8_t>((u8t_CurrentByte >> (st_BitPosition % 8U)) & 1U);
			u32t_Value |= static_cast<uint32_t>(u8t_CurrentBit) << i;
			++st_BitPosition;
		}
		return u32t_Value;
	}

	void alignToByte()
	{
		const size_t st_Remainder = st_BitPosition % 8U;
		if (st_Remainder != 0U)
		{
			st_BitPosition += 8U - st_Remainder;
		}
	}
};

// 运行时构造 Huffman 表，用于解析动态/固定 Huffman 块
struct HuffmanTable
{
	std::vector<std::vector<int>> vecVecI_TableByLength;
	int i_MaxBits = 0;

	int decodeSymbol(BitReader& br_Reader) const
	{
		if (i_MaxBits == 0)
		{
			throw std::runtime_error("Attempted to decode an empty Huffman table");
		}

		int i_Code = 0;
		for (int i_Length = 1; i_Length <= i_MaxBits; ++i_Length)
		{
			i_Code |= static_cast<int>(br_Reader.readBits(1)) << (i_Length - 1);
			if (i_Length < static_cast<int>(vecVecI_TableByLength.size()) &&
				!vecVecI_TableByLength[i_Length].empty() &&
				i_Code < static_cast<int>(vecVecI_TableByLength[i_Length].size()))
			{
				const int i_Symbol = vecVecI_TableByLength[i_Length][i_Code];
				if (i_Symbol >= 0)
				{
					return i_Symbol;
				}
			}
		}

		throw std::runtime_error("Invalid DEFLATE Huffman code");
	}
};

// 根据码长生成可解码的 Huffman 查找表
HuffmanTable buildHuffmanTable(const std::vector<uint8_t>& vecU8t_CodeLengths)
{
	constexpr int kMaxCodeBits = 15;
	std::array<int, kMaxCodeBits + 1> arrI_BlCount = {};
	for (const uint8_t u8t_Length : vecU8t_CodeLengths)
	{
		if (u8t_Length > kMaxCodeBits)
		{
			throw std::runtime_error("DEFLATE Huffman code length is invalid");
		}
		if (u8t_Length != 0U)
		{
			++arrI_BlCount[u8t_Length];
		}
	}

	HuffmanTable ht_Table;
	for (int i_Bits = kMaxCodeBits; i_Bits >= 1; --i_Bits)
	{
		if (arrI_BlCount[i_Bits] != 0)
		{
			ht_Table.i_MaxBits = i_Bits;
			break;
		}
	}
	if (ht_Table.i_MaxBits == 0)
	{
		ht_Table.vecVecI_TableByLength.resize(1);
		return ht_Table;
	}

	std::array<int, kMaxCodeBits + 1> arrI_NextCode = {};
	int i_Code = 0;
	for (int i_Bits = 1; i_Bits <= kMaxCodeBits; ++i_Bits)
	{
		i_Code = (i_Code + arrI_BlCount[i_Bits - 1]) << 1;
		arrI_NextCode[i_Bits] = i_Code;
	}

	ht_Table.vecVecI_TableByLength.resize(static_cast<size_t>(ht_Table.i_MaxBits + 1));
	for (int i_Length = 1; i_Length <= ht_Table.i_MaxBits; ++i_Length)
	{
		ht_Table.vecVecI_TableByLength[i_Length].assign(1U << i_Length, -1);
	}

	for (size_t st_Symbol = 0U; st_Symbol < vecU8t_CodeLengths.size(); ++st_Symbol)
	{
		const uint8_t u8t_Length = vecU8t_CodeLengths[st_Symbol];
		if (u8t_Length == 0U)
		{
			continue;
		}
		const int i_CurrentCode = arrI_NextCode[u8t_Length]++;
		ht_Table.vecVecI_TableByLength[u8t_Length][i_CurrentCode] = static_cast<int>(st_Symbol);
	}

	return ht_Table;
}

// 固定 Huffman 块使用规范定义好的默认表
std::pair<HuffmanTable, HuffmanTable> buildFixedHuffmanTables()
{
	std::vector<uint8_t> vecU8t_LiteralLengths(288U, 0U);
	for (int i = 0; i <= 143; ++i)
	{
		vecU8t_LiteralLengths[i] = 8U;
	}
	for (int i = 144; i <= 255; ++i)
	{
		vecU8t_LiteralLengths[i] = 9U;
	}
	for (int i = 256; i <= 279; ++i)
	{
		vecU8t_LiteralLengths[i] = 7U;
	}
	for (int i = 280; i <= 287; ++i)
	{
		vecU8t_LiteralLengths[i] = 8U;
	}

	std::vector<uint8_t> vecU8t_DistanceLengths(32U, 5U);
	return {buildHuffmanTable(vecU8t_LiteralLengths), buildHuffmanTable(vecU8t_DistanceLengths)};
}

// 动态 Huffman 块需要先从码流中读取码长信息，再构表
std::pair<HuffmanTable, HuffmanTable> buildDynamicHuffmanTables(BitReader& br_Reader)
{
	const int i_Hlit = static_cast<int>(br_Reader.readBits(5)) + 257;
	const int i_Hdist = static_cast<int>(br_Reader.readBits(5)) + 1;
	const int i_Hclen = static_cast<int>(br_Reader.readBits(4)) + 4;

	std::vector<uint8_t> vecU8t_CodeLengthCodeLengths(19U, 0U);
	for (int i = 0; i < i_Hclen; ++i)
	{
		vecU8t_CodeLengthCodeLengths[kCodeLengthOrder[i]] = static_cast<uint8_t>(br_Reader.readBits(3));
	}
	const HuffmanTable ht_CodeLengthTable = buildHuffmanTable(vecU8t_CodeLengthCodeLengths);

	std::vector<uint8_t> vecU8t_AllCodeLengths;
	vecU8t_AllCodeLengths.reserve(static_cast<size_t>(i_Hlit + i_Hdist));
	while (vecU8t_AllCodeLengths.size() < static_cast<size_t>(i_Hlit + i_Hdist))
	{
		const int i_Symbol = ht_CodeLengthTable.decodeSymbol(br_Reader);
		if (i_Symbol <= 15)
		{
			vecU8t_AllCodeLengths.push_back(static_cast<uint8_t>(i_Symbol));
		}
		else if (i_Symbol == 16)
		{
			if (vecU8t_AllCodeLengths.empty())
			{
				throw std::runtime_error("DEFLATE repeat code has no previous length");
			}
			const uint8_t u8t_PreviousLength = vecU8t_AllCodeLengths.back();
			const int i_RepeatCount = static_cast<int>(br_Reader.readBits(2)) + 3;
			for (int i = 0; i < i_RepeatCount; ++i)
			{
				vecU8t_AllCodeLengths.push_back(u8t_PreviousLength);
			}
		}
		else if (i_Symbol == 17)
		{
			const int i_RepeatCount = static_cast<int>(br_Reader.readBits(3)) + 3;
			for (int i = 0; i < i_RepeatCount; ++i)
			{
				vecU8t_AllCodeLengths.push_back(0U);
			}
		}
		else if (i_Symbol == 18)
		{
			const int i_RepeatCount = static_cast<int>(br_Reader.readBits(7)) + 11;
			for (int i = 0; i < i_RepeatCount; ++i)
			{
				vecU8t_AllCodeLengths.push_back(0U);
			}
		}
		else
		{
			throw std::runtime_error("Invalid DEFLATE code length symbol");
		}

		if (vecU8t_AllCodeLengths.size() > static_cast<size_t>(i_Hlit + i_Hdist))
		{
			throw std::runtime_error("DEFLATE code lengths exceed expected count");
		}
	}

	std::vector<uint8_t> vecU8t_LiteralLengths(vecU8t_AllCodeLengths.begin(),
		vecU8t_AllCodeLengths.begin() + i_Hlit);
	std::vector<uint8_t> vecU8t_DistanceLengths(vecU8t_AllCodeLengths.begin() + i_Hlit,
		vecU8t_AllCodeLengths.end());
	if (std::all_of(vecU8t_DistanceLengths.begin(), vecU8t_DistanceLengths.end(),
		[](uint8_t u8t_Length) { return u8t_Length == 0U; }))
	{
		vecU8t_DistanceLengths.assign(1U, 0U);
	}
	return {buildHuffmanTable(vecU8t_LiteralLengths), buildHuffmanTable(vecU8t_DistanceLengths)};
}

// 统一处理 literal/length + distance 的压缩块展开逻辑
void decodeCompressedBlock(BitReader& br_Reader,
	const HuffmanTable& ht_LiteralLengthTable,
	const HuffmanTable& ht_DistanceTable,
	std::vector<uint8_t>& refVecU8t_Output)
{
	while (true)
	{
		const int i_Symbol = ht_LiteralLengthTable.decodeSymbol(br_Reader);
		if (i_Symbol < 256)
		{
			refVecU8t_Output.push_back(static_cast<uint8_t>(i_Symbol));
			continue;
		}
		if (i_Symbol == 256)
		{
			return;
		}
		if (i_Symbol > 285)
		{
			throw std::runtime_error("DEFLATE literal/length symbol is invalid");
		}

		const int i_LengthIndex = i_Symbol - 257;
		size_t st_Length = static_cast<size_t>(kLengthBase[i_LengthIndex]);
		const int i_LengthExtraBits = kLengthExtraBits[i_LengthIndex];
		if (i_LengthExtraBits > 0)
		{
			st_Length += br_Reader.readBits(i_LengthExtraBits);
		}

		const int i_DistanceSymbol = ht_DistanceTable.decodeSymbol(br_Reader);
		if (i_DistanceSymbol < 0 || i_DistanceSymbol > 29)
		{
			throw std::runtime_error("DEFLATE distance symbol is invalid");
		}
		size_t st_Distance = static_cast<size_t>(kDistanceBase[i_DistanceSymbol]);
		const int i_DistanceExtraBits = kDistanceExtraBits[i_DistanceSymbol];
		if (i_DistanceExtraBits > 0)
		{
			st_Distance += br_Reader.readBits(i_DistanceExtraBits);
		}
		if (st_Distance == 0U || st_Distance > refVecU8t_Output.size())
		{
			throw std::runtime_error("DEFLATE back-reference distance is invalid");
		}

		for (size_t st_Index = 0U; st_Index < st_Length; ++st_Index)
		{
			refVecU8t_Output.push_back(refVecU8t_Output[refVecU8t_Output.size() - st_Distance]);
		}
	}
}

// 解开 zlib 包装并执行最小 DEFLATE 解压，供 PNG IDAT 使用
std::vector<uint8_t> inflateZlibData(const std::vector<uint8_t>& vecU8t_ZlibData, size_t st_ExpectedSize)
{
	if (vecU8t_ZlibData.size() < 6U)
	{
		throw std::runtime_error("Zlib stream is too short");
	}

	const uint8_t u8t_Cmf = vecU8t_ZlibData[0];
	const uint8_t u8t_Flg = vecU8t_ZlibData[1];
	if ((u8t_Cmf & 0x0FU) != 8U)
	{
		throw std::runtime_error("Unsupported zlib compression method");
	}
	if (((static_cast<uint16_t>(u8t_Cmf) << 8U) | u8t_Flg) % 31U != 0U)
	{
		throw std::runtime_error("Zlib header checksum is invalid");
	}
	if ((u8t_Flg & 0x20U) != 0U)
	{
		throw std::runtime_error("Preset zlib dictionary is not supported");
	}

	std::vector<uint8_t> vecU8t_CompressedData(vecU8t_ZlibData.begin() + 2, vecU8t_ZlibData.end() - 4);
	BitReader br_Reader(vecU8t_CompressedData);
	std::vector<uint8_t> vecU8t_Output;
	if (st_ExpectedSize > 0U)
	{
		vecU8t_Output.reserve(st_ExpectedSize);
	}

	const auto st_FixedTables = buildFixedHuffmanTables();
	bool b_IsFinalBlock = false;
	while (!b_IsFinalBlock)
	{
		b_IsFinalBlock = br_Reader.readBits(1) != 0U;
		const uint32_t u32t_BlockType = br_Reader.readBits(2);
		if (u32t_BlockType == 0U)
		{
			br_Reader.alignToByte();
			const uint32_t u32t_Length = br_Reader.readBits(16);
			const uint32_t u32t_NLength = br_Reader.readBits(16);
			if ((u32t_Length ^ 0xFFFFU) != u32t_NLength)
			{
				throw std::runtime_error("DEFLATE stored block length check failed");
			}
			for (uint32_t u32t_Index = 0U; u32t_Index < u32t_Length; ++u32t_Index)
			{
				vecU8t_Output.push_back(static_cast<uint8_t>(br_Reader.readBits(8)));
			}
		}
		else if (u32t_BlockType == 1U)
		{
			decodeCompressedBlock(br_Reader, st_FixedTables.first, st_FixedTables.second, vecU8t_Output);
		}
		else if (u32t_BlockType == 2U)
		{
			const auto st_DynamicTables = buildDynamicHuffmanTables(br_Reader);
			decodeCompressedBlock(br_Reader, st_DynamicTables.first, st_DynamicTables.second, vecU8t_Output);
		}
		else
		{
			throw std::runtime_error("DEFLATE block type is reserved");
		}
	}

	const uint32_t u32t_ExpectedAdler32 = readBigEndianUint32(vecU8t_ZlibData.data() + vecU8t_ZlibData.size() - 4U);
	const uint32_t u32t_ActualAdler32 = computeAdler32(vecU8t_Output.data(), vecU8t_Output.size());
	if (u32t_ExpectedAdler32 != u32t_ActualAdler32)
	{
		throw std::runtime_error("Zlib Adler32 checksum mismatch");
	}
	if (st_ExpectedSize != 0U && vecU8t_Output.size() != st_ExpectedSize)
	{
		throw std::runtime_error("Inflated image data size does not match the expected PNG size");
	}

	return vecU8t_Output;
}

// 根据每行开头的过滤器类型，恢复 PNG 原始扫描线
std::vector<uint8_t> unfilterScanlines(const std::vector<uint8_t>& vecU8t_FilteredData,
	const ST_png::HeaderChunkData& st_HeaderChunkData)
{
	const size_t st_RowBytes = bytesPerScanline(st_HeaderChunkData);
	const size_t st_BytesPerPixel = bytesPerPixelForFiltering(st_HeaderChunkData);
	const size_t st_ExpectedSize = static_cast<size_t>(st_HeaderChunkData.u32t_Height) * (1U + st_RowBytes);
	if (vecU8t_FilteredData.size() != st_ExpectedSize)
	{
		throw std::runtime_error("Filtered PNG data size is invalid");
	}

	std::vector<uint8_t> vecU8t_Reconstructed(st_RowBytes * st_HeaderChunkData.u32t_Height, 0U);
	size_t st_FilteredOffset = 0U;
	for (uint32_t u32t_Row = 0U; u32t_Row < st_HeaderChunkData.u32t_Height; ++u32t_Row)
	{
		const uint8_t u8t_FilterType = vecU8t_FilteredData[st_FilteredOffset++];
		const uint8_t* const pU8t_FilteredRow = vecU8t_FilteredData.data() + st_FilteredOffset;
		uint8_t* const pU8t_CurrentRow = vecU8t_Reconstructed.data() + static_cast<size_t>(u32t_Row) * st_RowBytes;
		const uint8_t* const pU8t_PreviousRow = (u32t_Row == 0U) ? nullptr : (pU8t_CurrentRow - st_RowBytes);

		for (size_t st_Column = 0U; st_Column < st_RowBytes; ++st_Column)
		{
			const uint8_t u8t_Left = (st_Column >= st_BytesPerPixel) ? pU8t_CurrentRow[st_Column - st_BytesPerPixel] : 0U;
			const uint8_t u8t_Up = (pU8t_PreviousRow != nullptr) ? pU8t_PreviousRow[st_Column] : 0U;
			const uint8_t u8t_UpLeft = (pU8t_PreviousRow != nullptr && st_Column >= st_BytesPerPixel) ?
				pU8t_PreviousRow[st_Column - st_BytesPerPixel] : 0U;

			switch (u8t_FilterType)
			{
			case 0:
				pU8t_CurrentRow[st_Column] = pU8t_FilteredRow[st_Column];
				break;
			case 1:
				pU8t_CurrentRow[st_Column] = static_cast<uint8_t>(pU8t_FilteredRow[st_Column] + u8t_Left);
				break;
			case 2:
				pU8t_CurrentRow[st_Column] = static_cast<uint8_t>(pU8t_FilteredRow[st_Column] + u8t_Up);
				break;
			case 3:
				pU8t_CurrentRow[st_Column] = static_cast<uint8_t>(pU8t_FilteredRow[st_Column] +
					static_cast<uint8_t>((static_cast<uint16_t>(u8t_Left) + u8t_Up) / 2U));
				break;
			case 4:
			{
				const int i_Predictor = static_cast<int>(u8t_Left) + static_cast<int>(u8t_Up) - static_cast<int>(u8t_UpLeft);
				const int i_Pa = std::abs(i_Predictor - static_cast<int>(u8t_Left));
				const int i_Pb = std::abs(i_Predictor - static_cast<int>(u8t_Up));
				const int i_Pc = std::abs(i_Predictor - static_cast<int>(u8t_UpLeft));
				const uint8_t u8t_Paeth = (i_Pa <= i_Pb && i_Pa <= i_Pc) ? u8t_Left : (i_Pb <= i_Pc ? u8t_Up : u8t_UpLeft);
				pU8t_CurrentRow[st_Column] = static_cast<uint8_t>(pU8t_FilteredRow[st_Column] + u8t_Paeth);
				break;
			}
			default:
				throw std::runtime_error("Unsupported PNG filter type");
			}
		}

		st_FilteredOffset += st_RowBytes;
	}

	return vecU8t_Reconstructed;
}

// 重写时采用“未压缩 DEFLATE 块”重新打包 zlib 数据，保持实现简单可控
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

// 输出 PNG 时复用的块写入逻辑
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
}

// 解析并校验 IHDR，拿到整张图像的基础参数
ST_png::HeaderChunkData ST_png::parseAndValidateHeaderChunk(const ChunkData& st_ChunkData)
{
	if (st_ChunkData.arrC_Type != kIhdrChunkType)
	{
		throw std::runtime_error("The first PNG chunk is not IHDR");
	}
	if (st_ChunkData.u32t_Length != kIhdrDataSize || st_ChunkData.vecU8t_Data.size() != kIhdrDataSize)
	{
		throw std::runtime_error("IHDR chunk length is invalid");
	}

	const uint8_t* const pU8t_IhdrData = st_ChunkData.vecU8t_Data.data();
	HeaderChunkData st_HeaderChunkDataValue;
	st_HeaderChunkDataValue.u32t_Width = readBigEndianUint32(pU8t_IhdrData);
	st_HeaderChunkDataValue.u32t_Height = readBigEndianUint32(pU8t_IhdrData + 4U);
	st_HeaderChunkDataValue.u8t_BitDepth = pU8t_IhdrData[8];
	st_HeaderChunkDataValue.u8t_ColorType = pU8t_IhdrData[9];
	st_HeaderChunkDataValue.u8t_CompressionMethod = pU8t_IhdrData[10];
	st_HeaderChunkDataValue.u8t_FilterMethod = pU8t_IhdrData[11];
	st_HeaderChunkDataValue.u8t_InterlaceMethod = pU8t_IhdrData[12];

	if (st_HeaderChunkDataValue.u32t_Width == 0U || st_HeaderChunkDataValue.u32t_Height == 0U)
	{
		throw std::runtime_error("PNG width and height must be greater than zero");
	}
	if (!isSupportedBitDepth(st_HeaderChunkDataValue.u8t_BitDepth, st_HeaderChunkDataValue.u8t_ColorType))
	{
		throw std::runtime_error("PNG bit depth and color type combination is invalid");
	}
	if (st_HeaderChunkDataValue.u8t_CompressionMethod != 0U)
	{
		throw std::runtime_error("Unsupported PNG compression method");
	}
	if (st_HeaderChunkDataValue.u8t_FilterMethod != 0U)
	{
		throw std::runtime_error("Unsupported PNG filter method");
	}
	if (st_HeaderChunkDataValue.u8t_InterlaceMethod > 1U)
	{
		throw std::runtime_error("Unsupported PNG interlace method");
	}

	return st_HeaderChunkDataValue;
}

// 解析并校验 PLTE，将索引色表保存为易用的 RGB 项列表
std::vector<ST_png::PaletteEntry> ST_png::parseAndValidatePaletteChunk(const ChunkData& st_ChunkData,
	const HeaderChunkData& st_HeaderChunkDataValue)
{
	if (st_ChunkData.arrC_Type != kPlteChunkType)
	{
		throw std::runtime_error("Unexpected palette chunk type");
	}
	if (st_ChunkData.vecU8t_Data.empty() || (st_ChunkData.vecU8t_Data.size() % 3U) != 0U)
	{
		throw std::runtime_error("PLTE chunk length is invalid");
	}
	if (st_HeaderChunkDataValue.u8t_ColorType == 0U || st_HeaderChunkDataValue.u8t_ColorType == 4U)
	{
		throw std::runtime_error("PLTE chunk is not allowed for grayscale PNG");
	}

	const size_t st_EntryCount = st_ChunkData.vecU8t_Data.size() / 3U;
	if (st_EntryCount > 256U)
	{
		throw std::runtime_error("PLTE chunk contains too many entries");
	}
	if (st_HeaderChunkDataValue.u8t_ColorType == 3U)
	{
		const size_t st_MaxEntryCount = static_cast<size_t>(1U << st_HeaderChunkDataValue.u8t_BitDepth);
		if (st_EntryCount > st_MaxEntryCount)
		{
			throw std::runtime_error("PLTE chunk contains more entries than the indexed color bit depth allows");
		}
	}

	std::vector<PaletteEntry> vecSt_PaletteEntriesValue;
	vecSt_PaletteEntriesValue.reserve(st_EntryCount);
	for (size_t st_Index = 0U; st_Index < st_ChunkData.vecU8t_Data.size(); st_Index += 3U)
	{
		PaletteEntry st_Entry;
		st_Entry.u8t_Red = st_ChunkData.vecU8t_Data[st_Index];
		st_Entry.u8t_Green = st_ChunkData.vecU8t_Data[st_Index + 1U];
		st_Entry.u8t_Blue = st_ChunkData.vecU8t_Data[st_Index + 2U];
		vecSt_PaletteEntriesValue.push_back(st_Entry);
	}

	return vecSt_PaletteEntriesValue;
}

// 解析 tRNS，兼容调色板透明度和无 Alpha 图像的透明色定义
ST_png::TransparencyData ST_png::parseAndValidateTransparencyChunk(const ChunkData& st_ChunkData,
	const HeaderChunkData& st_HeaderChunkDataValue,
	size_t st_PaletteEntryCount)
{
	if (st_ChunkData.arrC_Type != kTrnsChunkType)
	{
		throw std::runtime_error("Unexpected transparency chunk type");
	}
	if (st_HeaderChunkDataValue.u8t_ColorType == 4U || st_HeaderChunkDataValue.u8t_ColorType == 6U)
	{
		throw std::runtime_error("tRNS chunk is not allowed for PNG data that already carries alpha");
	}

	TransparencyData st_TransparencyDataValue;
	if (st_HeaderChunkDataValue.u8t_ColorType == 3U)
	{
		if (st_ChunkData.vecU8t_Data.size() > st_PaletteEntryCount)
		{
			throw std::runtime_error("tRNS palette alpha length exceeds the palette size");
		}
		st_TransparencyDataValue.b_HasPaletteAlpha = true;
		st_TransparencyDataValue.vecU8t_PaletteAlpha = st_ChunkData.vecU8t_Data;
	}
	else if (st_HeaderChunkDataValue.u8t_ColorType == 0U)
	{
		if (st_ChunkData.vecU8t_Data.size() != 2U)
		{
			throw std::runtime_error("Grayscale tRNS chunk length is invalid");
		}
		st_TransparencyDataValue.b_HasGrayTransparent = true;
		st_TransparencyDataValue.u16t_GrayTransparent = readBigEndianUint16(st_ChunkData.vecU8t_Data.data());
	}
	else if (st_HeaderChunkDataValue.u8t_ColorType == 2U)
	{
		if (st_ChunkData.vecU8t_Data.size() != 6U)
		{
			throw std::runtime_error("Truecolor tRNS chunk length is invalid");
		}
		st_TransparencyDataValue.b_HasRgbTransparent = true;
		st_TransparencyDataValue.u16t_RedTransparent = readBigEndianUint16(st_ChunkData.vecU8t_Data.data());
		st_TransparencyDataValue.u16t_GreenTransparent = readBigEndianUint16(st_ChunkData.vecU8t_Data.data() + 2U);
		st_TransparencyDataValue.u16t_BlueTransparent = readBigEndianUint16(st_ChunkData.vecU8t_Data.data() + 4U);
	}
	else
	{
		throw std::runtime_error("Unsupported PNG color type for tRNS");
	}

	return st_TransparencyDataValue;
}

// 构造时直接完成整条读取链路，保证对象一旦生成即可编辑
ST_png::ST_png(const char* pSz_FilePath)
{
	const std::filesystem::path fsPath_FilePath = validateInputFilePath(pSz_FilePath);
	loadFromPath(fsPath_FilePath.string());
}

// 从指定路径重新加载 PNG，用于构造或显式重新读取文件
void ST_png::loadFromPath(const std::string& str_FilePathValue)
{
	p_File = std::make_unique<FileOperation>(str_FilePathValue.c_str());
	const uint8_t* pU8t_FileData = nullptr;
	size_t st_FileSize = 0U;
	throwIfFileError(p_File->data(&pU8t_FileData, st_FileSize), "Failed to read PNG file data");
	str_SourceFilePath = str_FilePathValue;
	parsePngBytes(pU8t_FileData, st_FileSize);
	b_IsDirty = false;
}

// 每次重新解析前清空上一次的块、调色板、像素和透明度信息
void ST_png::resetParsedState()
{
	arrU8t_Signature.fill(0U);
	st_HeaderChunkData = {};
	vecSt_PaletteEntries.clear();
	vecU8t_ImageData.clear();
	vecSt_Chunks.clear();
	vecSt_AncillaryChunks.clear();
	vecSt_Pixels.clear();
	st_TransparencyData = {};
	b_HasPalette = false;
	b_HasImageData = false;
	b_HasEndChunk = false;
	b_IsDirty = false;
}

// 顺序扫描 PNG 各个块，并把结构化信息缓存到成员变量中
void ST_png::parsePngBytes(const uint8_t* pU8t_FileData, size_t st_FileSize)
{
	resetParsedState();
	validateSignature(pU8t_FileData, st_FileSize);
	std::copy(kPngSignature.begin(), kPngSignature.end(), arrU8t_Signature.begin());

	bool b_HasHeaderChunk = false;
	bool b_HasPaletteChunk = false;
	bool b_HasIdatChunk = false;
	bool b_IdatSequenceClosed = false;
	bool b_HasTransparencyChunk = false;
	size_t st_Offset = arrU8t_Signature.size();
	while (st_Offset < st_FileSize)
	{
		ChunkData st_ChunkData = readChunk(pU8t_FileData, st_FileSize, st_Offset);
		validateChunkCrc(st_ChunkData);

		if (b_HasEndChunk)
		{
			throw std::runtime_error("PNG contains data after IEND");
		}

		if (st_ChunkData.arrC_Type == kIhdrChunkType)
		{
			if (!vecSt_Chunks.empty() || b_HasHeaderChunk)
			{
				throw std::runtime_error("IHDR chunk must appear exactly once and be the first chunk");
			}
			st_HeaderChunkData = parseAndValidateHeaderChunk(st_ChunkData);
			b_HasHeaderChunk = true;
		}
		else if (st_ChunkData.arrC_Type == kPlteChunkType)
		{
			if (!b_HasHeaderChunk || b_HasIdatChunk || b_HasEndChunk)
			{
				throw std::runtime_error("PLTE chunk must appear after IHDR and before IDAT");
			}
			if (b_HasPaletteChunk)
			{
				throw std::runtime_error("PLTE chunk must not appear more than once");
			}
			vecSt_PaletteEntries = parseAndValidatePaletteChunk(st_ChunkData, st_HeaderChunkData);
			b_HasPalette = true;
			b_HasPaletteChunk = true;
		}
		else if (st_ChunkData.arrC_Type == kTrnsChunkType)
		{
			if (!b_HasHeaderChunk || b_HasIdatChunk || b_HasEndChunk)
			{
				throw std::runtime_error("tRNS chunk must appear before IDAT");
			}
			if (b_HasTransparencyChunk)
			{
				throw std::runtime_error("tRNS chunk must not appear more than once");
			}
			st_TransparencyData = parseAndValidateTransparencyChunk(st_ChunkData,
				st_HeaderChunkData,
				vecSt_PaletteEntries.size());
			b_HasTransparencyChunk = true;
			vecSt_AncillaryChunks.push_back(st_ChunkData);
		}
		else if (st_ChunkData.arrC_Type == kIdatChunkType)
		{
			if (!b_HasHeaderChunk || b_HasEndChunk)
			{
				throw std::runtime_error("IDAT chunk must appear after IHDR and before IEND");
			}
			if (st_HeaderChunkData.u8t_ColorType == 3U && !b_HasPalette)
			{
				throw std::runtime_error("Indexed-color PNG must provide PLTE before IDAT");
			}
			if (b_IdatSequenceClosed)
			{
				throw std::runtime_error("All IDAT chunks must be consecutive");
			}
			vecU8t_ImageData.insert(vecU8t_ImageData.end(),
				st_ChunkData.vecU8t_Data.begin(),
				st_ChunkData.vecU8t_Data.end());
			b_HasImageData = true;
			b_HasIdatChunk = true;
		}
		else if (st_ChunkData.arrC_Type == kIendChunkType)
		{
			if (!b_HasHeaderChunk || !b_HasIdatChunk)
			{
				throw std::runtime_error("IEND chunk must appear after IHDR and IDAT");
			}
			if (st_ChunkData.u32t_Length != 0U || !st_ChunkData.vecU8t_Data.empty())
			{
				throw std::runtime_error("IEND chunk length must be zero");
			}
			b_HasEndChunk = true;
		}
		else
		{
			if (!b_HasHeaderChunk)
			{
				throw std::runtime_error("PNG contains chunks before IHDR");
			}
			if (isCriticalChunk(st_ChunkData.arrC_Type))
			{
				throw std::runtime_error("Unsupported critical PNG chunk: " + chunkTypeToString(st_ChunkData.arrC_Type));
			}
			vecSt_AncillaryChunks.push_back(st_ChunkData);
		}

		if (b_HasIdatChunk &&
			st_ChunkData.arrC_Type != kIdatChunkType &&
			st_ChunkData.arrC_Type != kIendChunkType)
		{
			b_IdatSequenceClosed = true;
		}

		vecSt_Chunks.push_back(std::move(st_ChunkData));
	}

	if (!b_HasHeaderChunk)
	{
		throw std::runtime_error("PNG is missing IHDR chunk");
	}
	if (!b_HasIdatChunk)
	{
		throw std::runtime_error("PNG is missing IDAT chunk");
	}
	if (!b_HasEndChunk)
	{
		throw std::runtime_error("PNG is missing IEND chunk");
	}
	if (st_HeaderChunkData.u8t_ColorType == 3U && !b_HasPalette)
	{
		throw std::runtime_error("Indexed-color PNG is missing PLTE chunk");
	}

	decodeImageToPixels();
}

// 将 IDAT 解压后的扫描线恢复成统一的 RGBA 像素缓冲，供修改接口使用
void ST_png::decodeImageToPixels()
{
	if (st_HeaderChunkData.u8t_InterlaceMethod != 0U)
	{
		throw std::runtime_error("Interlaced PNG is not supported for pixel editing yet");
	}
	if (st_HeaderChunkData.u8t_BitDepth == 16U)
	{
		throw std::runtime_error("16-bit PNG is not supported for pixel editing yet");
	}

	const size_t st_RowBytes = bytesPerScanline(st_HeaderChunkData);
	const size_t st_FilteredSize = static_cast<size_t>(st_HeaderChunkData.u32t_Height) * (1U + st_RowBytes);
	const std::vector<uint8_t> vecU8t_FilteredData = inflateZlibData(vecU8t_ImageData, st_FilteredSize);
	const std::vector<uint8_t> vecU8t_RawRows = unfilterScanlines(vecU8t_FilteredData, st_HeaderChunkData);

	const uint32_t u32t_Width = st_HeaderChunkData.u32t_Width;
	const uint32_t u32t_Height = st_HeaderChunkData.u32t_Height;
	vecSt_Pixels.assign(static_cast<size_t>(u32t_Width) * u32t_Height, {});

	for (uint32_t u32t_Y = 0U; u32t_Y < u32t_Height; ++u32t_Y)
	{
		const uint8_t* const pU8t_Row = vecU8t_RawRows.data() + static_cast<size_t>(u32t_Y) * st_RowBytes;
		for (uint32_t u32t_X = 0U; u32t_X < u32t_Width; ++u32t_X)
		{
			RgbaPixel st_Pixel;
			if (st_HeaderChunkData.u8t_ColorType == 0U)
			{
				const uint32_t u32t_Sample = (st_HeaderChunkData.u8t_BitDepth < 8U) ?
					readPackedSample(pU8t_Row, u32t_X, st_HeaderChunkData.u8t_BitDepth) :
					static_cast<uint32_t>(pU8t_Row[u32t_X]);
				const uint8_t u8t_Gray = scaleSampleToByte(u32t_Sample, st_HeaderChunkData.u8t_BitDepth);
				st_Pixel.u8t_Red = u8t_Gray;
				st_Pixel.u8t_Green = u8t_Gray;
				st_Pixel.u8t_Blue = u8t_Gray;
				st_Pixel.u8t_Alpha = 255U;
				if (st_TransparencyData.b_HasGrayTransparent &&
					u32t_Sample == static_cast<uint32_t>(st_TransparencyData.u16t_GrayTransparent))
				{
					st_Pixel.u8t_Alpha = 0U;
				}
			}
			else if (st_HeaderChunkData.u8t_ColorType == 2U)
			{
				const size_t st_Offset = static_cast<size_t>(u32t_X) * 3U;
				st_Pixel.u8t_Red = pU8t_Row[st_Offset];
				st_Pixel.u8t_Green = pU8t_Row[st_Offset + 1U];
				st_Pixel.u8t_Blue = pU8t_Row[st_Offset + 2U];
				st_Pixel.u8t_Alpha = 255U;
				if (st_TransparencyData.b_HasRgbTransparent &&
					st_Pixel.u8t_Red == static_cast<uint8_t>(st_TransparencyData.u16t_RedTransparent) &&
					st_Pixel.u8t_Green == static_cast<uint8_t>(st_TransparencyData.u16t_GreenTransparent) &&
					st_Pixel.u8t_Blue == static_cast<uint8_t>(st_TransparencyData.u16t_BlueTransparent))
				{
					st_Pixel.u8t_Alpha = 0U;
				}
			}
			else if (st_HeaderChunkData.u8t_ColorType == 3U)
			{
				const uint32_t u32t_PaletteIndex = (st_HeaderChunkData.u8t_BitDepth < 8U) ?
					readPackedSample(pU8t_Row, u32t_X, st_HeaderChunkData.u8t_BitDepth) :
					static_cast<uint32_t>(pU8t_Row[u32t_X]);
				if (u32t_PaletteIndex >= vecSt_PaletteEntries.size())
				{
					throw std::runtime_error("PNG palette index is out of range");
				}
				const PaletteEntry& st_Entry = vecSt_PaletteEntries[u32t_PaletteIndex];
				st_Pixel.u8t_Red = st_Entry.u8t_Red;
				st_Pixel.u8t_Green = st_Entry.u8t_Green;
				st_Pixel.u8t_Blue = st_Entry.u8t_Blue;
				st_Pixel.u8t_Alpha = (st_TransparencyData.b_HasPaletteAlpha &&
					u32t_PaletteIndex < st_TransparencyData.vecU8t_PaletteAlpha.size()) ?
					st_TransparencyData.vecU8t_PaletteAlpha[u32t_PaletteIndex] : 255U;
			}
			else if (st_HeaderChunkData.u8t_ColorType == 4U)
			{
				const size_t st_Offset = static_cast<size_t>(u32t_X) * 2U;
				st_Pixel.u8t_Red = pU8t_Row[st_Offset];
				st_Pixel.u8t_Green = pU8t_Row[st_Offset];
				st_Pixel.u8t_Blue = pU8t_Row[st_Offset];
				st_Pixel.u8t_Alpha = pU8t_Row[st_Offset + 1U];
			}
			else if (st_HeaderChunkData.u8t_ColorType == 6U)
			{
				const size_t st_Offset = static_cast<size_t>(u32t_X) * 4U;
				st_Pixel.u8t_Red = pU8t_Row[st_Offset];
				st_Pixel.u8t_Green = pU8t_Row[st_Offset + 1U];
				st_Pixel.u8t_Blue = pU8t_Row[st_Offset + 2U];
				st_Pixel.u8t_Alpha = pU8t_Row[st_Offset + 3U];
			}
			else
			{
				throw std::runtime_error("Unsupported PNG color type for pixel decoding");
			}

			vecSt_Pixels[static_cast<size_t>(u32t_Y) * u32t_Width + u32t_X] = st_Pixel;
		}
	}
}

// 将当前 RGBA 像素缓冲重新编码成扫描线 + zlib 数据
std::vector<uint8_t> ST_png::encodeCurrentImageData() const
{
	const uint64_t u64t_BytesWithoutFilter = static_cast<uint64_t>(st_HeaderChunkData.u32t_Width) * 4U;
	if (u64t_BytesWithoutFilter > std::numeric_limits<size_t>::max())
	{
		throw std::runtime_error("PNG row width is too large to encode");
	}

	const size_t st_RowBytes = static_cast<size_t>(u64t_BytesWithoutFilter);
	std::vector<uint8_t> vecU8t_FilteredRows;
	vecU8t_FilteredRows.reserve(static_cast<size_t>(st_HeaderChunkData.u32t_Height) * (1U + st_RowBytes));
	for (uint32_t u32t_Y = 0U; u32t_Y < st_HeaderChunkData.u32t_Height; ++u32t_Y)
	{
		vecU8t_FilteredRows.push_back(0U);
		for (uint32_t u32t_X = 0U; u32t_X < st_HeaderChunkData.u32t_Width; ++u32t_X)
		{
			const RgbaPixel& st_Pixel = vecSt_Pixels[static_cast<size_t>(u32t_Y) * st_HeaderChunkData.u32t_Width + u32t_X];
			vecU8t_FilteredRows.push_back(st_Pixel.u8t_Red);
			vecU8t_FilteredRows.push_back(st_Pixel.u8t_Green);
			vecU8t_FilteredRows.push_back(st_Pixel.u8t_Blue);
			vecU8t_FilteredRows.push_back(st_Pixel.u8t_Alpha);
		}
	}

	return createStoredZlibStream(vecU8t_FilteredRows);
}

// 当前实现统一把修改后的图像保存为 8-bit RGBA PNG
std::vector<uint8_t> ST_png::encodePngBytes() const
{
	std::vector<uint8_t> vecU8t_PngData(kPngSignature.begin(), kPngSignature.end());

	std::vector<uint8_t> vecU8t_IhdrData;
	vecU8t_IhdrData.reserve(kIhdrDataSize);
	appendBigEndianUint32(vecU8t_IhdrData, st_HeaderChunkData.u32t_Width);
	appendBigEndianUint32(vecU8t_IhdrData, st_HeaderChunkData.u32t_Height);
	vecU8t_IhdrData.push_back(8U);
	vecU8t_IhdrData.push_back(6U);
	vecU8t_IhdrData.push_back(0U);
	vecU8t_IhdrData.push_back(0U);
	vecU8t_IhdrData.push_back(0U);
	appendChunk(vecU8t_PngData, kIhdrChunkType, vecU8t_IhdrData);

	appendChunk(vecU8t_PngData, kIdatChunkType, encodeCurrentImageData());
	appendChunk(vecU8t_PngData, kIendChunkType, {});
	return vecU8t_PngData;
}

// 将当前内存工作副本编码后写到新路径，但不改变源图绑定关系
void ST_png::writeToPath(const std::string& str_FilePathValue)
{
	const std::vector<uint8_t> vecU8t_EncodedPng = encodePngBytes();
	FileOperation fo_OutputFile(str_FilePathValue.c_str());
	throwIfFileError(fo_OutputFile.rewrite(vecU8t_EncodedPng.data(), vecU8t_EncodedPng.size()),
		"Failed to rewrite PNG output buffer");
	throwIfFileError(fo_OutputFile.flush(), "Failed to flush PNG output file");
}

// 下面是一组轻量查询与编辑接口的直接实现
FileHintCode ST_png::size(size_t& refSt_OutSize) const
{
	if (!p_File)
	{
		return FileHintCode::InvalidParameter;
	}
	return p_File->size(refSt_OutSize);
}

const std::string& ST_png::filePath() const noexcept
{
	return str_SourceFilePath;
}

const std::array<uint8_t, 8>& ST_png::signature() const noexcept
{
	return arrU8t_Signature;
}

const ST_png::HeaderChunkData& ST_png::header() const noexcept
{
	return st_HeaderChunkData;
}

const std::vector<ST_png::PaletteEntry>& ST_png::paletteEntries() const noexcept
{
	return vecSt_PaletteEntries;
}

const std::vector<uint8_t>& ST_png::imageData() const noexcept
{
	return vecU8t_ImageData;
}

const std::vector<ST_png::ChunkData>& ST_png::chunks() const noexcept
{
	return vecSt_Chunks;
}

const std::vector<ST_png::ChunkData>& ST_png::ancillaryChunks() const noexcept
{
	return vecSt_AncillaryChunks;
}

const std::vector<ST_png::RgbaPixel>& ST_png::pixels() const noexcept
{
	return vecSt_Pixels;
}

bool ST_png::hasPalette() const noexcept
{
	return b_HasPalette;
}

bool ST_png::hasImageData() const noexcept
{
	return b_HasImageData;
}

bool ST_png::hasEndChunk() const noexcept
{
	return b_HasEndChunk;
}

bool ST_png::isDirty() const noexcept
{
	return b_IsDirty;
}

void ST_png::setPixel(uint32_t u32t_X, uint32_t u32t_Y, const RgbaPixel& st_PixelColor)
{
	// 直接修改解码后的 RGBA 缓冲，保存时再统一重编码
	if (u32t_X >= st_HeaderChunkData.u32t_Width || u32t_Y >= st_HeaderChunkData.u32t_Height)
	{
		throw std::out_of_range("PNG pixel coordinate is out of range");
	}

	vecSt_Pixels[static_cast<size_t>(u32t_Y) * st_HeaderChunkData.u32t_Width + u32t_X] = st_PixelColor;
	b_IsDirty = true;
}

void ST_png::crop(uint32_t u32t_X, uint32_t u32t_Y, uint32_t u32t_Width, uint32_t u32t_Height)
{
	// 从当前 RGBA 缓冲中截取子区域，并同步更新 IHDR 尺寸
	if (u32t_Width == 0U || u32t_Height == 0U)
	{
		throw std::invalid_argument("Crop size must be greater than zero");
	}
	if (u32t_X >= st_HeaderChunkData.u32t_Width || u32t_Y >= st_HeaderChunkData.u32t_Height)
	{
		throw std::out_of_range("Crop origin is out of PNG bounds");
	}
	if (u32t_Width > st_HeaderChunkData.u32t_Width - u32t_X ||
		u32t_Height > st_HeaderChunkData.u32t_Height - u32t_Y)
	{
		throw std::out_of_range("Crop rectangle exceeds PNG bounds");
	}

	std::vector<RgbaPixel> vecSt_CroppedPixels;
	vecSt_CroppedPixels.reserve(static_cast<size_t>(u32t_Width) * u32t_Height);
	for (uint32_t u32t_Row = 0U; u32t_Row < u32t_Height; ++u32t_Row)
	{
		const size_t st_RowStart = static_cast<size_t>(u32t_Y + u32t_Row) * st_HeaderChunkData.u32t_Width + u32t_X;
		vecSt_CroppedPixels.insert(vecSt_CroppedPixels.end(),
			vecSt_Pixels.begin() + static_cast<std::ptrdiff_t>(st_RowStart),
			vecSt_Pixels.begin() + static_cast<std::ptrdiff_t>(st_RowStart + u32t_Width));
	}

	vecSt_Pixels = std::move(vecSt_CroppedPixels);
	st_HeaderChunkData.u32t_Width = u32t_Width;
	st_HeaderChunkData.u32t_Height = u32t_Height;
	b_IsDirty = true;
}

void ST_png::saveAs(const char* pSz_FilePath)
{
	// 保存到新路径，不覆盖源图，也不把当前对象切换到新文件
	const std::filesystem::path fsPath_FilePath = validateOutputFilePath(pSz_FilePath);
	std::error_code ec_ErrorCode;
	if (!str_SourceFilePath.empty() &&
		std::filesystem::equivalent(fsPath_FilePath, std::filesystem::path(str_SourceFilePath), ec_ErrorCode))
	{
		throw std::runtime_error("saveAs must not overwrite the original PNG file");
	}
	if (ec_ErrorCode)
	{
		ec_ErrorCode.clear();
	}
	writeToPath(fsPath_FilePath.string());
}

bool ST_png::isCriticalChunk(const std::array<char, 4>& arrC_Type) noexcept
{
	return std::isupper(static_cast<unsigned char>(arrC_Type[0])) != 0;
}
