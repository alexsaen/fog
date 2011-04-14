// [Fog-G2d]
//
// [License]
// MIT, See COPYING file in package

// [Precompiled Headers]
#if defined(FOG_PRECOMP)
#include FOG_PRECOMP
#endif // FOG_PRECOMP

// [Guard]
#include <Fog/Core/Config/Config.h>
#if defined(FOG_OS_WINDOWS)

// [Dependencies]
#include <Fog/Core/Global/Constants.h>
#include <Fog/Core/Global/Debug.h>
#include <Fog/Core/Global/Static.h>
#include <Fog/Core/IO/Stream.h>
#include <Fog/Core/Library/Library.h>
#include <Fog/Core/Math/Math.h>
#include <Fog/Core/Tools/ManagedString.h>
#include <Fog/Core/Tools/String.h>
#include <Fog/Core/Tools/Strings.h>
#include <Fog/Core/Win/WinCom.h>
#include <Fog/Core/Win/WinComStream_p.h>
#include <Fog/G2d/Global/Constants.h>
#include <Fog/G2d/Imaging/Codecs/GdipCodec_p.h>
#include <Fog/G2d/Imaging/Image.h>
#include <Fog/G2d/Imaging/ImageConverter.h>
#include <Fog/G2d/Imaging/ImageFormatDescription.h>

FOG_IMPLEMENT_OBJECT(Fog::GdiPlusDecoder)
FOG_IMPLEMENT_OBJECT(Fog::GdiPlusEncoder)

