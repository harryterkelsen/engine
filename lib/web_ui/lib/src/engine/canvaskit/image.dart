// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:js_interop';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:ui/src/engine.dart';
import 'package:ui/ui.dart' as ui;
import 'package:ui/ui_web/src/ui_web.dart' as ui_web;

/// Instantiates a [ui.Codec] backed by an `SkAnimatedImage` from Skia.
Future<ui.Codec> skiaInstantiateImageCodec(Uint8List list,
    [int? targetWidth, int? targetHeight, bool allowUpscaling = true]) async {
  ui.Codec codec;
  // ImageDecoder does not detect image type automatically. It requires us to
  // tell it what the image type is.
  final String contentType = tryDetectContentType(list, 'encoded image bytes');

  if (browserSupportsImageDecoder) {
    codec = await CkBrowserImageDecoder.create(
      data: list,
      contentType: contentType,
      debugSource: 'encoded image bytes',
    );
  } else {
    // TODO(harryterkelsen): If the image is animated, then use Skia to decode.
    final DomBlob blob = createDomBlob(<ByteBuffer>[list.buffer]);
    codec = await decodeBlobToCkImage(blob);
  }
  return ResizingCodec(
    codec,
    targetWidth: targetWidth,
    targetHeight: targetHeight,
    allowUpscaling: allowUpscaling,
  );
}

ui.Image createCkImageFromImageElement(
  DomHTMLImageElement image,
  int naturalWidth,
  int naturalHeight,
) {
  final SkImage? skImage = canvasKit.MakeLazyImageFromTextureSourceWithInfo(
    image,
    SkPartialImageInfo(
      alphaType: canvasKit.AlphaType.Premul,
      colorType: canvasKit.ColorType.RGBA_8888,
      colorSpace: SkColorSpaceSRGB,
      width: naturalWidth.toDouble(),
      height: naturalHeight.toDouble(),
    ),
  );
  if (skImage == null) {
    throw ImageCodecException(
      'Failed to create image from Image.decode',
    );
  }

  return CkImage(skImage, imageElement: image);
}

class CkImageElementCodec extends HtmlImageElementCodec {
  CkImageElementCodec(super.src);

  @override
  ui.Image createImageFromHTMLImageElement(
          DomHTMLImageElement image, int naturalWidth, int naturalHeight) =>
      createCkImageFromImageElement(image, naturalWidth, naturalHeight);
}

class CkImageBlobCodec extends HtmlBlobCodec {
  CkImageBlobCodec(super.blob);

  @override
  ui.Image createImageFromHTMLImageElement(
          DomHTMLImageElement image, int naturalWidth, int naturalHeight) =>
      createCkImageFromImageElement(image, naturalWidth, naturalHeight);
}

/// Creates and decodes an image using HtmlImageElement.
Future<CkImageBlobCodec> decodeBlobToCkImage(DomBlob blob) async {
  final CkImageBlobCodec codec = CkImageBlobCodec(blob);
  await codec.decode();
  return codec;
}

Future<CkImageElementCodec> decodeUrlToCkImage(String src) async {
  final CkImageElementCodec codec = CkImageElementCodec(src);
  await codec.decode();
  return codec;
}

void skiaDecodeImageFromPixels(
  Uint8List pixels,
  int width,
  int height,
  ui.PixelFormat format,
  ui.ImageDecoderCallback callback, {
  int? rowBytes,
  int? targetWidth,
  int? targetHeight,
  bool allowUpscaling = true,
}) {
  if (targetWidth != null) {
    assert(allowUpscaling || targetWidth <= width);
  }
  if (targetHeight != null) {
    assert(allowUpscaling || targetHeight <= height);
  }

  // Run in a timer to avoid janking the current frame by moving the decoding
  // work outside the frame event.
  Timer.run(() {
    final SkImage? skImage = canvasKit.MakeImage(
      SkImageInfo(
        width: width.toDouble(),
        height: height.toDouble(),
        colorType: format == ui.PixelFormat.rgba8888
            ? canvasKit.ColorType.RGBA_8888
            : canvasKit.ColorType.BGRA_8888,
        alphaType: canvasKit.AlphaType.Premul,
        colorSpace: SkColorSpaceSRGB,
      ),
      pixels,
      (rowBytes ?? 4 * width).toDouble(),
    );

    if (skImage == null) {
      domWindow.console.warn('Failed to create image from pixels.');
      return;
    }

    if (targetWidth != null || targetHeight != null) {
      if (validUpscale(
          allowUpscaling, targetWidth, targetHeight, width, height)) {
        return callback(scaleImage(skImage, targetWidth, targetHeight));
      }
    }
    return callback(CkImage(skImage));
  });
}

