// [Fog-G2d]
//
// [License]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_G2D_TEXT_MACFONTPROVIDER_H
#define _FOG_G2D_TEXT_MACFONTPROVIDER_H

// [Dependencies]
#include <Fog/Core/OS/MacDefs.h>
#include <Fog/G2d/Text/FontFaceCache.h>
#include <Fog/G2d/Text/FontProvider.h>

namespace Fog {

//! @addtogroup Fog_G2d_Text
//! @{

// ============================================================================
// [Fog::MacFontProviderData]
// ============================================================================

//! @brief Mac font-provider data.
struct FOG_API MacFontProviderData : public FontProviderData
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MacFontProviderData();
  virtual ~MacFontProviderData();

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  virtual err_t getFontFace(FontFace** dst, const StringW& fontFamily);
  virtual err_t getFontList(List<StringW>& dst);

  virtual StringW getDefaultFamily();

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  void _reset();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  FontFaceCache fontFaceCache;
  NSFontManager* mgr;

private:
  _FOG_NO_COPY(MacFontProviderData)
};

//! @}

} // Fog namespace

// [Guard]
#endif // _FOG_G2D_TEXT_MACFONTPROVIDER_H