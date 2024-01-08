#include <cstring>

#include "vtfvalidator.h"
#include "convar.h"
#include <tier0/dbg.h>
#include <tier1/strtools.h>

//Spray Fix ConVars
static ConVar sv_spray_width_min( "sv_spray_width_min", "4", FCVAR_NONE, "Minimum spray width." );
static ConVar sv_spray_width_max( "sv_spray_width_max", "4096", FCVAR_NONE, "Maximum spray width." );
static ConVar sv_spray_height_min( "sv_spray_height_min", "4", FCVAR_NONE, "Minimum spray height." );
static ConVar sv_spray_height_max( "sv_spray_height_max", "4096", FCVAR_NONE, "Maximum spray height." );
static ConVar sv_spray_num_frames_max( "sv_spray_num_frames_max", "100", FCVAR_NONE, "Maximum spray frames." );
static ConVar sv_spray_filesize_max( "sv_spray_filesize_max", "131072", FCVAR_NONE, "Maximum spray size in bytes." );
static ConVar sv_spray_log( "sv_spray_log", "1", FCVAR_NONE, "Enable show messages on checking spray. 1 - Enable. 0 - Disable." );

static char BufError[512];

bool CVTFValidator::IsShouldLog()
{
    return sv_spray_log.GetBool();
}

const char *CVTFValidator::GetLastError()
{
    return BufError;
}

bool CVTFValidator::IsValid(void* pData, uint size) {
    //pData not is nullptr by CnetChan::CheckReceivingList
    /*if ( !pData )
    {
        Q_strncpy(BufError, "VTF pointer data is nullptr", sizeof(BufError));

        return false;
    }*/

    BufError[0] = '\0';

    if ( size < sizeof(VTF_s) ) {
        Q_strncpy(BufError, "small size for VTF header", sizeof(BufError));

        return false;
    }

    if ( static_cast<int>(size) > sv_spray_filesize_max.GetInt() )
    {
        Q_snprintf(BufError, sizeof(BufError), "invalid file size. Current: %d, max: %d", size, sv_spray_filesize_max.GetInt());

        return false;
    }

    m_pVTFHeader = (struct VTF_s *)pData;

    if ( memcmp(&m_pVTFHeader->TypeString, VTF_HEADER_MAGIC, sizeof(m_pVTFHeader->TypeString)) != 0 )
    {
        Q_strncpy(BufError, "invalid VTF magic header", sizeof(BufError));

       return false;
    }

    if ( m_pVTFHeader->Version[0] != 7 || m_pVTFHeader->Version[1] > 5 )
    {
        Q_snprintf(BufError, sizeof(BufError), "invalid version. Current: %u.%u, support: 7.[0-5]", m_pVTFHeader->Version[0], m_pVTFHeader->Version[1]);

        return false;
    }

    if ( m_pVTFHeader->Version[1] < 2 )
    {
        // set depth if version is lower than 7.2
        m_pVTFHeader->Depth = 1;
    }

    if ( m_pVTFHeader->Width < sv_spray_width_min.GetInt() || m_pVTFHeader->Width  > sv_spray_width_max.GetInt() )
    {
        Q_snprintf(BufError, sizeof(BufError), "invalid width. Current: %u, min: %d, max: %d", m_pVTFHeader->Width, sv_spray_width_min.GetInt(), sv_spray_width_max.GetInt());

        return false;
    }

    if ( m_pVTFHeader->Height < sv_spray_height_min.GetInt() || m_pVTFHeader->Height > sv_spray_height_max.GetInt() )
    {
        Q_snprintf(BufError, sizeof(BufError), "invalid height. Current: %u, min: %d, max: %d", m_pVTFHeader->Height, sv_spray_height_min.GetInt(), sv_spray_height_max.GetInt());

        return false;
    }

    if ( m_pVTFHeader->Flags & ( TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_DEPTHRENDERTARGET | TEXTUREFLAGS_NODEPTHBUFFER ) )
    {
        Q_strncpy(BufError, "invalid flags", sizeof(BufError));

        return false;
    }

    if ( (m_pVTFHeader->Flags & TEXTUREFLAGS_ENVMAP) && (m_pVTFHeader->Width != m_pVTFHeader->Height) )
	{
		Q_strncpy(BufError, "non-square cubemap", sizeof(BufError));

		return false;
	}

    if ( (m_pVTFHeader->Flags & TEXTUREFLAGS_ENVMAP) && (m_pVTFHeader->Depth != 1) )
	{
		Q_strncpy(BufError, "invalid volume texture cubemap", sizeof(BufError));

		return false;
	}

    if ( m_pVTFHeader->Width <= 0 || m_pVTFHeader->Height <= 0 || m_pVTFHeader->Depth <= 0 )
	{
		Q_strncpy(BufError, "invalid texture size", sizeof(BufError));

		return false;
	}

    if ( m_pVTFHeader->Frames > sv_spray_num_frames_max.GetInt() )
    {
        Q_snprintf(BufError, sizeof(BufError), "invalid frames. Current: %u, max: %d", m_pVTFHeader->Frames, sv_spray_num_frames_max.GetInt());

        return false;
    }

    if ( m_pVTFHeader->ImageFormat < 0 || m_pVTFHeader->ImageFormat >= NUM_IMAGE_FORMATS )
    {
        Q_strncpy(BufError, "invalid image format", sizeof(BufError));

		return false;
    }

    m_nImageBufferSize = ComputeImageSize(m_pVTFHeader->Width, m_pVTFHeader->Height, m_pVTFHeader->Depth, m_pVTFHeader->MipCount, m_pVTFHeader->ImageFormat) * GetFaceCount() * m_pVTFHeader->Frames;

    if( m_pVTFHeader->HeaderSize > size || m_nImageBufferSize > size )
	{
        Q_strncpy(BufError, "small image data", sizeof(BufError));

        return false;
	}

    return true;
}