namespace Fog {

// ===========================================================================
// [Fog::GdiPlusImage - Format - Helpers]
// ===========================================================================

static uint32_t GdiPlus_fogFormatFromGpFormat(GpPixelFormat fmt)
{
  switch (fmt)
  {
    case GpPixelFormat1bppIndexed   : return IMAGE_FORMAT_I8       ;
    case GpPixelFormat4bppIndexed   : return IMAGE_FORMAT_I8       ;
    case GpPixelFormat8bppIndexed   : return IMAGE_FORMAT_I8       ;
    case GpPixelFormat16bppGrayScale: return IMAGE_FORMAT_RGB48    ;
    case GpPixelFormat16bppRGB555   : return IMAGE_FORMAT_RGB24    ;
    case GpPixelFormat16bppRGB565   : return IMAGE_FORMAT_RGB24    ;
    case GpPixelFormat16bppARGB1555 : return IMAGE_FORMAT_PRGB32   ;
    case GpPixelFormat24bppRGB      : return IMAGE_FORMAT_RGB24    ;
    case GpPixelFormat32bppRGB      : return IMAGE_FORMAT_XRGB32   ;
    case GpPixelFormat32bppARGB     : return IMAGE_FORMAT_PRGB32   ;
    case GpPixelFormat32bppPARGB    : return IMAGE_FORMAT_PRGB32   ;
    case GpPixelFormat48bppRGB      : return IMAGE_FORMAT_RGB48    ;
    case GpPixelFormat64bppARGB     : return IMAGE_FORMAT_PRGB64   ;
    case GpPixelFormat64bppPARGB    : return IMAGE_FORMAT_PRGB64   ;
    default                         : return IMAGE_FORMAT_NULL     ;
  }
}

static GpPixelFormat GdiPlus_gpFormatFromFogFormat(uint32_t fmt)
{
  switch (fmt)
  {
    case IMAGE_FORMAT_PRGB32   : return GpPixelFormat32bppPARGB ;
    case IMAGE_FORMAT_XRGB32   : return GpPixelFormat32bppRGB   ;
    case IMAGE_FORMAT_RGB24    : return GpPixelFormat24bppRGB   ;
    case IMAGE_FORMAT_A8       : return GpPixelFormat32bppPARGB ;
    case IMAGE_FORMAT_I8       : return GpPixelFormat8bppIndexed;
    case IMAGE_FORMAT_PRGB64   : return GpPixelFormat64bppPARGB ;
    case IMAGE_FORMAT_RGB48    : return GpPixelFormat48bppRGB   ;
    case IMAGE_FORMAT_A16      : return GpPixelFormat64bppPARGB ;
    default                    : return GpPixelFormatUndefined  ;
  }
}

// ===========================================================================
// [Fog::GdiPlusImage - Params - GUID]
// ===========================================================================

FOG_COM_DEFINE_GUID(GpEncoderQuality, 0x1d5be4b5, 0xfa4a, 0x452d, 0x9c, 0xdd, 0x5d, 0xb3, 0x51, 0x05, 0xe7, 0xeb);

// ===========================================================================
// [Fog::GdiPlusImage - Params - Helpers]
// ===========================================================================

static void GdiPlus_clearCommonParameters(GdiPlusCommonParams* params, uint32_t streamType)
{
  memset(params, 0, sizeof(GdiPlusCommonParams));

  switch (streamType)
  {
    case IMAGE_STREAM_JPEG:
      params->jpeg.quality = 90;
      break;
    case IMAGE_STREAM_PNG:
      break;
    case IMAGE_STREAM_TIFF:
      break;
  }
}

static err_t GdiPlus_getCommonParameter(const GdiPlusCommonParams* params, uint32_t streamType, const ManagedString& name, Value& value)
{
  // This means to continue property processing calling superclass.
  err_t err = (err_t)0xFFFFFFFF;

  switch (streamType)
  {
    case IMAGE_STREAM_JPEG:
      if (name == fog_strings->getString(STR_G2D_CODEC_quality))
      {
        return value.setInt32(params->jpeg.quality);
      }
      break;
    case IMAGE_STREAM_PNG:
      break;
    case IMAGE_STREAM_TIFF:
      break;
  }

  return err;
}

static err_t GdiPlus_setCommonParameter(GdiPlusCommonParams* params, uint32_t streamType, const ManagedString& name, const Value& value)
{
  // This means to continue property processing calling superclass.
  err_t err = (err_t)0xFFFFFFFF;

  switch (streamType)
  {
    case IMAGE_STREAM_JPEG:
      if (name == fog_strings->getString(STR_G2D_CODEC_quality))
      {
        int i;
        if ((err = value.getInt32(&i)) == ERR_OK)
          params->jpeg.quality = Math::bound(i, 0, 100);
        return ERR_OK;
      }
      break;
    case IMAGE_STREAM_PNG:
      break;
    case IMAGE_STREAM_TIFF:
      break;
  }

  return err;
}

// ===========================================================================
// [Fog::WinGdiPlusLibrary]
// ===========================================================================

WinGdiPlusLibrary::WinGdiPlusLibrary() :
  err(0xFFFFFFFF),
  gdiplusToken(0)
{
}

WinGdiPlusLibrary::~WinGdiPlusLibrary()
{
  close();
}

err_t WinGdiPlusLibrary::prepare()
{
  if (err == 0xFFFFFFFF)
  {
    FOG_ONCE_LOCK();
    if (err == 0xFFFFFFFF) err = init();
    FOG_ONCE_UNLOCK();
  }

  return err;
}

err_t WinGdiPlusLibrary::init()
{
  static const char symbols[] =
    "GdiplusStartup\0"
    "GdiplusShutdown\0"
    "GdipLoadImageFromStream\0"
    "GdipSaveImageToStream\0"
    "GdipDisposeImage\0"
    "GdipGetImageType\0"
    "GdipGetImageWidth\0"
    "GdipGetImageHeight\0"
    "GdipGetImageFlags\0"
    "GdipGetImagePixelFormat\0"
    "GdipGetImageGraphicsContext\0"
    "GdipImageGetFrameCount\0"
    "GdipImageSelectActiveFrame\0"
    "GdipCreateBitmapFromScan0\0"
    "GdipSetCompositingMode\0"
    "GdipDrawImageI\0"
    "GdipFlush\0"
    "GdipDeleteGraphics\0"

    "GdipGetImageEncoders\0"
    "GdipGetImageEncodersSize\0"
    ;

  // Ensure that we are not called twice (once initialization is done
  // we can't be called again).
  FOG_ASSERT(err == 0xFFFFFFFF);

  if (dll.open(Ascii8("gdiplus")) != ERR_OK)
  {
    // gdiplus.dll not found.
    return ERR_IMAGE_GDIPLUS_NOT_LOADED;
  }

  const char* badSymbol;
  if (dll.getSymbols(addr, symbols, FOG_ARRAY_SIZE(symbols), NUM_SYMBOLS, (char**)&badSymbol) != NUM_SYMBOLS)
  {
    // Some symbol failed to load? Inform about it.
    Debug::dbgFunc("Fog::WinGdiPlusLibrary", "init", "Can't load symbol '%s'.\n", badSymbol);
    dll.close();
    return ERR_IMAGE_GDIPLUS_NOT_LOADED;
  }

  // GdiPlus - Startup.
  GpGdiplusStartupInput startupInput;
  startupInput.GdiplusVersion = 1;
  startupInput.DebugEventCallback = NULL;
  startupInput.SuppressBackgroundThread = false;
  startupInput.SuppressExternalCodecs = false;

  GpStatus status = pGdiplusStartup(&gdiplusToken, &startupInput, NULL);
  if (status != GpOk)
  {
    Debug::dbgFunc("Fog::WinGdiPlusLibrary", "init", "GdiplusStartup() failed (%u).\n", status);
    dll.close();
    return ERR_IMAGE_GDIPLUS_NOT_LOADED;
  }

  return ERR_OK;
}

void WinGdiPlusLibrary::close()
{
  // GdiPlus - Shutdown.
  if (err == ERR_OK)
  {
    pGdiplusShutdown(gdiplusToken);
    gdiplusToken = 0;
  }

  dll.close();
  err = 0xFFFFFFFF;
}

static Static<WinGdiPlusLibrary> _gdiPlusLibrary;
static Atomic<sysint_t> _gdiPlusRefCount;

// ===========================================================================
// [Fog::GdiPlusCodecProvider]
// ===========================================================================

static err_t getGdiPlusEncoderClsid(const WCHAR* mime, CLSID* clsid)
{
  GpStatus status;
  GpImageCodecInfo* codecs = NULL;

  UINT i;
  UINT codecsCount;
  UINT codecsDataSize;

  err_t err = ERR_OK;

  status = _gdiPlusLibrary->pGdipGetImageEncodersSize(&codecsCount, &codecsDataSize);
  if (status != GpOk)
  {
    err = ERR_IMAGE_GDIPLUS_ERROR;
    goto _End;
  }

  codecs = reinterpret_cast<GpImageCodecInfo*>(Memory::alloc(codecsDataSize));
  if (codecs == NULL)
  {
    err = ERR_RT_OUT_OF_MEMORY;
    goto _End;
  }

  status = _gdiPlusLibrary->pGdipGetImageEncoders(codecsCount, codecsDataSize, codecs);
  if (status != GpOk)
  {
    err = ERR_IMAGE_GDIPLUS_ERROR;
    goto _End;
  }

  for (i = 0; i < codecsCount; i++)
  {
    if (wcscmp(codecs[i].MimeType, mime) == 0)
    {
      *clsid = codecs[i].Clsid;
      goto _End;
    }
  }

  // Shouldn't happen.
  err = ERR_IMAGEIO_INTERNAL_ERROR;

_End:
  if (codecs) Memory::free(codecs);
  return err;
}

GdiPlusCodecProvider::GdiPlusCodecProvider(uint32_t streamType)
{
  // Initialize WinGdiPlusLibrary.
  if (_gdiPlusRefCount.addXchg(1) == 0) _gdiPlusLibrary.init();

  const WCHAR* gdipMime = NULL;

  // Supported codecs.
  _codecType = IMAGE_CODEC_BOTH;

  // Supported streams.
  _streamType = streamType;

  // Name of ImageCodecProvider.
  switch (_streamType)
  {
    case IMAGE_STREAM_JPEG:
      _name = fog_strings->getString(STR_G2D_STREAM_JPEG);
      _gdipMime = L"image/jpeg";
      break;
    case IMAGE_STREAM_PNG:
      _name = fog_strings->getString(STR_G2D_STREAM_PNG);
      _gdipMime = L"image/png";
      break;
    case IMAGE_STREAM_TIFF:
      _name = fog_strings->getString(STR_G2D_STREAM_TIFF);
      _gdipMime = L"image/tiff";
      break;
  }

  // All GDI+ providers starts with "[GDI+]" suffix.
  _name.append(Ascii8("[GDI+]"));

  // Supported extensions.
  switch (_streamType)
  {
    case IMAGE_STREAM_JPEG:
      _imageExtensions.reserve(4);
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_jpg));
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_jpeg));
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_jfi));
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_jfif));
      break;
    case IMAGE_STREAM_PNG:
      _imageExtensions.reserve(1);
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_png));
      break;
    case IMAGE_STREAM_TIFF:
      _imageExtensions.reserve(2);
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_tif));
      _imageExtensions.append(fog_strings->getString(STR_G2D_EXTENSION_tiff));
      break;
    default:
      FOG_ASSERT_NOT_REACHED();
  }
}

