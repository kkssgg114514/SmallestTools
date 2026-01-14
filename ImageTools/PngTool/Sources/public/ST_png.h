//
// Created by kkssgg on 2026/1/13.
//

#ifndef SMALLESTTOOLS_ST_PNG_H
#define SMALLESTTOOLS_ST_PNG_H
#include <FileOperation.h>

class ST_png
{
public:
	// 暂时空置
	ST_png(const char* pSz_FilePath);
	~ST_png();

	// 不允许拷贝,只允许移动,防止指针悬空
	ST_png(const ST_png&) = delete;
	ST_png& operator=(const ST_png&) = delete;
	ST_png(ST_png&&) noexcept;
	ST_png& operator=(ST_png&&) noexcept;

public:
	// 其他函数
	// 获取文件大小
	FileHintCode size(size_t& refSt_OutSize) const;

private:
	FileOperation *p_File;

	// 头数据块
	struct HeaderChunk;
	HeaderChunk* p_HeaderChunk;

	// 调色板数据块
	struct PaletteChunk;
	PaletteChunk* p_PaletteChunk;

	// 图像数据块
	struct ImageDataChunks;
	ImageDataChunks* p_ImageDataChunks;

	// 尾部数据块
	struct ImageTrailerChunk;
	ImageTrailerChunk* p_ImageTrailerChunk;

private:
	bool isPngFile(uint8_t* pU8t_FileSignature) const;
};


#endif //SMALLESTTOOLS_ST_PNG_H
