#!/usr/bin/env python

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

library_path = "addons/CameraServerExtension/{}/libcameraserver-extension.{}{}"

if env["platform"] == "android":
    sources += Glob("src/android/*.cpp")
elif env["platform"] == "macos" or env["platform"] == "ios":
    env.Append(LINKFLAGS=["-framework", "AVFoundation"])
    env.Append(LINKFLAGS=["-framework", "CoreMedia"])
    env.Append(LINKFLAGS=["-framework", "CoreVideo"])
    env.Append(LINKFLAGS=["-framework", "Foundation"])
    sources += Glob("src/apple/*.mm")
elif env["platform"] == "linux":
    env.ParseConfig("pkg-config glib-2.0 --cflags --libs")
    env.ParseConfig("pkg-config gio-2.0 --cflags --libs")
    env.ParseConfig("pkg-config libpipewire-0.3 --cflags --libs")
    sources += Glob("src/linux/*.cpp")
elif env["platform"] == "windows":
    env.Append(LIBS=[
        "advapi32",
        "ole32",
        "shlwapi",
        "mf",
        "mfuuid",
        "mfplat",
        "mfreadwrite"
    ])
    sources += Glob("src/windows/*.cpp")
else:
    sources += Glob("src/dummy/*.cpp")

library = env.SharedLibrary(
    library_path.format(env["arch"], env["platform"], env["SHLIBSUFFIX"]),
    source=sources,
)

env.NoCache(library)
Default(library)