GdiPlusCodecProvider::~GdiPlusCodecProvider()
{
  // Shutdown WinGdiPlusLibrary.
  if (_gdiPlusRefCount.deref()) _gdiPlusLibrary.destroy();
}

uint32_t GdiPlusCodecProvider::checkSignature(const void* mem, sysuint_t length) const
{
  // Note: GdiPlus proxy provider uses 14 as a base score. This
  // is by one less than all other providers based on external 
  // libraries (libpng, libjpeg, libtiff) and reason is that when
  // these external libraries are available they are used instead.
  if (!mem || length == 0) return 0;

  uint32_t score = 0;
  sysuint_t i;

  // Mime data.
  static const uint8_t mimeJPEG[2]    = { 0xFF, 0xD8 };
  static const uint8_t mimePNG[8]     = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
  static const uint8_t mimeTIFF_LE[4] = { 0x49, 0x49, 0x00, 0x42 };
  static const uint8_t mimeTIFF_BE[4] = { 0x4D, 0x4D, 0x42, 0x00 };

  // Mime check.
  switch (_streamType)
  {
    case IMAGE_STREAM_JPEG:
      i = Math::min<sysuint_t>(length, 2);
      if (memcmp(mem, mimeJPEG, i) == 0)
        score = Math::max<uint32_t>(score, 14 + ((uint32_t)i * 40));
      break;
    case IMAGE_STREAM_PNG:
      i = Math::min<sysuint_t>(length, 8);
      if (memcmp(mem, mimePNG, i) == 0)
        score = Math::max<uint32_t>(score, 14 + ((uint32_t)i * 10));
      break;
    case IMAGE_STREAM_TIFF:
      i = Math::min<sysuint_t>(length, 4);
      if (memcmp(mem, mimeTIFF_LE, i) == 0 || memcmp(mem, mimeTIFF_BE, i) == 0)
        score = Math::max<uint32_t>(score, 14 + ((uint32_t)i * 20));
      break;
    default:
      FOG_ASSERT_NOT_REACHED();
  }

  return score;
}

