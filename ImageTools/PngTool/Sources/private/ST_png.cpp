//
// Created by kkssgg on 2026/1/13.
//

#include <stdexcept>
#include <vector>
#include "../public/ST_png.h"

template<typename T>
struct Chunk
{
	Chunk(){}
	uint32_t u32t_Length = 0;
	std::vector<char> vecC_ChunkTypeCode;
	T t_ChuckData;
	// 校验和
	uint32_t u32t_Crc32 = 0;
};

struct HeaderChunkData
{
	HeaderChunkData() = default;
	HeaderChunkData(std::vector<uint8_t>& vecU8tRef_ChunkData)
	{
		int i = 0;
		for (; i < 4; i++)
		{
			uint32_t tmp = 0;
			tmp <<= 8;
			tmp |= vecU8tRef_ChunkData[i];
			u32t_Width |= tmp;
		}
		for (; i < 8; i++)
		{
			uint32_t tmp = 0;
			tmp <<= 8;
			tmp |= vecU8tRef_ChunkData[i];
			u32t_Height |= tmp;
		}
		u8t_ColorType = vecU8tRef_ChunkData[i++];
		u8t_CompressionMethod = vecU8tRef_ChunkData[i++];
		u8t_FilterMethod = vecU8tRef_ChunkData[i++];
		u8t_InterlaceMethod = vecU8tRef_ChunkData[i++];
	}
	uint32_t u32t_Width = 0;
	uint32_t u32t_Height = 0;
	uint8_t u8t_ColorType = 0;
	uint8_t u8t_CompressionMethod = 0;
	uint8_t u8t_FilterMethod = 0;
	uint8_t u8t_InterlaceMethod = 0;
};

struct ST_png::HeaderChunk : public Chunk<HeaderChunkData>
{
	HeaderChunk(std::vector<uint8_t>& vecU8tRef_ChunkData)
	{
		std::vector<uint8_t>::iterator it = vecU8tRef_ChunkData.begin();
		//解析长度
		std::vector<uint8_t>::iterator itEnd = it + 4;
		for (; it != itEnd; it++)
		{
			uint32_t tmp = 0;
			tmp <<= 8;
			tmp |= *it;
			u32t_Length |= tmp;
		}
		if (u32t_Length != 13)
		{
			throw std::runtime_error("Chunk length error");
		}
		//解析类型码
		const char pSz_ChunkTypeCode[4] = {'I', 'H', 'D', 'R'};
		int i = 0;
		itEnd += 4;
		for (; it != itEnd; it++, i++)
		{
			if (*it ^ pSz_ChunkTypeCode[i])
			{
				throw std::runtime_error("Chunk type code error");
			}
			vecC_ChunkTypeCode.push_back(*it);
		}
		//解析数据
		std::vector<uint8_t> temp(it, it + u32t_Length);
		t_ChuckData = HeaderChunkData(temp);
	}
};

ST_png::ST_png(const char *pSz_FilePath)
{
	// 构造函数
	p_File = new FileOperation(pSz_FilePath);
	// 检查前8个字节是否为PNG签名
	uint8_t u8t_FileSignature[8];
	p_File->read(0, u8t_FileSignature, 8);
	if (!isPngFile(u8t_FileSignature))
	{

		throw std::runtime_error("Not a PNG file");
	}
}

ST_png::~ST_png()
{
	// 析构函数
	delete p_File;
}

ST_png::ST_png(ST_png &&stP_other) noexcept: p_File(stP_other.p_File)
{
	// 移动构造函数
	stP_other.p_File = nullptr;
}

ST_png &ST_png::operator=(ST_png &&stP_other) noexcept
{
	// 移动赋值运算符
	if (this != &stP_other)
	{
		delete p_File;
		p_File = stP_other.p_File;
		stP_other.p_File = nullptr;
	}
	return *this;
}

FileHintCode ST_png::size(size_t &refSt_OutSize) const
{
	// 获取文件大小,直接从管理文件的类获取
	if (!p_File)
	{
		return FileHintCode::InvalidParameter;
	}
	return p_File->size(refSt_OutSize);
}

bool ST_png::isPngFile(uint8_t *pU8t_FileSignature) const
{
	const uint8_t u8t_PngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	for (size_t i = 0; i < 8; i++)
	{
		if (pU8t_FileSignature[i] ^ u8t_PngSignature[i])
		{
			return false;
		}
	}
	return true;
}
