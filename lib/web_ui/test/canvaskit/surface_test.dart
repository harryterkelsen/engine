// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:html' as html;

import 'package:test/bootstrap/browser.dart';
import 'package:test/test.dart';
import 'package:ui/src/engine.dart';
import 'package:ui/ui.dart' as ui;

import 'common.dart';

void main() {
  internalBootstrapBrowserTest(() => testMain);
}

void testMain() {
  group('CanvasKit', () {
    setUpCanvasKitTest();
    setUp(() {
      window.debugOverrideDevicePixelRatio(1.0);
    });

    test('Surface allocates canvases efficiently', () {
      final Surface surface = SurfaceFactory.instance.getSurface();
      surface.acquireFrame(ui.Size(9, 19));
      final html.CanvasElement original = surface.htmlCanvas!;

      // Expect exact requested dimensions.
      expect(original.width, 9);
      expect(original.height, 19);
      expect(surface.htmlCanvas!.style.width, '9px');
      expect(surface.htmlCanvas!.style.height, '19px');

      // Shrinking reuses the existing canvas straight-up.
      surface.acquireFrame(ui.Size(5, 15));
      final html.CanvasElement shrunk = surface.htmlCanvas!;
      expect(shrunk, same(original));
      expect(surface.htmlCanvas!.style.width, '9px');
      expect(surface.htmlCanvas!.style.height, '19px');

      // The first increase will allocate a new canvas, but will overallocate
      // by 40% to accommodate future increases.
      surface.acquireFrame(ui.Size(10, 20));
      final html.CanvasElement firstIncrease = surface.htmlCanvas!;
      expect(firstIncrease, isNot(same(original)));

      // Expect overallocated dimensions
      expect(firstIncrease.width, 14);
      expect(firstIncrease.height, 28);
      expect(surface.htmlCanvas!.style.width, '14px');
      expect(surface.htmlCanvas!.style.height, '28px');

      // Subsequent increases within 40% reuse the old canvas.
      surface.acquireFrame(ui.Size(11, 22));
      final html.CanvasElement secondIncrease = surface.htmlCanvas!;
      expect(secondIncrease, same(firstIncrease));

      // Increases beyond the 40% limit will cause a new allocation.
      surface.acquireFrame(ui.Size(20, 40));
      final html.CanvasElement huge = surface.htmlCanvas!;
      expect(huge, isNot(same(firstIncrease)));

      // Also over-allocated
      expect(huge.width, 28);
      expect(huge.height, 56);
      expect(surface.htmlCanvas!.style.width, '28px');
      expect(surface.htmlCanvas!.style.height, '56px');

      // Shrink again. Reuse the last allocated surface.
      surface.acquireFrame(ui.Size(5, 15));
      final html.CanvasElement shrunk2 = surface.htmlCanvas!;
      expect(shrunk2, same(huge));
    });

    test(
      'Surface creates new context when WebGL context is restored',
      () async {
        final Surface surface = SurfaceFactory.instance.getSurface();
        expect(surface.debugForceNewContext, isTrue);
        final CkSurface before =
            surface.acquireFrame(ui.Size(9, 19)).skiaSurface;
        expect(surface.debugForceNewContext, isFalse);

        // Pump a timer to flush any microtasks.
        await Future<void>.delayed(Duration.zero);
        final CkSurface afterAcquireFrame =
            surface.acquireFrame(ui.Size(9, 19)).skiaSurface;
        // Existing context is reused.
        expect(afterAcquireFrame, same(before));

        // Emulate WebGL context loss.
        final html.CanvasElement canvas =
            surface.htmlElement.children.single as html.CanvasElement;
        final dynamic ctx = canvas.getContext('webgl2');
        expect(ctx, isNotNull);
        final dynamic loseContextExtension =
            ctx.getExtension('WEBGL_lose_context');
        loseContextExtension.loseContext();

        // Pump a timer to allow the "lose context" event to propagate.
        await Future<void>.delayed(Duration.zero);
        // We don't create a new GL context until the context is restored.
        expect(surface.debugContextLost, isTrue);
        expect(ctx.isContextLost(), isTrue);

        // Emulate WebGL context restoration.
        loseContextExtension.restoreContext();

        // Pump a timer to allow the "restore context" event to propagate.
        await Future<void>.delayed(Duration.zero);
        expect(surface.debugForceNewContext, isTrue);

        final CkSurface afterContextLost =
            surface.acquireFrame(ui.Size(9, 19)).skiaSurface;
        // A new context is created.
        expect(afterContextLost, isNot(same(before)));
      },
      // Firefox doesn't have the WEBGL_lose_context extension.
      skip: isFirefox || isSafari,
    );

    // Regression test for https://github.com/flutter/flutter/issues/75286
    test('updates canvas logical size when device-pixel ratio changes', () {
      final Surface surface = Surface();
      final CkSurface original =
          surface.acquireFrame(ui.Size(10, 16)).skiaSurface;

      expect(original.width(), 10);
      expect(original.height(), 16);
      expect(surface.htmlCanvas!.style.width, '10px');
      expect(surface.htmlCanvas!.style.height, '16px');

      // Increase device-pixel ratio: this makes CSS pixels bigger, so we need
      // fewer of them to cover the browser window.
      window.debugOverrideDevicePixelRatio(2.0);
      final CkSurface highDpr =
          surface.acquireFrame(ui.Size(10, 16)).skiaSurface;
      expect(highDpr.width(), 10);
      expect(highDpr.height(), 16);
      expect(surface.htmlCanvas!.style.width, '5px');
      expect(surface.htmlCanvas!.style.height, '8px');

      // Decrease device-pixel ratio: this makes CSS pixels smaller, so we need
      // more of them to cover the browser window.
      window.debugOverrideDevicePixelRatio(0.5);
      final CkSurface lowDpr =
          surface.acquireFrame(ui.Size(10, 16)).skiaSurface;
      expect(lowDpr.width(), 10);
      expect(lowDpr.height(), 16);
      expect(surface.htmlCanvas!.style.width, '20px');
      expect(surface.htmlCanvas!.style.height, '32px');
    });
  }, skip: isIosSafari);
}
