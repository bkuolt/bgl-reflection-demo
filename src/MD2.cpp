// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"

#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <IL/il.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>

namespace bgl {

std::filesystem::path GetAssetPath(const std::filesystem::path &filename) {
  if (std::filesystem::exists(filename)) {
    return filename;
  }
  auto bin_path = std::filesystem::path("bin") / filename;
  if (std::filesystem::exists(bin_path)) {
    return bin_path;
  }
  return filename;
}

GLuint LoadImage(std::string_view name) {
  const std::string path_str = GetAssetPath(name).string();

  ilInit();

  ILuint image_id{0};
  ilGenImages(1, &image_id);
  ilBindImage(image_id);

  if (!ilLoadImage(path_str.c_str())) {
    spdlog::warn("Could not load image file from path: {}", path_str);
    ilDeleteImages(1, &image_id);
    return 0;
  }

  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

  const int width = ilGetInteger(IL_IMAGE_WIDTH);
  const int height = ilGetInteger(IL_IMAGE_HEIGHT);

  GLuint texture{0};
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, ilGetData());

  glBindTexture(GL_TEXTURE_2D, 0);
  ilDeleteImages(1, &image_id);

  spdlog::info("Loaded texture image: {} ({}x{})", path_str, width, height);
  return texture;
}

void glutBitmapString(void *font, std::string_view string) {
  for (const char c : string) {
    glutBitmapCharacter(font, c);
  }
}

MD2::MD2(const std::filesystem::path &filepath) {
  load(filepath);
  _started = false;
}

MD2::~MD2() noexcept {
  if (_texture != 0) {
    glDeleteTextures(1, &_texture);
    _texture = 0;
  }
}

void MD2::load(const std::filesystem::path &filepath) {
  const std::filesystem::path resolved_path = GetAssetPath(filepath);
  std::ifstream file(resolved_path, std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("Failed to open MD2 model file: {}", resolved_path.string());
    throw std::runtime_error("Failed to open MD2 model file: " +
                             resolved_path.string());
  }

  md2_header_t header;
  file.read(reinterpret_cast<char *>(&header), sizeof(md2_header_t));
  if (header.id != MD2_ID || header.version != MD2_VERSION) {
    spdlog::error("Invalid MD2 header format in: {}", resolved_path.string());
    throw std::runtime_error("Invalid MD2 header format in: " +
                             resolved_path.string());
  }

  _keyframe_count = static_cast<std::size_t>(header.num_frames);
  _vertex_count = static_cast<std::size_t>(header.num_xyz);
  _triangle_count = static_cast<std::size_t>(header.num_tris);

  _indices.resize(_triangle_count * 3);
  _keyframes.resize(_keyframe_count);

  for (auto &keyframe : _keyframes) {
    keyframe.vertices.resize(_vertex_count);
  }
  _texture_coords.resize(_triangle_count * 3);

  // Load keyframe vertex data using RAII buffer
  std::vector<md2_vertex_t> vertex_buffer(_vertex_count);
  md2_frame_header_t frame_header;

  for (std::size_t f = 0; f < _keyframe_count; ++f) {
    const std::streamoff frame_offset =
        header.ofs_frames + (static_cast<std::streamoff>(f) * header.framesize);
    file.seekg(frame_offset, std::ios::beg);
    file.read(reinterpret_cast<char *>(&frame_header),
              sizeof(md2_frame_header_t));
    file.read(reinterpret_cast<char *>(vertex_buffer.data()),
              _vertex_count * sizeof(md2_vertex_t));

    _keyframes[f].name =
        std::string(frame_header.name, strnlen(frame_header.name, 16));

    for (std::size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] =
          glm::vec3((vertex_buffer[v].v[0] * frame_header.scale[0]) +
                        frame_header.translate[0],
                    (vertex_buffer[v].v[1] * frame_header.scale[1]) +
                        frame_header.translate[1],
                    (vertex_buffer[v].v[2] * frame_header.scale[2]) +
                        frame_header.translate[2]);
    }
  }

  // Load texture coordinates
  std::vector<md2_tex_coord_t> sts(static_cast<std::size_t>(header.num_st));
  file.seekg(header.ofs_st, std::ios::beg);
  file.read(reinterpret_cast<char *>(sts.data()),
            sizeof(md2_tex_coord_t) * header.num_st);

  // Load triangle indices
  std::vector<md2_triangle_t> triangles(_triangle_count);
  file.seekg(header.ofs_tris, std::ios::beg);
  file.read(reinterpret_cast<char *>(triangles.data()),
            sizeof(md2_triangle_t) * _triangle_count);

  for (std::size_t t = 0; t < _triangle_count; ++t) {
    for (std::size_t v = 0; v < 3; ++v) {
      _indices[(3 * t) + v] = triangles[t].index_xyz[v];
    }
  }

  for (std::size_t t = 0; t < _triangle_count; ++t) {
    for (std::size_t v = 0; v < 3; ++v) {
      const std::size_t index = triangles[t].index_st[v];
      _texture_coords[(3 * t) + v] =
          glm::vec2(static_cast<float>(sts[index].u) /
                        static_cast<float>(header.skinwidth),
                    static_cast<float>(sts[index].v) /
                        static_cast<float>(header.skinheight));
    }
  }

  try {
    _texture = LoadImage("igdosh.jpeg");
  } catch (const std::exception &error) {
    spdlog::warn("Exception while loading texture: {}", error.what());
  }

  createAnimationList();
  _current_vertices.resize(_vertex_count);

  file.close();
  spdlog::info("Successfully loaded MD2 model: {} (Frames: {}, Vertices: {}, "
               "Triangles: {})",
               resolved_path.string(), _keyframe_count, _vertex_count,
               _triangle_count);
}

