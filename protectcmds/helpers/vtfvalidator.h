// vtfvalidator.h
#ifndef VTFVALIDATOR_H
#define VTFVALIDATOR_H

#ifdef _WIN32
#pragma once
#endif

#include <bitmap/imageformat.h>
#include <vtf/vtf.h>

#pragma pack(1)

#define VTF_HEADER_MAGIC "\x56\x54\x46\x00"

typedef unsigned char   uchar;
typedef unsigned int    uint;
typedef unsigned short  ushort;

typedef ImageFormat VTFImageFormat;

typedef struct tagSVTFImageFormatInfo
{
	const char      *lpName;			//!< Enumeration text equivalent.
	uint	        uiBitsPerPixel;			//!< Format bits per pixel.
	uint	        uiBytesPerPixel;		//!< Format bytes per pixel.
	uint	        uiRedBitsPerPixel;		//!< Format red bits per pixel.  0 for N/A.
	uint	        uiGreenBitsPerPixel;	//!< Format green bits per pixel.  0 for N/A.
	uint	        uiBlueBitsPerPixel;		//!< Format blue bits per pixel.  0 for N/A.
	uint	        uiAlphaBitsPerPixel;	//!< Format alpha bits per pixel.  0 for N/A.
	uint        	bIsCompressed;			//!< Format is compressed (DXT).
	uint	        bIsSupported;			//!< Format is supported by VTFLib.
} SVTFImageFormatInfo;

