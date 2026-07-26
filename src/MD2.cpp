// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"

#include <IL/il.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace bgl {

std::filesystem::path GetAssetPath(const std::filesystem::path &filename) {
  const std::array<std::filesystem::path, 7> search_paths = {
      filename,
      std::filesystem::path("src") / filename,
      std::filesystem::path("bin") / filename,
      std::filesystem::path("../bin") / filename,
      std::filesystem::path("build/Debug") / filename,
      std::filesystem::path("build/Debug/shaders") / filename.filename(),
      std::filesystem::path("src/shaders") / filename.filename()
  };

  for (const auto &p : search_paths) {
    if (std::filesystem::exists(p)) {
      return p;
    }
  }
  return filename;
}

GLuint LoadImage(std::string_view name) {
  ilInit();

  const auto path = GetAssetPath(name);
  const std::string path_str = path.string();

  ILuint image_id = 0;
  ilGenImages(1, &image_id);
  ilBindImage(image_id);

  if (!ilLoadImage(path_str.c_str())) {
    ilDeleteImages(1, &image_id);
    spdlog::error("Failed to load image texture file: {}", path_str);
    throw std::runtime_error("Failed to load texture image file: " + path_str);
  }

  ilEnable(IL_CONV_PAL);
  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

  const int width = ilGetInteger(IL_IMAGE_WIDTH);
  const int height = ilGetInteger(IL_IMAGE_HEIGHT);
  const unsigned char *data = ilGetData();

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLfloat max_anisotropy = 1.0f;
#if defined(GL_MAX_TEXTURE_MAX_ANISOTROPY)
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
#elif defined(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
#else
  constexpr GLenum GL_MAX_TEX_ANISOTROPY = 0x84FF;
  constexpr GLenum GL_TEX_ANISOTROPY = 0x84FE;
  glGetFloatv(GL_MAX_TEX_ANISOTROPY, &max_anisotropy);
  glTexParameterf(GL_TEXTURE_2D, GL_TEX_ANISOTROPY, max_anisotropy);
#endif

  glBindTexture(GL_TEXTURE_2D, 0);

  ilDeleteImages(1, &image_id);
  spdlog::info("Loaded texture: {} ({}x{}, Mipmaps: Enabled, Anisotropy: {}x)", path_str, width, height, max_anisotropy);
  return texture_id;
}

MD2Model::MD2Model(const std::filesystem::path &filepath) {
  load(filepath);
  normalize();
}

void MD2Model::load(const std::filesystem::path &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open MD2 model file: " + filepath.string());
  }

  md2_header_t header{};
  file.read(reinterpret_cast<char *>(&header), sizeof(md2_header_t));

  if (header.id != MD2_ID || header.version != MD2_VERSION) {
    throw std::runtime_error("Invalid MD2 file header magic or version: " + filepath.string());
  }

  _keyframe_count = header.num_frames;
  _triangle_count = header.num_tris;
  const std::size_t raw_vertex_count = header.num_xyz;

  // Read raw triangles and UV coordinates
  file.seekg(header.ofs_tris, std::ios::beg);
  std::vector<md2_triangle_t> triangles(_triangle_count);
  file.read(reinterpret_cast<char *>(triangles.data()), _triangle_count * sizeof(md2_triangle_t));

  file.seekg(header.ofs_st, std::ios::beg);
  std::vector<md2_tex_coord_t> raw_st(header.num_st);
  file.read(reinterpret_cast<char *>(raw_st.data()), header.num_st * sizeof(md2_tex_coord_t));

  // Unroll vertices per triangle corner for perfect UV mapping without seams
  _vertex_count = _triangle_count * 3;
  _indices.resize(_vertex_count);
  _texture_coords.resize(_vertex_count);

  std::vector<std::uint16_t> corner_xyz_indices(_vertex_count);

  for (std::size_t t = 0; t < _triangle_count; ++t) {
    for (int c = 0; c < 3; ++c) {
      const std::size_t corner_idx = t * 3 + c;
      const std::uint16_t v_idx = triangles[t].index_xyz[c];
      const std::uint16_t st_idx = triangles[t].index_st[c];

      _indices[corner_idx] = static_cast<std::uint32_t>(corner_idx);
      corner_xyz_indices[corner_idx] = v_idx;

      if (st_idx < header.num_st) {
        _texture_coords[corner_idx] = glm::vec2(
            static_cast<float>(raw_st[st_idx].u) / static_cast<float>(header.skinwidth),
            static_cast<float>(raw_st[st_idx].v) / static_cast<float>(header.skinheight)
        );
      } else {
        _texture_coords[corner_idx] = glm::vec2(0.0f);
      }
    }
  }

  // Read keyframes and map compressed vertices to unrolled triangle corners
  _keyframes.resize(_keyframe_count);
  file.seekg(header.ofs_frames, std::ios::beg);

  for (std::size_t f = 0; f < _keyframe_count; ++f) {
    md2_frame_header_t raw_frame{};
    file.read(reinterpret_cast<char *>(&raw_frame), sizeof(md2_frame_header_t));

    std::vector<md2_vertex_t> raw_verts(raw_vertex_count);
    file.read(reinterpret_cast<char *>(raw_verts.data()), raw_vertex_count * sizeof(md2_vertex_t));

    _keyframes[f].name = std::string(raw_frame.name);
    _keyframes[f].vertices.resize(_vertex_count);

    for (std::size_t corner_idx = 0; corner_idx < _vertex_count; ++corner_idx) {
      const std::uint16_t v_idx = corner_xyz_indices[corner_idx];
      _keyframes[f].vertices[corner_idx] = glm::vec3(
          raw_verts[v_idx].v[0] * raw_frame.scale[0] + raw_frame.translate[0],
          raw_verts[v_idx].v[1] * raw_frame.scale[1] + raw_frame.translate[1],
          raw_verts[v_idx].v[2] * raw_frame.scale[2] + raw_frame.translate[2]
      );
    }
  }

  computeSmoothKeyframeNormals();
  createAnimationList();

  _current_vertices.resize(_vertex_count);
  _current_normals.resize(_vertex_count);

  spdlog::info("Successfully loaded MD2 model: {} (Frames: {}, Unrolled Vertices: {}, Triangles: {})",
               filepath.string(), _keyframe_count, _vertex_count, _triangle_count);
}

