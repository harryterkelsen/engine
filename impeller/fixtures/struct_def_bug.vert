// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

uniform FrameInfo {
  mat4 mvp;
  vec2 atlas_size;
  vec4 text_color;
} frame_info;

in vec2 unit_vertex;
in mat4 glyph_position; // <--- Causes multiple slots to be used and is a failure.
in vec2 glyph_size;
in vec2 atlas_position;
in vec2 atlas_glyph_size;

out vec2 v_unit_vertex;
out vec2 v_atlas_position;
out vec2 v_atlas_glyph_size;
out vec2 v_atlas_size;
out vec4 v_text_color;

void main() {
  gl_Position = frame_info.mvp
              * glyph_position
              * vec4(unit_vertex.x * glyph_size.x,
                     unit_vertex.y * glyph_size.y, 0.0, 1.0);

  v_unit_vertex = unit_vertex;
  v_atlas_position = atlas_position;
  v_atlas_glyph_size = atlas_glyph_size;
  v_atlas_size = frame_info.atlas_size;
  v_text_color = frame_info.text_color;
}
