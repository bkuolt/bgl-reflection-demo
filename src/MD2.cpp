// Copyright 2021 Bastian Kuolt
#include "MD2.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
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

GLuint LoadImage(const std::string &name) {
  std::string path_str = GetAssetPath(name).string();

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

  int width = ilGetInteger(IL_IMAGE_WIDTH);
  int height = ilGetInteger(IL_IMAGE_HEIGHT);

  GLuint texture{0};
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gluBuild2DMipmaps(
      GL_TEXTURE_2D,
      GL_RGBA,
      width,
      height,
      GL_RGBA,
      GL_UNSIGNED_BYTE,
      ilGetData()
  );

  glBindTexture(GL_TEXTURE_2D, 0);
  ilDeleteImages(1, &image_id);

  spdlog::info("Loaded texture image: {} ({}x{})", path_str, width, height);
  return texture;
}

void glutBitmapString(void *font, const char *string) {
  for (const char *c = string; *c; ++c) {
    glutBitmapCharacter(font, *c);
  }
}

} // namespace bgl

MD2::MD2(const std::string &filename) {
  load(filename);
  _started = false;
}

void MD2::load(const std::string &filename) {
  std::filesystem::path resolved_path = bgl::GetAssetPath(filename);
  std::ifstream file(resolved_path.string(), std::ifstream::binary);
  if (!file.is_open()) {
    spdlog::error("Failed to open MD2 model file: {}", resolved_path.string());
    throw std::runtime_error("Failed to open MD2 model file: " + resolved_path.string());
  }

  md2_t header;
  file.read(reinterpret_cast<char *>(&header), sizeof(md2_t));
  if (header.id != MD2_ID || header.version != MD2_VERSION) {
    spdlog::error("Invalid MD2 header format in: {}", resolved_path.string());
    throw std::runtime_error("Invalid MD2 header format in: " + resolved_path.string());
  }

  _keyframe_count = header.num_frames;
  _vertex_count = header.num_xyz;
  _triangle_count = header.num_tris;

  _indices.resize(_triangle_count * 3);
  _keyframes.resize(_keyframe_count);

  for (size_t f = 0; f < _keyframe_count; ++f) {
    _keyframes[f].vertices.resize(_vertex_count);
  }
  _texture_coords.resize(_triangle_count * 3);

  // Load keyframe data
  std::vector<frame_t> frames(_keyframe_count);
  for (int i = 0; i < header.num_frames; ++i) {
    file.seekg(header.ofs_frames + (i * header.framesize), std::ios_base::beg);
    file.read(reinterpret_cast<char *>(&frames[i]), 40);
    frames[i].verts = new vertex_t[header.num_xyz];
    file.seekg(header.ofs_frames + (i * header.framesize) + 40, std::ios_base::beg);
    file.read(reinterpret_cast<char *>(frames[i].verts), header.num_xyz * sizeof(vertex_t));
  }

  for (size_t f = 0; f < _keyframe_count; ++f) {
    std::strncpy(_keyframes[f].name, frames[f].name, 16);
    for (size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] = glm::vec3(
          (frames[f].verts[v].v[0] * frames[f].scale[0]) + frames[f].translate[0],
          (frames[f].verts[v].v[1] * frames[f].scale[1]) + frames[f].translate[1],
          (frames[f].verts[v].v[2] * frames[f].scale[2]) + frames[f].translate[2]);
    }
    delete[] frames[f].verts;
  }

  // Load texture coordinates
  std::vector<tex_coord_t> sts(header.num_st);
  file.seekg(header.ofs_st, std::ios_base::beg);
  file.read(reinterpret_cast<char *>(sts.data()), sizeof(tex_coord_t) * header.num_st);

  // Load triangle indices
  std::vector<triangle_t> triangles(header.num_tris);
  file.seekg(header.ofs_tris, std::ios_base::beg);
  file.read(reinterpret_cast<char *>(triangles.data()), sizeof(triangle_t) * header.num_tris);

  for (size_t t = 0; t < header.num_tris; ++t) {
    for (size_t v = 0; v < 3; ++v) {
      _indices[(3 * t) + v] = triangles[t].index_xyz[v];
    }
  }

  for (size_t t = 0; t < header.num_tris; ++t) {
    for (size_t v = 0; v < 3; ++v) {
      int index = triangles[t].index_st[v];
      _texture_coords[(3 * t) + v] = glm::vec2(
          static_cast<float>(sts[index].u) / header.skinwidth,
          static_cast<float>(sts[index].v) / header.skinheight);
    }
  }

  try {
    _texture = bgl::LoadImage("igdosh.bmp");
  } catch (const std::exception &error) {
    spdlog::warn("Exception while loading texture: {}", error.what());
  }

  createAnimationList();
  _current_vertices.resize(_vertex_count);

  file.close();
  spdlog::info("Successfully loaded MD2 model: {} (Frames: {}, Vertices: {}, Triangles: {})",
               resolved_path.string(), _keyframe_count, _vertex_count, _triangle_count);
}