SVTFImageFormatInfo const &CVTFValidator::GetImageFormatInfo(VTFImageFormat ImageFormat)
{
	return VTFImageFormatInfo[ImageFormat];
}

//------------------------------------------------------------------------------------
// ComputeImageSize(uint uiWidth, uint uiHeight, VTFImageFormat ImageFormat)
// 
// Returns how many bytes are needed to store an image of width * height in the chosen
// image format. If bMipMaps is true, the total will reflect the space needed to store
// the original image plus all the mipmaps down to a size of 1 x 1
//------------------------------------------------------------------------------------
uint CVTFValidator::ComputeImageSize(uint uiWidth, uint uiHeight, uint uiDepth, VTFImageFormat ImageFormat)
{    
	switch(ImageFormat)
	{
        case IMAGE_FORMAT_DXT1:
        case IMAGE_FORMAT_DXT1_ONEBITALPHA:
            if(uiWidth < 4 && uiWidth > 0)
                uiWidth = 4;

            if(uiHeight < 4 && uiHeight > 0)
                uiHeight = 4;

            return ((uiWidth + 3) / 4) * ((uiHeight + 3) / 4) * 8 * uiDepth;
        case IMAGE_FORMAT_DXT3:
        case IMAGE_FORMAT_DXT5:
            if(uiWidth < 4 && uiWidth > 0)
                uiWidth = 4;

            if(uiHeight < 4 && uiHeight > 0)
                uiHeight = 4;

            return ((uiWidth + 3) / 4) * ((uiHeight + 3) / 4) * 16 * uiDepth;
        default:
            return uiWidth * uiHeight * uiDepth * GetImageFormatInfo(ImageFormat).uiBytesPerPixel;
	}
}

uint CVTFValidator::ComputeImageSize(uint uiWidth, uint uiHeight, uint uiDepth, uint uiMipmaps, VTFImageFormat ImageFormat)
{
	uint uiImageSize = 0;

	for (uint i = 0; i < uiMipmaps; i++)
	{
		uiImageSize += ComputeImageSize(uiWidth, uiHeight, uiDepth, ImageFormat);
		
		uiWidth >>= 1;
		uiHeight >>= 1;
		uiDepth >>= 1;

		if(uiWidth < 1)
			uiWidth = 1;

		if(uiHeight < 1)
			uiHeight = 1;

		if(uiDepth < 1)
			uiDepth = 1;
	}

	return uiImageSize;
}

uint CVTFValidator::GetFaceCount() const
{
	return m_pVTFHeader->Flags & TEXTUREFLAGS_ENVMAP ? (m_pVTFHeader->StartFrame != 0xffff && m_pVTFHeader->Version[1] < 5 ? 7 : 7 - 1) : 1;
}