static SVTFImageFormatInfo VTFImageFormatInfo[] =
{
	{ "RGBA8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_RGBA8888,
	{ "ABGR8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_ABGR8888, 
	{ "RGB888",				 24,  3,  8,  8,  8,  0, false,  true },		// IMAGE_FORMAT_RGB888,
	{ "BGR888",				 24,  3,  8,  8,  8,  0, false,  true },		// IMAGE_FORMAT_BGR888,
	{ "RGB565",				 16,  2,  5,  6,  5,  0, false,  true },		// IMAGE_FORMAT_RGB565, 
	{ "I8",					  8,  1,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_I8,
	{ "IA88",				 16,  2,  0,  0,  0,  8, false,  true },		// IMAGE_FORMAT_IA88
	{ "P8",					  8,  1,  0,  0,  0,  0, false, false },		// IMAGE_FORMAT_P8
	{ "A8",					  8,  1,  0,  0,  0,  8, false,  true },		// IMAGE_FORMAT_A8
	{ "RGB888 Bluescreen",	 24,  3,  8,  8,  8,  0, false,  true },		// IMAGE_FORMAT_RGB888_BLUESCREEN
	{ "BGR888 Bluescreen",	 24,  3,  8,  8,  8,  0, false,  true },		// IMAGE_FORMAT_BGR888_BLUESCREEN
	{ "ARGB8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_ARGB8888
	{ "BGRA8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_BGRA8888
	{ "DXT1",				  4,  0,  0,  0,  0,  0,  true,  true },		// IMAGE_FORMAT_DXT1
	{ "DXT3",				  8,  0,  0,  0,  0,  8,  true,  true },		// IMAGE_FORMAT_DXT3
	{ "DXT5",				  8,  0,  0,  0,  0,  8,  true,  true },		// IMAGE_FORMAT_DXT5
	{ "BGRX8888",			 32,  4,  8,  8,  8,  0, false,  true },		// IMAGE_FORMAT_BGRX8888
	{ "BGR565",				 16,  2,  5,  6,  5,  0, false,  true },		// IMAGE_FORMAT_BGR565
	{ "BGRX5551",			 16,  2,  5,  5,  5,  0, false,  true },		// IMAGE_FORMAT_BGRX5551
	{ "BGRA4444",			 16,  2,  4,  4,  4,  4, false,  true },		// IMAGE_FORMAT_BGRA4444
	{ "DXT1 One Bit Alpha",	  4,  0,  0,  0,  0,  1,  true,  true },		// IMAGE_FORMAT_DXT1_ONEBITALPHA
	{ "BGRA5551",			 16,  2,  5,  5,  5,  1, false,  true },		// IMAGE_FORMAT_BGRA5551
	{ "UV88",				 16,  2,  8,  8,  0,  0, false,  true },		// IMAGE_FORMAT_UV88
	{ "UVWQ8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_UVWQ8899
	{ "RGBA16161616F",	     64,  8, 16, 16, 16, 16, false,  true },		// IMAGE_FORMAT_RGBA16161616F
	{ "RGBA16161616",	     64,  8, 16, 16, 16, 16, false,  true },		// IMAGE_FORMAT_RGBA16161616
	{ "UVLX8888",			 32,  4,  8,  8,  8,  8, false,  true },		// IMAGE_FORMAT_UVLX8888
	{ "R32F",				 32,  4, 32,  0,  0,  0, false,  true },		// IMAGE_FORMAT_R32F
	{ "RGB323232F",			 96, 12, 32, 32, 32,  0, false,  true },		// IMAGE_FORMAT_RGB323232F
	{ "RGBA32323232F",		128, 16, 32, 32, 32, 32, false,  true },		// IMAGE_FORMAT_RGBA32323232F
	{ "nVidia DST16",		 16,  2,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_NV_DST16
	{ "nVidia DST24",		 24,  3,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_NV_DST24
	{ "nVidia INTZ",		 32,  4,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_NV_INTZ
	{ "nVidia RAWZ",		 32,  4,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_NV_RAWZ
	{ "ATI DST16",			 16,  2,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_ATI_DST16
	{ "ATI DST24",			 24,  3,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_ATI_DST24
	{ "nVidia NULL",		 32,  4,  0,  0,  0,  0, false,  true },		// IMAGE_FORMAT_NV_NULL
	{ "ATI1N",				  4,  0,  0,  0,  0,  0,  true,  true },		// IMAGE_FORMAT_ATI1N
	{ "ATI2N",				  8,  0,  0,  0,  0,  0,  true,  true } 		// IMAGE_FORMAT_ATI2N
};

struct VTF_s
{
	char                TypeString[4];					//!< "Magic number" identifier- "VTF\0".
	uint                Version[2];						//!< Version[0].version[1] (currently 7.2)
	uint                HeaderSize;						//!< Size of the header struct (currently 80 bytes)		
    ushort              Width;							//!< Width of the largest image
	ushort              Height;							//!< Height of the largest image
	uint                Flags;							//!< Flags for the image
	ushort              Frames;							//!< Number of frames if animated (1 for no animation)
    ushort              StartFrame;						//!< Start frame (always 0)
	uchar               Padding0[4];					//!< Reflectivity padding (16 byte alignment)
	float               Reflectivity[3];				//!< Reflectivity vector
	uchar               Padding1[4];					//!< Reflectivity padding (8 byte packing)
	float               BumpScale;						//!< Bump map scale
	VTFImageFormat      ImageFormat;					//!< Image format index
	uchar               MipCount;						//!< Number of MIP levels (including the largest image)
	VTFImageFormat      LowResImageFormat;				//!< Image format of the thumbnail image
	uchar               LowResImageWidth;				//!< Thumbnail image width
	uchar               LowResImageHeight;				//!< Thumbnail image height
    ushort              Depth;							//!< Depth of the largest image
    uchar               Padding2[3];
	uint                ResourceCount;	
};

class CVTFValidator
{    
    public:
        bool IsValid(void* pData, uint size);
        bool IsShouldLog();
        const char *GetLastError();

    private:
        SVTFImageFormatInfo const &GetImageFormatInfo(VTFImageFormat ImageFormat);
        uint ComputeImageSize(uint uiWidth, uint uiHeight, uint uiDepth, VTFImageFormat ImageFormat);
        uint ComputeImageSize(uint uiWidth, uint uiHeight, uint uiDepth, uint uiMipmaps, VTFImageFormat ImageFormat);

        uint GetFaceCount() const;
        
        VTF_s *m_pVTFHeader = nullptr;
        uint m_nImageBufferSize;				// Size of VTF image data buffer
};

#pragma pack()

#endif // VTFVALIDATOR_H