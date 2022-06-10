// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_DISPLAY_LIST_DISPLAY_LIST_IMAGE_H_
#define FLUTTER_DISPLAY_LIST_DISPLAY_LIST_IMAGE_H_

#include <memory>

#include "flutter/fml/macros.h"
#include "include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkImage.h"

namespace impeller {
class Texture;
}  // namespace impeller

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Represents an image whose allocation is (usually) resident on
///             device memory.
///
///             Since it is usually impossible or expensive to transmute images
///             for one rendering backend to another, these objects are backend
///             specific.
///
class DlImage : public SkRefCnt {
 public:
  static sk_sp<DlImage> Make(const SkImage* image);

  static sk_sp<DlImage> Make(sk_sp<SkImage> image);

  virtual ~DlImage();

  //----------------------------------------------------------------------------
  /// @brief      If this display list image is meant to be used by the Skia
  ///             backend, an SkImage instance. Null otherwise.
  ///
  /// @return     A Skia image instance or null.
  ///
  virtual sk_sp<SkImage> skia_image() const = 0;

  //----------------------------------------------------------------------------
  /// @brief      If this display list image is meant to be used by the Impeller
  ///             backend, an Impeller texture instance. Null otherwise.
  ///
  /// @return     An Impeller texture instance or null.
  ///
  virtual std::shared_ptr<impeller::Texture> impeller_texture() const = 0;

  virtual bool isTextureBacked() const = 0;

  //----------------------------------------------------------------------------
  /// @return     The dimensions of the pixel grid.
  ///
  virtual SkISize dimensions() const = 0;

  //----------------------------------------------------------------------------
  /// @return     The approximate byte size of the allocation of this image.
  ///             This takes into account details such as mip-mapping. The
  ///             allocation is usually resident in device memory.
  ///
  virtual size_t GetApproximateByteSize() const = 0;

  //----------------------------------------------------------------------------
  /// @return     The width of the pixel grid. A convenience method that calls
  ///             |DlImage::dimensions|.
  ///
  int width() const;

  //----------------------------------------------------------------------------
  /// @return     The height of the pixel grid. A convenience method that calls
  ///             |DlImage::dimensions|.
  ///
  int height() const;

  //----------------------------------------------------------------------------
  /// @return     The bounds of the pixel grid with 0, 0 as origin. A
  ///             convenience method that calls |DlImage::dimensions|.
  ///
  SkIRect bounds() const;

 protected:
  DlImage();
};

}  // namespace flutter

#endif  // FLUTTER_DISPLAY_LIST_DISPLAY_LIST_IMAGE_H_
