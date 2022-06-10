// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_DISPLAY_LIST_DISPLAY_LIST_H_
#define FLUTTER_DISPLAY_LIST_DISPLAY_LIST_H_

#include <optional>

#include "flutter/display_list/types.h"
#include "flutter/fml/logging.h"

// The Flutter DisplayList mechanism encapsulates a persistent sequence of
// rendering operations.
//
// This file contains the definitions for:
// DisplayList: the base class that holds the information about the
//              sequence of operations and can dispatch them to a Dispatcher
// Dispatcher: a pure virtual interface which can be implemented to field
//             the requests for purposes such as sending them to an SkCanvas
//             or detecting various rendering optimization scenarios
// DisplayListBuilder: a class for constructing a DisplayList from the same
//                     calls defined in the Dispatcher
//
// Other files include various class definitions for dealing with display
// lists, such as:
// display_list_canvas.h: classes to interact between SkCanvas and DisplayList
//                        (SkCanvas->DisplayList adapter and vice versa)
//
// display_list_utils.h: various utility classes to ease implementing
//                       a Dispatcher, including NOP implementations of
//                       the attribute, clip, and transform methods,
//                       classes to track attributes, clips, and transforms
//                       and a class to compute the bounds of a DisplayList
//                       Any class implementing Dispatcher can inherit from
//                       these utility classes to simplify its creation
//
// The Flutter DisplayList mechanism can be used in place of the Skia
// SkPicture mechanism. The primary means of communication into and out
// of the DisplayList is through the Dispatcher virtual class which
// provides a nearly 1:1 translation between the records of the DisplayList
// to method calls.
//
// A DisplayList can be created directly using a DisplayListBuilder and
// the Dispatcher methods that it implements, or it can be created from
// a sequence of SkCanvas calls using the DisplayListCanvasRecorder class.
//
// A DisplayList can be read back by implementing the Dispatcher virtual
// methods (with help from some of the classes in the utils file) and
// passing an instance to the dispatch() method, or it can be rendered
// to Skia using a DisplayListCanvasDispatcher or simply by passing an
// SkCanvas pointer to its renderTo() method.
//
// The mechanism is inspired by the SkLiteDL class that is not directly
// supported by Skia, but has been recommended as a basis for custom
// display lists for a number of their customers.

namespace flutter {

#define FOR_EACH_DISPLAY_LIST_OP(V) \
  V(SetAntiAlias)                   \
  V(SetDither)                      \
  V(SetInvertColors)                \
                                    \
  V(SetStrokeCap)                   \
  V(SetStrokeJoin)                  \
                                    \
  V(SetStyle)                       \
  V(SetStrokeWidth)                 \
  V(SetStrokeMiter)                 \
                                    \
  V(SetColor)                       \
  V(SetBlendMode)                   \
                                    \
  V(SetBlender)                     \
  V(ClearBlender)                   \
                                    \
  V(SetSkPathEffect)                \
  V(SetPodPathEffect)               \
  V(ClearPathEffect)                \
                                    \
  V(ClearColorFilter)               \
  V(SetPodColorFilter)              \
  V(SetSkColorFilter)               \
                                    \
  V(ClearColorSource)               \
  V(SetPodColorSource)              \
  V(SetSkColorSource)               \
  V(SetImageColorSource)            \
                                    \
  V(ClearImageFilter)               \
  V(SetPodImageFilter)              \
  V(SetSkImageFilter)               \
  V(SetSharedImageFilter)           \
                                    \
  V(ClearMaskFilter)                \
  V(SetPodMaskFilter)               \
  V(SetSkMaskFilter)                \
                                    \
  V(Save)                           \
  V(SaveLayer)                      \
  V(SaveLayerBounds)                \
  V(SaveLayerBackdrop)              \
  V(SaveLayerBackdropBounds)        \
  V(Restore)                        \
                                    \
  V(Translate)                      \
  V(Scale)                          \
  V(Rotate)                         \
  V(Skew)                           \
  V(Transform2DAffine)              \
  V(TransformFullPerspective)       \
  V(TransformReset)                 \
                                    \
  V(ClipIntersectRect)              \
  V(ClipIntersectRRect)             \
  V(ClipIntersectPath)              \
  V(ClipDifferenceRect)             \
  V(ClipDifferenceRRect)            \
  V(ClipDifferencePath)             \
                                    \
  V(DrawPaint)                      \
  V(DrawColor)                      \
                                    \
  V(DrawLine)                       \
  V(DrawRect)                       \
  V(DrawOval)                       \
  V(DrawCircle)                     \
  V(DrawRRect)                      \
  V(DrawDRRect)                     \
  V(DrawArc)                        \
  V(DrawPath)                       \
                                    \
  V(DrawPoints)                     \
  V(DrawLines)                      \
  V(DrawPolygon)                    \
  V(DrawVertices)                   \
  V(DrawSkVertices)                 \
                                    \
  V(DrawImage)                      \
  V(DrawImageWithAttr)              \
  V(DrawImageRect)                  \
  V(DrawImageNine)                  \
  V(DrawImageNineWithAttr)          \
  V(DrawImageLattice)               \
  V(DrawAtlas)                      \
  V(DrawAtlasCulled)                \
                                    \
  V(DrawSkPicture)                  \
  V(DrawSkPictureMatrix)            \
  V(DrawDisplayList)                \
  V(DrawTextBlob)                   \
                                    \
  V(DrawShadow)                     \
  V(DrawShadowTransparentOccluder)

#define DL_OP_TO_ENUM_VALUE(name) k##name,
enum class DisplayListOpType { FOR_EACH_DISPLAY_LIST_OP(DL_OP_TO_ENUM_VALUE) };
#undef DL_OP_TO_ENUM_VALUE

class Dispatcher;
class DisplayListBuilder;

class SaveLayerOptions {
 public:
  static const SaveLayerOptions kWithAttributes;
  static const SaveLayerOptions kNoAttributes;

