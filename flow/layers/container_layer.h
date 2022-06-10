// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_LAYERS_CONTAINER_LAYER_H_
#define FLUTTER_FLOW_LAYERS_CONTAINER_LAYER_H_

#include <vector>

#include "flutter/flow/layers/layer.h"

namespace flutter {

class ContainerLayer : public Layer {
 public:
  ContainerLayer();

  void Diff(DiffContext* context, const Layer* old_layer) override;
  void PreservePaintRegion(DiffContext* context) override;

  virtual void Add(std::shared_ptr<Layer> layer);

  void Preroll(PrerollContext* context, const SkMatrix& matrix) override;
  void Paint(PaintContext& context) const override;

  const std::vector<std::shared_ptr<Layer>>& layers() const { return layers_; }

  virtual void DiffChildren(DiffContext* context,
                            const ContainerLayer* old_layer);
  void PaintChildren(PaintContext& context) const;
  const ContainerLayer* as_container_layer() const override { return this; }

  const SkRect& child_paint_bounds() const { return child_paint_bounds_; }

 protected:
  void PrerollChildren(PrerollContext* context,
                       const SkMatrix& child_matrix,
                       SkRect* child_paint_bounds);

  // Try to prepare the raster cache for a given layer.
  //
  // The raster cache would fail if either of the followings is true:
  // 1. The context has a platform view.
  // 2. The context does not have a valid raster cache.
  // 3. The layer's paint bounds does not intersect with the cull rect.
  //
  // We make this a static function instead of a member function that directly
  // uses the "this" pointer as the layer because we sometimes need to raster
  // cache a child layer and one can't access its child's protected method.
  static void TryToPrepareRasterCache(PrerollContext* context,
                                      Layer* layer,
                                      const SkMatrix& matrix,
                                      RasterCacheLayerStrategy strategy);

 private:
  std::vector<std::shared_ptr<Layer>> layers_;
  SkRect child_paint_bounds_;

  FML_DISALLOW_COPY_AND_ASSIGN(ContainerLayer);
};

}  // namespace flutter

#endif  // FLUTTER_FLOW_LAYERS_CONTAINER_LAYER_H_
