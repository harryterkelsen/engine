// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/color.glsl>

// Creates a color filter that applies the inverse of the sRGB gamma curve
// to the RGB channels.

uniform sampler2D input_texture;

in vec2 v_position;
out vec4 frag_color;

void main() {
  vec4 input_color = texture(input_texture, v_position);

  for (int i = 0; i < 4; i++) {
    if (input_color[i] <= 0.04045) {
      input_color[i] = input_color[i] / 12.92;
    } else {
      input_color[i] = pow((input_color[i] + 0.055) / 1.055, 2.4);
    }
  }

  frag_color = input_color;
}
