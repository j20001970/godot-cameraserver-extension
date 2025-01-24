extends EditorExportPlugin

func _supports_platform(platform: EditorExportPlatform) -> bool:
	if platform is EditorExportPlatformAndroid:
		return true
	return false

func _get_android_libraries(platform: EditorExportPlatform, debug: bool) -> PackedStringArray:
	if debug:
		return PackedStringArray(["CameraServerExtension/CameraServerExtension-debug.aar"])
	else:
		return PackedStringArray(["CameraServerExtension/CameraServerExtension-release.aar"])

func _get_android_dependencies(platform: EditorExportPlatform, debug: bool) -> PackedStringArray:
	return PackedStringArray([])

func _get_name():
	return "CameraServerExtension"
