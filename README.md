# CameraServerExtension
CameraServerExtension is a Godot 4.4+ plugin that extends the support of original CameraServer to multiple platforms.

## Usage
`CameraServerExtension` class will be available to Godot once the plugin is loaded, after creating a `CameraServerExtension` instance, it can be used for checking camera access permission and making permission request, newly created `CameraFeedExtension` can be found in `CameraServer`.

```gdscript
var camera_extension := CameraServerExtension.new()
# Check camera permission
if camera_extension.permission_granted():
    # All good
    pass
else:
    var _on_permission_result = func(granted: bool) -> void:
        if not granted:
            print("Camera access permission not granted")
            return
    camera_extesion.permission_result.connect(_on_permission_result)
    camera_extension.request_permission()
# Check new camera feeds
print(CameraServer.feeds())
```

## Support Status
<table>
    <tbody>
        <tr>
            <th>Platform</th>
            <th>Backend</th>
            <th>Formats</th>
            <th>Notes</th>
        </tr>
        <tr>
            <td>Android</td>
            <td align="center">Camera2 (CPU-based)</td>
            <td>
                <ul>
                    <li>JPEG</li>
                </ul>
            </td>
            <td>
                <ul>
                    <li>Official Godot support using NDK since v4.5 (https://github.com/godotengine/godot/pull/106094)</li>
                </ul>
            </td>
        </tr>
        <tr>
            <td>Linux</td>
            <td align="center">PipeWire</td>
            <td>
                <ul>
                    <li>YUY2</li>
                    <li>YVYU</li>
                    <li>UYVY</li>
                    <li>VYUY</li>
                </ul>
            </td>
            <td>
                <ul>
                    <li>Official Godot support using V4L2 since v4.4 (https://github.com/godotengine/godot/pull/53666)</li>
                    <li>Upstream PR: https://github.com/godotengine/godot/pull/109500</li>
                </ul>
            </td>
        </tr>
        <tr>
            <td>iOS</td>
            <td align="center" rowspan=2>AVFoundation</td>
            <td align="center" rowspan=2>-</td>
            <td>
                <ul>
                    <li>Untested</li>
                    <li>Upstream PR: https://github.com/godotengine/godot/pull/106305</li>
                </ul>
            </td>
        </tr>
        <tr>
            <td>macOS</td>
            <td align="center">-</td>
        </tr>
        <tr>
            <td>Windows</td>
            <td align="center">Media Foundation</td>
            <td>
                <ul>
                    <li>YUY2</li>
                    <li>NV12</li>
                    <li>MJPG</li>
                </ul>
            </td>
            <td>
                <ul>
                    <li>Upstream PR: https://github.com/godotengine/godot/pull/105476</li>
                    <li>Upstream PR: https://github.com/godotengine/godot/pull/108538</li>
                </ul>
            </td>
        </tr>
        <tr>
            <td>Web</td>
            <td align="center">-</td>
            <td align="center">-</td>
            <td>
                <ul>
                    <li>Upstream PR: https://github.com/godotengine/godot/pull/106784</li>
                </ul>
            </td>
        </tr>
    </tbody>
</table>

## Known Issues
- Avoid upcasting `CameraFeedExtension` to `CameraFeed`, otherwise `get_formats` and `set_format` will not work.
```gdscript
var feed := CameraServer.get_feed(0) # Returned feed will not work if it is CameraFeedExtension
var feed: CameraFeed = CameraServer.get_feed(0) # Same as above
var feed = CameraServer.get_feed(0) # Both CameraFeed and CameraFeedExtension will work
```
- Since Godot v4.5, CameraServerExtension requires setting `CameraServer.monitoring_feeds` to `true` before instantiation to provide custom feeds.