void MD2Model::computeSmoothKeyframeNormals() {
  for (auto &keyframe : _keyframes) {
    keyframe.normals.assign(_vertex_count, glm::vec3(0.0f));

    for (std::size_t i = 0; i < _indices.size(); i += 3) {
      const std::uint32_t i0 = _indices[i];
      const std::uint32_t i1 = _indices[i + 1];
      const std::uint32_t i2 = _indices[i + 2];

      const glm::vec3 &v0 = keyframe.vertices[i0];
      const glm::vec3 &v1 = keyframe.vertices[i1];
      const glm::vec3 &v2 = keyframe.vertices[i2];

      // Outer face normal
      const glm::vec3 face_normal = glm::cross(v1 - v0, v2 - v0);
      keyframe.normals[i0] += face_normal;
      keyframe.normals[i1] += face_normal;
      keyframe.normals[i2] += face_normal;
    }

    for (auto &norm : keyframe.normals) {
      const float len = glm::length(norm);
      if (len > 0.0001f) {
        norm /= len;
      } else {
        norm = glm::vec3(0.0f, 1.0f, 0.0f);
      }
    }
  }
}

void MD2Model::normalize() {
  if (_keyframes.empty()) return;

  glm::vec3 min_bounds(1e9f);
  glm::vec3 max_bounds(-1e9f);

  for (const auto &v : _keyframes[0].vertices) {
    min_bounds = glm::min(min_bounds, v);
    max_bounds = glm::max(max_bounds, v);
  }

  const glm::vec3 center = (min_bounds + max_bounds) * 0.5f;

  for (auto &keyframe : _keyframes) {
    for (auto &v : keyframe.vertices) {
      v -= center;
    }
  }
}

void MD2Model::createAnimationList() {
  _animations.clear();
  std::size_t start_frame = 0;
  std::string last_name;

  for (std::size_t i = 0; i < _keyframe_count; ++i) {
    std::string name = _keyframes[i].name;
    const auto pos = name.find_first_of("0123456789");
    if (pos != std::string::npos) {
      name = name.substr(0, pos);
    }

    if (i == 0) {
      last_name = name;
    } else if (name != last_name) {
      _animations.push_back(anim_t{start_frame, i - 1, 10});
      last_name = name;
      start_frame = i;
    }
  }
  _animations.push_back(anim_t{start_frame, _keyframe_count - 1, 10});
}

void MD2Model::start(std::size_t animation, std::size_t fps) {
  if (animation >= _animations.size()) {
    throw std::out_of_range("Invalid MD2 animation index");
  }
  _current_animation = animation;
  _current_frame = _animations[animation].first_frame;
  _fps = fps;
  _started = true;
  _elapsed_frame_time = 0.0f;
  createFrame(_current_animation, _current_frame, 0.0f);
}