// An invalid upscale happens when allowUpscaling is false AND either the given
// targetWidth is larger than the originalWidth OR the targetHeight is larger than originalHeight.
bool validUpscale(bool allowUpscaling, int? targetWidth, int? targetHeight,
    int originalWidth, int originalHeight) {
  if (allowUpscaling) {
    return true;
  }
  final bool targetWidthFits;
  final bool targetHeightFits;
  if (targetWidth != null) {
    targetWidthFits = targetWidth <= originalWidth;
  } else {
    targetWidthFits = true;
  }

  if (targetHeight != null) {
    targetHeightFits = targetHeight <= originalHeight;
  } else {
    targetHeightFits = true;
  }
  return targetWidthFits && targetHeightFits;
}

/// Creates a scaled [CkImage] from an [SkImage] by drawing the [SkImage] to a canvas.
///
/// This function will only be called if either a targetWidth or targetHeight is not null
///
/// If only one of targetWidth or  targetHeight are specified, the other
/// dimension will be scaled according to the aspect ratio of the supplied
/// dimension.
///
/// If either targetWidth or targetHeight is less than or equal to zero, it
/// will be treated as if it is null.
CkImage scaleImage(SkImage image, int? targetWidth, int? targetHeight) {
  assert(targetWidth != null || targetHeight != null);
  if (targetWidth != null && targetWidth <= 0) {
    targetWidth = null;
  }
  if (targetHeight != null && targetHeight <= 0) {
    targetHeight = null;
  }
  if (targetWidth == null && targetHeight != null) {
    targetWidth = (targetHeight * (image.width() / image.height())).round();
  } else if (targetHeight == null && targetWidth != null) {
    targetHeight = targetWidth ~/ (image.width() / image.height());
  }

  assert(targetWidth != null);
  assert(targetHeight != null);

  final CkPictureRecorder recorder = CkPictureRecorder();
  final CkCanvas canvas = recorder.beginRecording(ui.Rect.largest);

  final CkPaint paint = CkPaint();
  canvas.drawImageRect(
    CkImage(image),
    ui.Rect.fromLTWH(0, 0, image.width(), image.height()),
    ui.Rect.fromLTWH(0, 0, targetWidth!.toDouble(), targetHeight!.toDouble()),
    paint,
  );
  paint.dispose();

  final CkPicture picture = recorder.endRecording();
  final ui.Image finalImage = picture.toImageSync(targetWidth, targetHeight);

  final CkImage ckImage = finalImage as CkImage;
  return ckImage;
}

CkImage scaleImageWithCanvas(
    SkImage image, int? targetWidth, int? targetHeight) {}

/// Thrown when the web engine fails to decode an image, either due to a
/// network issue, corrupted image contents, or missing codec.
class ImageCodecException implements Exception {
  ImageCodecException(this._message);

  final String _message;

  @override
  String toString() => 'ImageCodecException: $_message';
}

const String _kNetworkImageMessage = 'Failed to load network image.';

/// Instantiates a [ui.Codec] backed by an `SkAnimatedImage` from Skia after
/// requesting from URI.
Future<ui.Codec> skiaInstantiateWebImageCodec(
    String url, ui_web.ImageCodecChunkCallback? chunkCallback) async {
  final CkImageElementCodec imageElementCodec = CkImageElementCodec(url);
  try {
    await imageElementCodec.decode();
    return imageElementCodec;
  } on ImageCodecException {
    imageElementCodec.dispose();
    final Uint8List list = await fetchImage(url, chunkCallback);
    final String contentType = tryDetectContentType(list, url);
    if (browserSupportsImageDecoder) {
      return CkBrowserImageDecoder.create(
          data: list, contentType: contentType, debugSource: url);
    } else {
      final DomBlob blob = createDomBlob(<ByteBuffer>[list.buffer]);
      final CkImageBlobCodec codec = CkImageBlobCodec(blob);

      try {
        await codec.decode();
        return codec;
      } on ImageCodecException {
        codec.dispose();
        return CkAnimatedImage.decodeFromBytes(list, url);
      }
    }
  }
}