err_t GdiPlusCodecProvider::createCodec(uint32_t codecType, ImageCodec** codec) const
{
  FOG_ASSERT(codec != NULL);
  FOG_RETURN_ON_ERROR(_gdiPlusLibrary->prepare());

  ImageCodec* c = NULL;
  switch (codecType)
  {
    case IMAGE_CODEC_DECODER:
      c = fog_new GdiPlusDecoder(const_cast<GdiPlusCodecProvider*>(this));
      break;
    case IMAGE_CODEC_ENCODER:
      c = fog_new GdiPlusEncoder(const_cast<GdiPlusCodecProvider*>(this));
      break;
    default:
      return ERR_RT_INVALID_ARGUMENT;
  }

  if (FOG_IS_NULL(c)) return ERR_RT_OUT_OF_MEMORY;
  *codec = c;

  return ERR_OK;
}

// ===========================================================================
// [Fog::GdiPlusDecoder - Construction / Destruction]
// ===========================================================================

GdiPlusDecoder::GdiPlusDecoder(ImageCodecProvider* provider) :
  ImageDecoder(provider),
  _istream(NULL),
  _gpImage(NULL)
{
  GdiPlus_clearCommonParameters(&_params, _streamType);
}

GdiPlusDecoder::~GdiPlusDecoder()
{
}

