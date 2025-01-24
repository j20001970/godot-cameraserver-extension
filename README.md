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
    camera_extension.request_permission()
# Check new camera feeds
print(CameraServer.feeds())
```

## Support Status
| Platform | Backend | Formats |
| - | :-: | :-: |
| Android | Camera2 (CPU-based) | JPEG |
| iOS | - | - |
| Linux | PipeWire | YUY2<br>YVYU<br>UYVY<br>VYUY |
| macOS | - | - |
| Windows | - | - |
| Web | - | - |

## Known Issues
- Avoid upcasting `CameraFeedExtension` to `CameraFeed`, otherwise `get_formats` and `set_format` will not work.
```gdscript
var feed := CameraServer.get_feed(0) # Returned feed will not work if it is CameraFeedExtension
var feed: CameraFeed = CameraServer.get_feed(0) # Same as above
var feed = CameraServer.get_feed(0) # Both CameraFeed and CameraFeedExtension will work
```