void MD2Model::update(float delta_time) {
  if (!_started || _fps == 0) return;

  _elapsed_frame_time += delta_time;
  const float frame_duration = 1.0f / static_cast<float>(_fps);

  while (_elapsed_frame_time >= frame_duration) {
    _elapsed_frame_time -= frame_duration;
    next();
  }

  const float factor = _elapsed_frame_time / frame_duration;
  createFrame(_current_animation, _current_frame, factor);
}

void MD2Model::createFrame(std::size_t animation, std::size_t frame, float factor) {
  const std::size_t next_frame = (frame == _animations[animation].last_frame) ? _animations[animation].first_frame : frame + 1;
  interpolate(_current_vertices, _current_normals, frame, next_frame, factor);
}

void MD2Model::interpolate(std::span<glm::vec3> dest_verts, std::span<glm::vec3> dest_norms,
                            std::size_t first, std::size_t second, float factor) const noexcept {
  const auto &v1 = _keyframes[first].vertices;
  const auto &v2 = _keyframes[second].vertices;
  const auto &n1 = _keyframes[first].normals;
  const auto &n2 = _keyframes[second].normals;

  for (std::size_t i = 0; i < _vertex_count; ++i) {
    dest_verts[i] = glm::mix(v1[i], v2[i], factor);
    dest_norms[i] = glm::normalize(glm::mix(n1[i], n2[i], factor));
  }
}

void MD2Model::next() noexcept {
  if (_current_frame == _animations[_current_animation].last_frame) {
    _current_frame = _animations[_current_animation].first_frame;
  } else {
    ++_current_frame;
  }
}

// -----------------------------------------------------------------------------
// MD2Renderer Implementation
// -----------------------------------------------------------------------------

MD2Renderer::MD2Renderer(const MD2Model &model, std::string_view texture_name) {
  _texture = LoadImage(texture_name);
  setupGLBuffers(model);
}

MD2Renderer::~MD2Renderer() noexcept {
  if (_vao != 0) glDeleteVertexArrays(1, &_vao);
  if (_vbo_pos != 0) glDeleteBuffers(1, &_vbo_pos);
  if (_vbo_tex != 0) glDeleteBuffers(1, &_vbo_tex);
  if (_vbo_norm != 0) glDeleteBuffers(1, &_vbo_norm);
  if (_ibo != 0) glDeleteBuffers(1, &_ibo);
  if (_texture != 0) glDeleteTextures(1, &_texture);
}

void MD2Renderer::setupGLBuffers(const MD2Model &model) {
  const auto &indices = model.getIndices();
  const auto &tex_coords = model.getTextureCoords();
  const auto &vertices = model.getCurrentVertices();
  const auto &normals = model.getCurrentNormals();
  _index_count = indices.size();

  glGenVertexArrays(1, &_vao);
  glGenBuffers(1, &_vbo_pos);
  glGenBuffers(1, &_vbo_tex);
  glGenBuffers(1, &_vbo_norm);
  glGenBuffers(1, &_ibo);

  glBindVertexArray(_vao);

  // Position VBO (location = 0)
  glBindBuffer(GL_ARRAY_BUFFER, _vbo_pos);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);

  // TexCoord VBO (location = 1)
  glBindBuffer(GL_ARRAY_BUFFER, _vbo_tex);
  glBufferData(GL_ARRAY_BUFFER, tex_coords.size() * sizeof(glm::vec2), tex_coords.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void *)0);

  // Normal VBO (location = 2)
  glBindBuffer(GL_ARRAY_BUFFER, _vbo_norm);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);

  // Index Buffer (IBO)
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void MD2Renderer::updateBuffers(const MD2Model &model) {
  const auto &vertices = model.getCurrentVertices();
  const auto &normals = model.getCurrentNormals();

  glBindBuffer(GL_ARRAY_BUFFER, _vbo_pos);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), vertices.data());

  glBindBuffer(GL_ARRAY_BUFFER, _vbo_norm);
  glBufferSubData(GL_ARRAY_BUFFER, 0, normals.size() * sizeof(glm::vec3), normals.data());

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MD2Renderer::render(const Shader &shader, const glm::mat4 &model_matrix) const {
  shader.setMat4("uModel", model_matrix);
  shader.setBool("uUseTexture", _texture != 0);

  if (_texture != 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture);
    shader.setInt("uTexture", 0);
  }

  glBindVertexArray(_vao);
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_index_count), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

} // namespace bgl