// ===========================================================================
// [Fog::GdiPlusDecoder - AttachStream / DetachStream]
// ===========================================================================

void GdiPlusDecoder::attachStream(Stream& stream)
{
  _istream = fog_new WinComStream(stream);

  base::attachStream(stream);
}

void GdiPlusDecoder::detachStream()
{
  if (_gpImage) 
  {
    _gdiPlusLibrary->pGdipDisposeImage(_gpImage);
    _gpImage = NULL;
  }

  if (_istream)
  {
    _istream->Release();
    _istream = NULL;
  }

  base::detachStream();
}

// ===========================================================================
// [Fog::GdiPlusDecoder - Reset]
// ===========================================================================

void GdiPlusDecoder::reset()
{
  GdiPlus_clearCommonParameters(&_params, _streamType);
  ImageDecoder::reset();
}

// ===========================================================================
// [Fog::GdiPlusDecoder - ReadHeader]
// ===========================================================================

err_t GdiPlusDecoder::readHeader()
{
  // Do not read header more than once.
  if (_headerResult) return _headerResult;

  if (_istream == NULL) return ERR_RT_INVALID_HANDLE;

  GpStatus status = _gdiPlusLibrary->pGdipLoadImageFromStream(_istream, &_gpImage);
  if (status != GpOk) return (_headerResult = ERR_IMAGE_GDIPLUS_ERROR);

  FOG_ASSERT(sizeof(UINT) == sizeof(int));
  _gdiPlusLibrary->pGdipGetImageWidth(_gpImage, (UINT*)&_size.w);
  _gdiPlusLibrary->pGdipGetImageHeight(_gpImage, (UINT*)&_size.h);
  _planes = 1;

  GpPixelFormat pf;
  _gdiPlusLibrary->pGdipGetImagePixelFormat(_gpImage, &pf);

  _format = GdiPlus_fogFormatFromGpFormat(pf);
  _depth = ImageFormatDescription::getByFormat(_format).getDepth();

  return ERR_OK;
}

// ===========================================================================
// [Fog::GdiPlusDecoder - ReadImage]
// ===========================================================================

