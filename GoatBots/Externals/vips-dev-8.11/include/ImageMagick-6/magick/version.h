/*
  Copyright 1999-2021 ImageMagick Studio LLC, a non-profit organization
  dedicated to making software imaging solutions freely available.
  
  You may not use this file except in compliance with the License.
  obtain a copy of the License at
  
    https://imagemagick.org/script/license.php
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  MagickCore version methods.
*/
#ifndef _MAGICKCORE_VERSION_H
#define _MAGICKCORE_VERSION_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*
  Define declarations.
*/
#define MagickPackageName "ImageMagick"
#define MagickCopyright  "(C) 1999-2021 ImageMagick Studio LLC"
#define MagickLibVersion  0x69C
#define MagickLibVersionText  "6.9.12"
#define MagickLibVersionNumber  7,0,0
#define MagickLibAddendum  "-17"
#define MagickLibInterface  7
#define MagickLibMinInterface  7
#define MagickPlatform  "x86_64"
#define MagickppLibVersionText  "6.9.12"
#define MagickppLibVersionNumber  9:0:0
#define MagickppLibAddendum  "-17"
#define MagickppLibInterface  9
#define MagickppLibMinInterface  9
#define MagickReleaseDate  "2021-06-20"
#define MagickHomeURL  "file:///data/mxe/usr/x86_64-w64-mingw32.shared.posix.all/share/doc/ImageMagick-6/index.html"
#define MagickAuthoritativeLicense  \
  "https://imagemagick.org/script/license.php"
#define MagickAuthoritativeURL  "https://imagemagick.org"
#if (MAGICKCORE_QUANTUM_DEPTH == 8)
#define MagickQuantumDepth  "Q8"
#define MagickQuantumRange  "255"
#elif (MAGICKCORE_QUANTUM_DEPTH == 16)
#define MagickQuantumDepth  "Q16"
#define MagickQuantumRange  "65535"
#elif (MAGICKCORE_QUANTUM_DEPTH == 32)
#define MagickQuantumDepth  "Q32"
#define MagickQuantumRange  "4294967295"
#elif (MAGICKCORE_QUANTUM_DEPTH == 64)
#define MagickQuantumDepth  "Q64"
#define MagickQuantumRange  "18446744073709551615.0"
#else
#define MagickQuantumDepth  "Q?"
#define MagickQuantumRange  "?"
#endif
#define MagickVersion  \
  MagickPackageName " " MagickLibVersionText MagickLibAddendum " " \
  MagickQuantumDepth " " MagickPlatform " " MagickReleaseDate " " \
  MagickAuthoritativeURL

extern MagickExport char
  *GetMagickHomeURL(void);

extern MagickExport const char
  *GetMagickCopyright(void) magick_attribute((__const__)),
  *GetMagickDelegates(void) magick_attribute((__const__)),
  *GetMagickFeatures(void) magick_attribute((__const__)),
  *GetMagickLicense(void) magick_attribute((__const__)),
  *GetMagickPackageName(void) magick_attribute((__const__)),
  *GetMagickQuantumDepth(size_t *),
  *GetMagickQuantumRange(size_t *),
  *GetMagickReleaseDate(void) magick_attribute((__const__)),
  *GetMagickVersion(size_t *);

extern MagickExport void
  ListMagickVersion(FILE *);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
