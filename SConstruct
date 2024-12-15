#!/usr/bin/env python

env = SConscript("godot-cpp/SConstruct")

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

library_path = "addons/CameraServerExtension/{}/libcameraserver-extension.{}{}"

sources += Glob("src/dummy/*.cpp")

library = env.SharedLibrary(
    library_path.format(env["arch"], env["platform"], env["SHLIBSUFFIX"]),
    source=sources,
)

env.NoCache(library)
Default(library)
