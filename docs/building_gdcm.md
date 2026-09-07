# Building GDCM 2.6.8

GDCM 2.6.8's own `CMakeLists.txt` sets several old CMake policies to their
pre-3.x behavior (`CMP0022`, `CMP0026`), and CMake versions from roughly the
last couple of years have dropped support for setting those policies to
their old values at all - configuring with a recent CMake fails with errors
like:

```
Policy CMP0022 may not be set to OLD behavior because this version of CMake
no longer supports it.
```

The straightforward fix is to configure GDCM with an older CMake, then use
whatever CMake you already have for everything else. A pinned CMake in a
throwaway Python virtual environment is the least fuss:

```bash
python3 -m venv /tmp/oldcmake
/tmp/oldcmake/bin/pip install "cmake==3.27.*"

git clone --branch v2.6.8 https://github.com/malaterre/GDCM
/tmp/oldcmake/bin/cmake -S GDCM -B gdcm-build \
  -DGDCM_BUILD_SHARED_LIBS=ON \
  -DGDCM_BUILD_DOCBOOK_MANPAGES=OFF \
  -DCMAKE_INSTALL_PREFIX=/path/to/gdcm-install
/tmp/oldcmake/bin/cmake --build gdcm-build --target install
```

Once installed, point `GDCM_DIR` at
`/path/to/gdcm-install/lib/gdcm-2.6` as usual - the rest of the build (Geant4,
OpenTOPAS, this extension) can use whatever CMake you normally have.
