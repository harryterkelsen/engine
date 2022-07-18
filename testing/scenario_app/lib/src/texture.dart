// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:scenario_app/src/scenarios.dart';

import 'channel_util.dart';
import 'scenario.dart';

/// Displays a platform texture with the given width and height.
class DisplayTexture extends Scenario {
  /// Creates the DisplayTexture scenario.
  ///
  /// The [dispatcher] parameter must not be null.
  DisplayTexture(PlatformDispatcher dispatcher)
      : super(dispatcher);

  int get _textureId => scenarioParams['texture_id'] as int;
  double get _textureWidth =>
      (scenarioParams['texture_width'] as num).toDouble();
  double get _textureHeight =>
      (scenarioParams['texture_height'] as num).toDouble();

  @override
  void onBeginFrame(Duration duration) {
    final SceneBuilder builder = SceneBuilder();
    builder.addTexture(
      _textureId,
      offset: Offset(
        (window.physicalSize.width / 2.0) - (_textureWidth / 2.0),
        0.0,
      ),
      width: _textureWidth,
      height: _textureHeight,
    );
    final Scene scene = builder.build();
    window.render(scene);
    scene.dispose();

    sendJsonMessage(
      dispatcher: dispatcher,
      channel: 'display_data',
      json: <String, dynamic>{
        'data': 'ready',
      },
    );
  }
}