/// Sends a request to fetch image data.
Future<Uint8List> fetchImage(
    String url, ui_web.ImageCodecChunkCallback? chunkCallback) async {
  try {
    final HttpFetchResponse response = await httpFetch(url);
    final int? contentLength = response.contentLength;

    if (!response.hasPayload) {
      throw ImageCodecException(
        '$_kNetworkImageMessage\n'
        'Image URL: $url\n'
        'Server response code: ${response.status}',
      );
    }

    if (chunkCallback != null && contentLength != null) {
      return readChunked(response.payload, contentLength, chunkCallback);
    } else {
      return await response.asUint8List();
    }
  } on HttpFetchError catch (_) {
    throw ImageCodecException(
      '$_kNetworkImageMessage\n'
      'Image URL: $url\n'
      'Trying to load an image from another domain? Find answers at:\n'
      'https://flutter.dev/docs/development/platform-integration/web-images',
    );
  }
}

/// Reads the [payload] in chunks using the browser's Streams API
///
/// See: https://developer.mozilla.org/en-US/docs/Web/API/Streams_API
Future<Uint8List> readChunked(HttpFetchPayload payload, int contentLength,
    ui_web.ImageCodecChunkCallback chunkCallback) async {
  final JSUint8Array result = createUint8ArrayFromLength(contentLength);
  int position = 0;
  int cumulativeBytesLoaded = 0;
  await payload.read<JSUint8Array>((JSUint8Array chunk) {
    cumulativeBytesLoaded += chunk.length.toDartInt;
    chunkCallback(cumulativeBytesLoaded, contentLength);
    result.set(chunk, position.toJS);
    position += chunk.length.toDartInt;
  });
  return result.toDart;
}

/// A [ui.Image] backed by an `SkImage` from Skia.
class CkImage implements ui.Image, StackTraceDebugger {
  CkImage(SkImage skImage, {this.videoFrame, this.imageElement}) {
    box = CountedRef<CkImage, SkImage>(skImage, this, 'SkImage');
    _init();
  }

  CkImage.cloneOf(this.box, {this.videoFrame, this.imageElement}) {
    _init();
    box.ref(this);
  }

  void _init() {
    assert(() {
      _debugStackTrace = StackTrace.current;
      return true;
    }());
    ui.Image.onCreate?.call(this);
  }

  @override
  StackTrace get debugStackTrace => _debugStackTrace;
  late StackTrace _debugStackTrace;

  // Use ref counting because `SkImage` may be deleted either due to this object
  // being garbage-collected, or by an explicit call to [delete].
  late final CountedRef<CkImage, SkImage> box;

  /// For browsers that support `ImageDecoder` this field holds the video frame
  /// from which this image was created.
  ///
  /// Skia owns the video frame and will close it when it's no longer used.
  /// However, Flutter co-owns the [SkImage] and therefore it's safe to access
  /// the video frame until the image is [dispose]d of.
  VideoFrame? videoFrame;

  /// For images which are decoded via an HTML Image element, this field holds
  /// the image element from which this image was created.
  ///
  /// Skia owns the image element and will close it when it's no longer used.
  /// However, Flutter co-owns the [SkImage] and therefore it's safe to access
  /// the image element until the image is [dispose]d of.
  DomHTMLImageElement? imageElement;

  /// The underlying Skia image object.
  ///
  /// Do not store the returned value. It is memory-managed by [CountedRef].
  /// Storing it may result in use-after-free bugs.
  SkImage get skImage => box.nativeObject;

  bool _disposed = false;

  bool _debugCheckIsNotDisposed() {
    assert(!_disposed, 'This image has been disposed.');
    return true;
  }

  @override
  void dispose() {
    assert(
      !_disposed,
      'Cannot dispose an image that has already been disposed.',
    );
    ui.Image.onDispose?.call(this);
    _disposed = true;
    box.unref(this);
  }

  @override
  bool get debugDisposed {
    bool? result;
    assert(() {
      result = _disposed;
      return true;
    }());

    if (result != null) {
      return result!;
    }

    throw StateError(
        'Image.debugDisposed is only available when asserts are enabled.');
  }

  @override
  CkImage clone() {
    assert(_debugCheckIsNotDisposed());
    return CkImage.cloneOf(
      box,
      videoFrame: videoFrame?.clone(),
      imageElement: imageElement,
    );
  }

  @override
  bool isCloneOf(ui.Image other) {
    assert(_debugCheckIsNotDisposed());
    return other is CkImage && other.skImage.isAliasOf(skImage);
  }

  @override
  List<StackTrace>? debugGetOpenHandleStackTraces() =>
      box.debugGetStackTraces();