void MD2::render(std::span<const glm::vec3> vertices) const {
  glPushMatrix();
  glDisable(GL_BLEND | GL_ALPHA_TEST | GL_CULL_FACE);

  if (_texture != 0) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  }

  glBegin(GL_TRIANGLES);
  const std::size_t vertex_indices_count = _triangle_count * 3;
  for (std::size_t i = 0; i < vertex_indices_count; ++i) {
    glTexCoord2fv(glm::value_ptr(_texture_coords[i]));
    glVertex3fv(glm::value_ptr(vertices[_indices[i]]));
  }
  glEnd();

  if (_texture != 0) {
    glDisable(GL_TEXTURE_2D);
  }
  glPopMatrix();
}

void MD2::createAnimationList() {
  _animations.clear();
  if (_keyframes.empty())
    return;

  _animations.push_back(anim_t{});
  _animations.back().first_frame = 0;

  auto get_base_name = [](std::string_view name) -> std::string_view {
    std::size_t pos = 0;
    while (pos < name.length() &&
           !std::isdigit(static_cast<unsigned char>(name[pos]))) {
      ++pos;
    }
    return name.substr(0, pos);
  };

  std::string_view current_base = get_base_name(_keyframes[0].name);

  for (std::size_t i = 1; i < _keyframe_count; ++i) {
    std::string_view next_base = get_base_name(_keyframes[i].name);
    if (current_base != next_base) {
      _animations.back().last_frame = i - 1;
      _animations.push_back(anim_t{});
      _animations.back().first_frame = i;
      current_base = next_base;
    }
  }

  _animations.back().last_frame = _keyframe_count - 1;
  spdlog::debug("Extracted {} animations from MD2 file", _animations.size());
}

void MD2::interpolate(std::span<glm::vec3> dest, std::size_t first,
                      std::size_t second, float factor) const noexcept {
  const auto &v1 = _keyframes[first].vertices;
  const auto &v2 = _keyframes[second].vertices;
  for (std::size_t v = 0; v < _vertex_count; ++v) {
    dest[v] = glm::mix(v1[v], v2[v], factor);
  }
}

void MD2::createFrame(std::size_t animation, std::size_t frame, float factor) {
  std::size_t first = _animations[animation].first_frame + frame;
  std::size_t second = _animations[animation].first_frame + frame + 1;

  if (first >= _animations[animation].last_frame) {
    second = _animations[animation].first_frame;
  }
  interpolate(_current_vertices, first, second, factor);
}

void MD2::next() noexcept {
  if (_animations[_current_animation].last_frame ==
      _animations[_current_animation].first_frame + _current_frame) {
    _current_frame = 0;
  } else {
    ++_current_frame;
  }
}

void MD2::start(std::size_t animation, std::size_t fps) {
  _started = true;
  _fps = fps;
  _current_frame = 0;
  _current_animation = (animation < _animations.size()) ? animation : 0;
  spdlog::info("Started MD2 animation index: {} at {} FPS", _current_animation,
               fps);
}

void MD2::animate() {
  static int last_time = 0;

  if (_started && _fps > 0) {
    const int current_time = glutGet(GLUT_ELAPSED_TIME);
    const int frame_duration = static_cast<int>(1000 / _fps);
    float factor = 0.0f;

    if (current_time > last_time + frame_duration) {
      next();
      factor = 0.0f;
      last_time = current_time;
    } else {
      factor = std::clamp(static_cast<float>(current_time - last_time) /
                              static_cast<float>(frame_duration),
                          0.0f, 1.0f);
    }

    createFrame(_current_animation, _current_frame, factor);
    render(_current_vertices);
  }
}

void MD2::normalize() {
  if (_keyframes.empty() || _vertex_count == 0)
    return;

  glm::vec3 min_bound(_keyframes[0].vertices[0]);
  glm::vec3 max_bound(_keyframes[0].vertices[0]);

  for (std::size_t f = 0; f < _keyframe_count; ++f) {
    for (std::size_t v = 0; v < _vertex_count; ++v) {
      min_bound = glm::min(min_bound, _keyframes[f].vertices[v]);
      max_bound = glm::max(max_bound, _keyframes[f].vertices[v]);
    }
  }

  const glm::vec3 center = min_bound + (max_bound - min_bound) * 0.5f;

  for (std::size_t f = 0; f < _keyframe_count; ++f) {
    for (std::size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] -= center;
    }
  }

  const glm::vec3 half_extent = (max_bound - min_bound) * 0.5f;

  for (std::size_t f = 0; f < _keyframe_count; ++f) {
    for (std::size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] /= half_extent;
    }
  }

  spdlog::info("Normalized MD2 model vertices around center: ({}, {}, {})",
               center.x, center.y, center.z);
}

} // namespace bgl