err_t GdiPlusDecoder::readImage(Image& image)
{
  err_t err = ERR_OK;

  if (_istream == NULL) return ERR_RT_INVALID_HANDLE;

  GpBitmap* bm = NULL;
  GpGraphics* gr = NULL;
  GpStatus status;

  // Read image header.
  if (readHeader() != ERR_OK) return _headerResult;

  // Don't read image more than once.
  if (isReaderDone()) return (_readerResult = ERR_IMAGE_NO_MORE_FRAMES);

  // Create image.
  if ((err = image.create(_size, _format)) != ERR_OK) return err;

  // Create GpBitmap that will share raster data with our image.
  status = _gdiPlusLibrary->pGdipCreateBitmapFromScan0(
    (INT)image.getWidth(),
    (INT)image.getHeight(), 
    (INT)image.getStride(),
    GdiPlus_gpFormatFromFogFormat(image.getFormat()),
    (BYTE*)image.getDataX(),
    &bm);
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

  // Create GpGraphics context.
  status = _gdiPlusLibrary->pGdipGetImageGraphicsContext((GpImage*)bm, &gr);
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

  // Set compositing to source copy (we want alpha bits).
  status = _gdiPlusLibrary->pGdipSetCompositingMode(gr, GpCompositingModeSourceCopy);
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

  // Draw streamed image to GpGraphics context.
  status = _gdiPlusLibrary->pGdipDrawImageI(gr, _gpImage, 0, 0);
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

  // flush (this step is probably not necessary).
  status = _gdiPlusLibrary->pGdipFlush(gr, GpFlushIntentionSync);
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

_End:
  // Delete created Gdi+ objects.
  if (gr) _gdiPlusLibrary->pGdipDeleteGraphics(gr);
  if (bm) _gdiPlusLibrary->pGdipDisposeImage((GpImage*)bm);

  if (err == ERR_OK) updateProgress(1.0f);
  return (_readerResult = err);
}

// ===========================================================================
// [Fog::GdiPlusDecoder - Properties]
// ===========================================================================

err_t GdiPlusDecoder::getProperty(const ManagedString& name, Value& value) const
{
  err_t err = GdiPlus_getCommonParameter(&_params, _streamType, name, value);
  if (err != (err_t)0xFFFFFFFF) return err;

  return base::getProperty(name, value);
}

err_t GdiPlusDecoder::setProperty(const ManagedString& name, const Value& value)
{
  err_t err = GdiPlus_setCommonParameter(&_params, _streamType, name, value);
  if (err != (err_t)0xFFFFFFFF) return err;

  return base::setProperty(name, value);
}

// ===========================================================================
// [Fog::GdiPlusEncoder - Construction / Destruction]
// ===========================================================================

GdiPlusEncoder::GdiPlusEncoder(ImageCodecProvider* provider) :
  ImageEncoder(provider)
{
  GdiPlus_clearCommonParameters(&_params, _streamType);
}

GdiPlusEncoder::~GdiPlusEncoder()
{
}

// ===========================================================================
// [Fog::GdiPlusEncoder - AttachStream / DetachStream]
// ===========================================================================

void GdiPlusEncoder::attachStream(Stream& stream)
{
  _istream = fog_new WinComStream(stream);

  base::attachStream(stream);
}

void GdiPlusEncoder::detachStream()
{
  if (_istream)
  {
    _istream->Release();
    _istream = NULL;
  }

  base::detachStream();
}

// ===========================================================================
// [Fog::GdiPlusEncoder - Reset]
// ===========================================================================

void GdiPlusEncoder::reset()
{
  GdiPlus_clearCommonParameters(&_params, _streamType);
  ImageEncoder::reset();
}

// ===========================================================================
// [Fog::GdiPlusEncoder - WriteImage]
// ===========================================================================

