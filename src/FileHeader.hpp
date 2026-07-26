// Copyright 2021 Bastian Kuolt
#ifndef FILE_HEADER_HPP_
#define FILE_HEADER_HPP_

#include <cstddef>
#include <cstdint>

namespace bgl {

// Magic number "IDP2" (0x32504449)
constexpr std::int32_t MD2_ID = ('2' << 24) | ('P' << 16) | ('D' << 8) | 'I';
constexpr std::int32_t MD2_VERSION = 8;
constexpr std::size_t MAX_MD2_VERTS = 2048;

#pragma pack(push, 1)

// MD2 File Header layout (68 bytes)
struct md2_header_t {
  std::int32_t id{0};         // Magic number ("IDP2")
  std::int32_t version{0};    // MD2 version (must be 8)
  std::int32_t skinwidth{0};  // Texture width
  std::int32_t skinheight{0}; // Texture height
  std::int32_t framesize{0};  // Frame size in bytes
  std::int32_t num_skins{0};  // Number of skins
  std::int32_t num_xyz{0};    // Number of vertices per frame
  std::int32_t num_st{0};     // Number of texture coordinates
  std::int32_t num_tris{0};   // Number of triangles
  std::int32_t num_glcmds{0}; // Number of OpenGL commands
  std::int32_t num_frames{0}; // Total frame count
  std::int32_t ofs_skins{0};  // Offset to skin names
  std::int32_t ofs_st{0};     // Offset to texture coordinates
  std::int32_t ofs_tris{0};   // Offset to triangle indices
  std::int32_t ofs_frames{0}; // Offset to frame data
  std::int32_t ofs_glcmds{0}; // Offset to OpenGL commands
  std::int32_t ofs_end{0};    // Offset to end of file
};

// Texture coordinate in MD2 file
struct md2_tex_coord_t {
  std::int16_t u{0};
  std::int16_t v{0};
};

// Triangle indices in MD2 file (12 bytes)
struct md2_triangle_t {
  std::uint16_t index_xyz[3]{0, 0, 0}; // Vertex indices
  std::uint16_t index_st[3]{0, 0, 0};  // Texture coordinate indices
};

// Compressed vertex in MD2 file (4 bytes)
struct md2_vertex_t {
  std::uint8_t v[3]{0, 0, 0};       // Compressed (x, y, z) coordinates
  std::uint8_t lightnormalindex{0}; // Pre-calculated lighting normal index
};

// Raw frame header in MD2 file (40 bytes)
struct md2_frame_header_t {
  float scale[3]{0.0f, 0.0f, 0.0f};     // Scaling factors
  float translate[3]{0.0f, 0.0f, 0.0f}; // Translation factors
  char name[16]{0};                     // Frame name
};

#pragma pack(pop)

static_assert(sizeof(md2_header_t) == 68, "MD2 header size must be 68 bytes");
static_assert(sizeof(md2_tex_coord_t) == 4, "MD2 texture coordinate size must be 4 bytes");
static_assert(sizeof(md2_triangle_t) == 12, "MD2 triangle size must be 12 bytes");
static_assert(sizeof(md2_vertex_t) == 4, "MD2 vertex size must be 4 bytes");
static_assert(sizeof(md2_frame_header_t) == 40, "MD2 frame header size must be 40 bytes");

// MD2 animation sequence tracking
struct anim_t {
  std::size_t first_frame{0};
  std::size_t last_frame{0};
  std::size_t fps{10};
};

} // namespace bgl

#endif // FILE_HEADER_HPP_
