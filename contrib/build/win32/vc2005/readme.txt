MSVC 2005 build files

Contributed by Roman Vorobets
Updated by Barry Duncan (sirlemonhead)
---

In order to build liboggkate, kateenc and katedec (both example and tools), you
will need to compile libogg first (available at http://www.xiph.org).
For the kateenc tool, you will also need to compile zlib (available at
http://www.zlib.net/) and libpng (available at http://www.libpng.org).

You must add directories to libogg.lib static libraries under Tools -> Options
-> Projects and Solutions -> VC++ Directories -> Show directories for: Library
files. Alternatively, you may want to set these under Project -> Properties ->
Configuration Properties -> Linker -> General -> Additional Library Directories
for every project if the global settings interfere with your other projects.
