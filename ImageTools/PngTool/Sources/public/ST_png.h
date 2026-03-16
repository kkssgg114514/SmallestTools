#ifndef SMALLESTTOOLS_ST_PNG_H
#define SMALLESTTOOLS_ST_PNG_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <FileOperation.h>

class ST_png
{
public:
	// IHDR 头块中的核心图像参数
	struct HeaderChunkData
	{
		uint32_t u32t_Width = 0;
		uint32_t u32t_Height = 0;
		uint8_t u8t_BitDepth = 0;
		uint8_t u8t_ColorType = 0;
		uint8_t u8t_CompressionMethod = 0;
		uint8_t u8t_FilterMethod = 0;
		uint8_t u8t_InterlaceMethod = 0;
	};

	// PLTE 调色板中的单个颜色项
	struct PaletteEntry
	{
		uint8_t u8t_Red = 0;
		uint8_t u8t_Green = 0;
		uint8_t u8t_Blue = 0;
	};

	// 对外暴露的统一 RGBA 像素格式，便于修改和裁剪
	struct RgbaPixel
	{
		uint8_t u8t_Red = 0;
		uint8_t u8t_Green = 0;
		uint8_t u8t_Blue = 0;
		uint8_t u8t_Alpha = 255;
	};

	// 保留原始块信息，便于调试、展示和后续扩展
	struct ChunkData
	{
		uint32_t u32t_Length = 0;
		std::array<char, 4> arrC_Type = {};
		std::vector<uint8_t> vecU8t_Data;
		uint32_t u32t_Crc32 = 0;
	};

	// 构造时立即完成 PNG 读取、校验、解压和像素解码
	explicit ST_png(const char* pSz_FilePath);
	~ST_png() = default;

	// 文件资源和像素缓冲只允许移动，避免复制代价和所有权混乱
	ST_png(const ST_png&) = delete;
	ST_png& operator=(const ST_png&) = delete;
	ST_png(ST_png&&) noexcept = default;
	ST_png& operator=(ST_png&&) noexcept = default;

	// 基础查询接口；filePath 返回最初读取的源图路径
	FileHintCode size(size_t& refSt_OutSize) const;
	const std::string& filePath() const noexcept;
	const std::array<uint8_t, 8>& signature() const noexcept;
	const HeaderChunkData& header() const noexcept;
	const std::vector<PaletteEntry>& paletteEntries() const noexcept;
	const std::vector<uint8_t>& imageData() const noexcept;
	const std::vector<ChunkData>& chunks() const noexcept;
	const std::vector<ChunkData>& ancillaryChunks() const noexcept;
	const std::vector<RgbaPixel>& pixels() const noexcept;
	bool hasPalette() const noexcept;
	bool hasImageData() const noexcept;
	bool hasEndChunk() const noexcept;
	bool isDirty() const noexcept;

	// 像素编辑只作用于内存中的工作副本，不会改动源图文件
	void setPixel(uint32_t u32t_X, uint32_t u32t_Y, const RgbaPixel& st_PixelColor);
	void crop(uint32_t u32t_X, uint32_t u32t_Y, uint32_t u32t_Width, uint32_t u32t_Height);
	// 仅允许显式另存为新 PNG，禁止覆盖最初读取的原图
	void saveAs(const char* pSz_FilePath);

private:
	// tRNS 透明度块的解析结果，根据颜色类型分别保存透明信息
	struct TransparencyData
	{
		bool b_HasPaletteAlpha = false;
		std::vector<uint8_t> vecU8t_PaletteAlpha;
		bool b_HasGrayTransparent = false;
		uint16_t u16t_GrayTransparent = 0;
		bool b_HasRgbTransparent = false;
		uint16_t u16t_RedTransparent = 0;
		uint16_t u16t_GreenTransparent = 0;
		uint16_t u16t_BlueTransparent = 0;
	};

	// 单块解析与校验辅助函数
	static HeaderChunkData parseAndValidateHeaderChunk(const ChunkData& st_ChunkData);
	static std::vector<PaletteEntry> parseAndValidatePaletteChunk(const ChunkData& st_ChunkData,
		const HeaderChunkData& st_HeaderChunkData);
	static TransparencyData parseAndValidateTransparencyChunk(const ChunkData& st_ChunkData,
		const HeaderChunkData& st_HeaderChunkData,
		size_t st_PaletteEntryCount);
	static bool isCriticalChunk(const std::array<char, 4>& arrC_Type) noexcept;

	// 读取、解析、像素解码与重写编码流程
	void loadFromPath(const std::string& str_FilePathValue);
	void parsePngBytes(const uint8_t* pU8t_FileData, size_t st_FileSize);
	void resetParsedState();
	void decodeImageToPixels();
	std::vector<uint8_t> encodeCurrentImageData() const;
	std::vector<uint8_t> encodePngBytes() const;
	void writeToPath(const std::string& str_FilePathValue);

	// 文件与解析状态缓存
	std::unique_ptr<FileOperation> p_File;
	std::string str_SourceFilePath;
	std::array<uint8_t, 8> arrU8t_Signature = {};
	HeaderChunkData st_HeaderChunkData;
	std::vector<PaletteEntry> vecSt_PaletteEntries;
	std::vector<uint8_t> vecU8t_ImageData;
	std::vector<ChunkData> vecSt_Chunks;
	std::vector<ChunkData> vecSt_AncillaryChunks;
	std::vector<RgbaPixel> vecSt_Pixels;
	TransparencyData st_TransparencyData;
	bool b_HasPalette = false;
	bool b_HasImageData = false;
	bool b_HasEndChunk = false;
	bool b_IsDirty = false;
};

#endif //SMALLESTTOOLS_ST_PNG_H