  SaveLayerOptions() : flags_(0) {}
  SaveLayerOptions(const SaveLayerOptions& options) : flags_(options.flags_) {}
  SaveLayerOptions(const SaveLayerOptions* options) : flags_(options->flags_) {}

  SaveLayerOptions without_optimizations() const {
    SaveLayerOptions options;
    options.fRendersWithAttributes = fRendersWithAttributes;
    return options;
  }

  bool renders_with_attributes() const { return fRendersWithAttributes; }
  SaveLayerOptions with_renders_with_attributes() const {
    SaveLayerOptions options(this);
    options.fRendersWithAttributes = true;
    return options;
  }

  bool can_distribute_opacity() const { return fCanDistributeOpacity; }
  SaveLayerOptions with_can_distribute_opacity() const {
    SaveLayerOptions options(this);
    options.fCanDistributeOpacity = true;
    return options;
  }

  SaveLayerOptions& operator=(const SaveLayerOptions& other) {
    flags_ = other.flags_;
    return *this;
  }
  bool operator==(const SaveLayerOptions& other) const {
    return flags_ == other.flags_;
  }
  bool operator!=(const SaveLayerOptions& other) const {
    return flags_ != other.flags_;
  }

 private:
  union {
    struct {
      unsigned fRendersWithAttributes : 1;
      unsigned fCanDistributeOpacity : 1;
    };
    uint32_t flags_;
  };
};

// The base class that contains a sequence of rendering operations
// for dispatch to a Dispatcher. These objects must be instantiated
// through an instance of DisplayListBuilder::build().
class DisplayList : public SkRefCnt {
 public:
  static const SkSamplingOptions NearestSampling;
  static const SkSamplingOptions LinearSampling;
  static const SkSamplingOptions MipmapSampling;
  static const SkSamplingOptions CubicSampling;

  DisplayList();

  ~DisplayList();

  void Dispatch(Dispatcher& ctx) const {
    uint8_t* ptr = storage_.get();
    Dispatch(ctx, ptr, ptr + byte_count_);
  }

  void RenderTo(DisplayListBuilder* builder,
                SkScalar opacity = SK_Scalar1) const;

  void RenderTo(SkCanvas* canvas, SkScalar opacity = SK_Scalar1) const;

  // SkPicture always includes nested bytes, but nested ops are
  // only included if requested. The defaults used here for these
  // accessors follow that pattern.
  size_t bytes(bool nested = true) const {
    return sizeof(DisplayList) + byte_count_ +
           (nested ? nested_byte_count_ : 0);
  }

  unsigned int op_count(bool nested = false) const {
    return op_count_ + (nested ? nested_op_count_ : 0);
  }

  uint32_t unique_id() const { return unique_id_; }

  const SkRect& bounds() {
    if (bounds_.width() < 0.0) {
      // ComputeBounds() will leave the variable with a
      // non-negative width and height
      ComputeBounds();
    }
    return bounds_;
  }

  bool Equals(const DisplayList* other) const;
  bool Equals(const DisplayList& other) const { return Equals(&other); }
  bool Equals(sk_sp<const DisplayList> other) const {
    return Equals(other.get());
  }

  bool can_apply_group_opacity() { return can_apply_group_opacity_; }

  static void DisposeOps(uint8_t* ptr, uint8_t* end);

 private:
  DisplayList(uint8_t* ptr,
              size_t byte_count,
              unsigned int op_count,
              size_t nested_byte_count,
              unsigned int nested_op_count,
              const SkRect& cull_rect,
              bool can_apply_group_opacity);

  std::unique_ptr<uint8_t, SkFunctionWrapper<void(void*), sk_free>> storage_;
  size_t byte_count_;
  unsigned int op_count_;

  size_t nested_byte_count_;
  unsigned int nested_op_count_;

  uint32_t unique_id_;
  SkRect bounds_;

  // Only used for drawPaint() and drawColor()
  SkRect bounds_cull_;

  bool can_apply_group_opacity_;

  void ComputeBounds();
  void Dispatch(Dispatcher& ctx, uint8_t* ptr, uint8_t* end) const;

  friend class DisplayListBuilder;
};

}  // namespace flutter

#endif  // FLUTTER_DISPLAY_LIST_DISPLAY_LIST_H_
