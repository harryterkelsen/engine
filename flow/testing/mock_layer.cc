// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/testing/mock_layer.h"

#include "flutter/flow/layers/container_layer.h"
#include "flutter/flow/layers/layer.h"
#include "flutter/flow/testing/mock_raster_cache.h"
namespace flutter {
namespace testing {

MockLayer::MockLayer(SkPath path,
                     SkPaint paint,
                     bool fake_has_platform_view,
                     bool fake_reads_surface,
                     bool fake_opacity_compatible,
                     bool fake_has_texture_layer)
    : fake_paint_path_(path),
      fake_paint_(paint),
      fake_has_platform_view_(fake_has_platform_view),
      fake_reads_surface_(fake_reads_surface),
      fake_opacity_compatible_(fake_opacity_compatible),
      fake_has_texture_layer_(fake_has_texture_layer) {}

bool MockLayer::IsReplacing(DiffContext* context, const Layer* layer) const {
  // Similar to PictureLayer, only return true for identical mock layers;
  // That way ContainerLayer::DiffChildren can properly detect mock layer
  // insertion
  auto mock_layer = layer->as_mock_layer();
  return mock_layer && mock_layer->fake_paint_ == fake_paint_ &&
         mock_layer->fake_paint_path_ == fake_paint_path_;
}

void MockLayer::Diff(DiffContext* context, const Layer* old_layer) {
  DiffContext::AutoSubtreeRestore subtree(context);
  context->AddLayerBounds(fake_paint_path_.getBounds());
  context->SetLayerPaintRegion(this, context->CurrentSubtreeRegion());
}

void MockLayer::Preroll(PrerollContext* context, const SkMatrix& matrix) {
  parent_mutators_ = context->mutators_stack;
  parent_matrix_ = matrix;
  parent_cull_rect_ = context->cull_rect;
  parent_has_platform_view_ = context->has_platform_view;
  parent_has_texture_layer_ = context->has_texture_layer;

  context->has_platform_view = fake_has_platform_view_;
  context->has_texture_layer = fake_has_texture_layer_;
  set_paint_bounds(fake_paint_path_.getBounds());
  if (fake_reads_surface_) {
    context->surface_needs_readback = true;
  }
  if (fake_opacity_compatible_) {
    context->subtree_can_inherit_opacity = true;
  }
}

void MockLayer::Paint(PaintContext& context) const {
  FML_DCHECK(needs_painting(context));

  if (context.inherited_opacity < SK_Scalar1) {
    SkPaint p;
    p.setAlphaf(context.inherited_opacity);
    context.leaf_nodes_canvas->saveLayer(fake_paint_path_.getBounds(), &p);
  }
  context.leaf_nodes_canvas->drawPath(fake_paint_path_, fake_paint_);
  if (context.inherited_opacity < SK_Scalar1) {
    context.leaf_nodes_canvas->restore();
  }
}

void MockCacheableContainerLayer::Preroll(PrerollContext* context,
                                          const SkMatrix& matrix) {
  Layer::AutoPrerollSaveLayerState save =
      Layer::AutoPrerollSaveLayerState::Create(context);
  auto cache = AutoCache(layer_raster_cache_item_.get(), context, matrix);

  ContainerLayer::Preroll(context, matrix);
}

void MockCacheableLayer::Preroll(PrerollContext* context,
                                 const SkMatrix& matrix) {
  Layer::AutoPrerollSaveLayerState save =
      Layer::AutoPrerollSaveLayerState::Create(context);
  auto cache = AutoCache(raster_cache_item_.get(), context, matrix);

  MockLayer::Preroll(context, matrix);
}

}  // namespace testing
}  // namespace flutter