  @override
  int get width {
    assert(_debugCheckIsNotDisposed());
    return skImage.width().toInt();
  }

  @override
  int get height {
    assert(_debugCheckIsNotDisposed());
    return skImage.height().toInt();
  }

  @override
  Future<ByteData> toByteData({
    ui.ImageByteFormat format = ui.ImageByteFormat.rawRgba,
  }) {
    assert(_debugCheckIsNotDisposed());
    // readPixelsFromVideoFrame currently does not convert I420, I444, I422
    // videoFrame formats to RGBA
    if (videoFrame != null &&
        videoFrame!.format != 'I420' &&
        videoFrame!.format != 'I444' &&
        videoFrame!.format != 'I422') {
      return readPixelsFromVideoFrame(videoFrame!, format);
    } else if (imageElement != null) {
      return readPixelsFromImageElement(imageElement!, format);
    } else {
      ByteData? data = _readPixelsFromSkImage(format);
      data ??= _readPixelsFromImageViaSurface(format);
      if (data == null) {
        return Future<ByteData>.error('Failed to encode the image into bytes.');
      } else {
        return Future<ByteData>.value(data);
      }
    }
  }

  @override
  ui.ColorSpace get colorSpace => ui.ColorSpace.sRGB;

  ByteData? _readPixelsFromSkImage(ui.ImageByteFormat format) {
    final SkAlphaType alphaType = format == ui.ImageByteFormat.rawStraightRgba
        ? canvasKit.AlphaType.Unpremul
        : canvasKit.AlphaType.Premul;
    final ByteData? data = _encodeImage(
      skImage: skImage,
      format: format,
      alphaType: alphaType,
      colorType: canvasKit.ColorType.RGBA_8888,
      colorSpace: SkColorSpaceSRGB,
    );
    return data;
  }

  ByteData? _readPixelsFromImageViaSurface(ui.ImageByteFormat format) {
    final Surface surface = CanvasKitRenderer.instance.pictureToImageSurface;
    final CkSurface ckSurface =
        surface.createOrUpdateSurface(BitmapSize(width, height));
    final CkCanvas ckCanvas = ckSurface.getCanvas();
    ckCanvas.clear(const ui.Color(0x00000000));
    ckCanvas.drawImage(this, ui.Offset.zero, CkPaint());
    final SkImage skImage = ckSurface.surface.makeImageSnapshot();
    final SkImageInfo imageInfo = SkImageInfo(
      alphaType: canvasKit.AlphaType.Premul,
      colorType: canvasKit.ColorType.RGBA_8888,
      colorSpace: SkColorSpaceSRGB,
      width: width.toDouble(),
      height: height.toDouble(),
    );
    final Uint8List? pixels = skImage.readPixels(0, 0, imageInfo);
    if (pixels == null) {
      throw StateError('Unable to convert read pixels from SkImage.');
    }
    return pixels.buffer.asByteData();
  }

  static ByteData? _encodeImage({
    required SkImage skImage,
    required ui.ImageByteFormat format,
    required SkAlphaType alphaType,
    required SkColorType colorType,
    required ColorSpace colorSpace,
  }) {
    Uint8List? bytes;

    if (format == ui.ImageByteFormat.rawRgba ||
        format == ui.ImageByteFormat.rawStraightRgba) {
      final SkImageInfo imageInfo = SkImageInfo(
        alphaType: alphaType,
        colorType: colorType,
        colorSpace: colorSpace,
        width: skImage.width(),
        height: skImage.height(),
      );
      bytes = skImage.readPixels(0, 0, imageInfo);
    } else {
      bytes = skImage.encodeToBytes(); // defaults to PNG 100%
    }

    return bytes?.buffer.asByteData(0, bytes.length);
  }

  @override
  String toString() {
    assert(_debugCheckIsNotDisposed());
    return '[$width\u00D7$height]';
  }
}

/// Detect the content type or throw an error if content type can't be detected.
String tryDetectContentType(Uint8List data, String debugSource) {
  // ImageDecoder does not detect image type automatically. It requires us to
  // tell it what the image type is.
  final String? contentType = detectContentType(data);

  if (contentType == null) {
    final String fileHeader;
    if (data.isNotEmpty) {
      fileHeader =
          '[${bytesToHexString(data.sublist(0, math.min(10, data.length)))}]';
    } else {
      fileHeader = 'empty';
    }
    throw ImageCodecException(
        'Failed to detect image file format using the file header.\n'
        'File header was $fileHeader.\n'
        'Image source: $debugSource');
  }
  return contentType;
}