void MD2::render(const std::vector<glm::vec3> &vertices) const {
  glPushMatrix();
  glDisable(GL_BLEND | GL_ALPHA_TEST | GL_CULL_FACE);

  if (_texture != 0) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  }

  glBegin(GL_TRIANGLES);
  for (size_t i = 0; i < _triangle_count * 3; ++i) {
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
  char last_name[16];

  _animations.push_back(anim_t());
  _animations.back().first_frame = 0;
  std::strncpy(last_name, _keyframes[0].name, 16);

  for (size_t i = 1; i < _keyframe_count; ++i) {
    size_t c = 0;
    for (; c < 16 && _keyframes[i].name[c] != '\0' && !std::isdigit(_keyframes[i].name[c]); ++c) {
    }
    std::strncpy(last_name, _keyframes[i].name, 16);
    if (c < 16) last_name[c] = 0;

    if (0 != std::strncmp(last_name, _keyframes[i - 1].name, c)) {
      _animations.back().last_frame = static_cast<int>(i - 1);
      _animations.push_back(anim_t());
      _animations.back().first_frame = static_cast<int>(i);
    }
  }

  _animations.back().last_frame = static_cast<int>(_keyframe_count - 1);
  spdlog::debug("Extracted {} animations from MD2 file", _animations.size());
}

void MD2::interpolate(std::vector<glm::vec3> &dest, size_t first, size_t second, float factor) {
  for (size_t v = 0; v < _vertex_count; ++v) {
    dest[v] = glm::mix(_keyframes[first].vertices[v], _keyframes[second].vertices[v], factor);
  }
}

void MD2::createFrame(size_t animation, size_t frame, float factor) {
  int first = _animations[animation].first_frame + static_cast<int>(frame);
  int second = _animations[animation].first_frame + static_cast<int>(frame) + 1;

  if (first >= _animations[animation].last_frame) {
    second = _animations[animation].first_frame;
  }
  interpolate(_current_vertices, first, second, factor);
}

void MD2::next() noexcept {
  if (_animations[_current_animation].last_frame ==
      _animations[_current_animation].first_frame + static_cast<int>(_current_frame)) {
    _current_frame = 0;
  } else {
    ++_current_frame;
  }
}

void MD2::start(size_t animation, size_t fps) {
  _started = true;
  _fps = fps;
  _current_frame = 0;
  _current_animation = animation;
  spdlog::info("Started MD2 animation index: {} at {} FPS", animation, fps);
}

void MD2::animate() {
  static int time = 0;
  static GLuint list = 0;
  static bool init{false};

  if (!init) {
    list = glGenLists(1);
    init = true;
  }

  if (_started && _fps > 0) {
    int current_time = glutGet(GLUT_ELAPSED_TIME);
    float factor = 0.0f;
    bool create = false;

    int frame_duration = static_cast<int>(1000 / _fps);
    if (current_time > time + frame_duration) {
      next();
      factor = 0.0f;
      create = true;
      time = current_time;
    } else {
      factor = static_cast<float>(current_time - time) / frame_duration;
      if (factor > 1.0f) factor = 1.0f;
      create = true;
    }

    if (create) {
      createFrame(_current_animation, _current_frame, factor);
      glNewList(list, GL_COMPILE);
      render(_current_vertices);
      glEndList();
    }
    glCallList(list);
  }
}

void MD2::normalize() {
  glm::vec3 min_bound(0.0f);
  glm::vec3 max_bound(0.0f);

  for (size_t f = 0; f < _keyframe_count; ++f) {
    for (size_t v = 0; v < _vertex_count; ++v) {
      min_bound = glm::min(min_bound, _keyframes[f].vertices[v]);
      max_bound = glm::max(max_bound, _keyframes[f].vertices[v]);
    }
  }

  glm::vec3 center = min_bound + (max_bound - min_bound) * 0.5f;

  for (size_t f = 0; f < _keyframe_count; ++f) {
    for (size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] -= center;
    }
  }

  max_bound -= center;

  for (size_t f = 0; f < _keyframe_count; ++f) {
    for (size_t v = 0; v < _vertex_count; ++v) {
      _keyframes[f].vertices[v] /= max_bound;
    }
  }

  spdlog::info("Normalized MD2 model vertices around center: ({}, {}, {})", center.x, center.y, center.z);
}
