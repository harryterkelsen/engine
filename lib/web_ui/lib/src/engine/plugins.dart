part of engine;

Future<void> Function(String, ByteData, ui.PlatformMessageResponseCallback) pluginMessageCallHandler;

void webOnlySetPluginHandler(Future<void> Function(String, ByteData, ui.PlatformMessageResponseCallback) handler) {
  pluginMessageCallHandler = handler;
}