err_t GdiPlusEncoder::writeImage(const Image& image)
{
  Image tmp;
  if (image.isEmpty()) return ERR_IMAGE_INVALID_SIZE;

  err_t err = ERR_OK;
  if (_istream == NULL) return ERR_RT_INVALID_HANDLE;

  GpBitmap* bm = NULL;
  GpGraphics* gr = NULL;
  GpStatus status;

  CLSID encoderClsid;

  uint32_t fogFormat = image.getFormat();
  GpPixelFormat gpFormat = GdiPlus_gpFormatFromFogFormat(fogFormat);

  // Get GDI+ encoder CLSID.
  err = getGdiPlusEncoderClsid(
    reinterpret_cast<GdiPlusCodecProvider*>(getProvider())->_gdipMime, &encoderClsid);
  if (FOG_IS_ERROR(err)) goto _End;

  if (GdiPlus_fogFormatFromGpFormat(gpFormat) != fogFormat)
  {
    // Create GpBitmap that will share raster data with the temporary image.
    tmp = image;
    err = tmp.convert(GdiPlus_fogFormatFromGpFormat(gpFormat));
    if (FOG_IS_ERROR(err)) goto _End;

    status = _gdiPlusLibrary->pGdipCreateBitmapFromScan0(
      (INT)tmp.getWidth(),
      (INT)tmp.getHeight(),
      (INT)tmp.getStride(),
      gpFormat,
      (BYTE*)tmp.getData(),
      &bm);
  }
  else
  {
    // Create GpBitmap that will share raster data with the image.
    status = _gdiPlusLibrary->pGdipCreateBitmapFromScan0(
      (INT)image.getWidth(),
      (INT)image.getHeight(),
      (INT)image.getStride(),
      gpFormat,
      (BYTE*)image.getData(),
      &bm);
  }
  if (status != GpOk) { err = ERR_IMAGE_GDIPLUS_ERROR; goto _End; }

  // Encoder parameters.
  {
    uint8_t paramsData[sizeof(GpEncoderParameters)];
    GpEncoderParameters* params = reinterpret_cast<GpEncoderParameters*>(paramsData);

    params->Count = 0;

    switch (_streamType)
    {
       case IMAGE_STREAM_JPEG:
        params->Count = 1;
        params->Parameter[0].Guid = GpEncoderQuality;
        params->Parameter[0].Type = GpEncoderParameterValueTypeLong;
        params->Parameter[0].NumberOfValues = 1;
        params->Parameter[0].Value = &_params.jpeg.quality;
        break;
    }

    status = _gdiPlusLibrary->pGdipSaveImageToStream(
      (GpImage*)bm, _istream, &encoderClsid,
      // If there are no parameters then NULL pointer must be used instead.
      // This information can be found on MSDN. Windows Vista and Win7 will
      // return an error if (params.Count == 0).
      params->Count > 0 ? params : NULL);
  }

_End:
  // Delete created Gdi+ objects.
  if (bm) _gdiPlusLibrary->pGdipDisposeImage((GpImage*)bm);

  if (err == ERR_OK) updateProgress(1.0f);
  return err;
}

// ===========================================================================
// [Fog::GdiPlusEncoder - Properties]
// ===========================================================================

err_t GdiPlusEncoder::getProperty(const ManagedString& name, Value& value) const
{
  err_t err = GdiPlus_getCommonParameter(&_params, _streamType, name, value);
  if (err != (err_t)0xFFFFFFFF) return err;

  return base::getProperty(name, value);
}

err_t GdiPlusEncoder::setProperty(const ManagedString& name, const Value& value)
{
  err_t err = GdiPlus_setCommonParameter(&_params, _streamType, name, value);
  if (err != (err_t)0xFFFFFFFF) return err;

  return base::setProperty(name, value);
}

// ===========================================================================
// [Fog::G2d - Library Initializers]
// ===========================================================================

FOG_NO_EXPORT void _g2d_imagecodecprovider_init_gdip(void)
{
  _gdiPlusRefCount.init(0);
  GdiPlusCodecProvider* provider;

  provider = fog_new GdiPlusCodecProvider(IMAGE_STREAM_PNG);
  ImageCodecProvider::addProvider(IMAGE_CODEC_BOTH, provider);

  provider = fog_new GdiPlusCodecProvider(IMAGE_STREAM_JPEG);
  ImageCodecProvider::addProvider(IMAGE_CODEC_BOTH, provider);

  provider = fog_new GdiPlusCodecProvider(IMAGE_STREAM_TIFF);
  ImageCodecProvider::addProvider(IMAGE_CODEC_BOTH, provider);
}

} // Fog namespace

#endif // FOG_OS_WINDOWS