// The one place where stb's code is compiled.
//
// stb libraries are "single-file": the header holds both declarations and the
// implementation. Defining STB_*_IMPLEMENTATION before including turns the
// implementation on. Doing that in exactly one .cpp gives one copy of the code;
// doing it in two would be a "multiple definition" link error.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Only the formats we expect; each one left out is code we don't ship.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP

#include <stb_image.h>
#include <stb_image_write.h>
