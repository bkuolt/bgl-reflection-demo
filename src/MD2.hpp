// Copyright 2021 Bastian Kuolt
#ifndef MD2_HPP_
#define MD2_HPP_

#include "FileHeader.hpp"

#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <IL/il.h>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace bgl {

// Resolves asset path across workspace/binary directories
[[nodiscard]] std::filesystem::path GetAssetPath(const std::filesystem::path &filename);

// Loads texture using DevIL and returns OpenGL texture ID
[[nodiscard]] GLuint LoadImage(const std::string &name);

// Renders bitmap font string using FreeGLUT
void glutBitmapString(void *font, const char *string);

} // namespace bgl

class MD2 {
public:
  struct Keyframe {
    std::vector<glm::vec3> vertices;
    char name[16]{0};
  };

  explicit MD2(const std::string &filename);
  ~MD2() = default;

  // Load MD2 model file
  void load(const std::string &filename);

  // Start playing animation
  void start(std::size_t animation, std::size_t fps);

  // Update and render model animation frame
  void animate();

  // Normalize model vertices to [-1, 1] range
  void normalize();

private:
  void render(const std::vector<glm::vec3> &vertices) const;
  void createAnimationList();
  void interpolate(std::vector<glm::vec3> &dest, std::size_t first, std::size_t second, float factor);
  void createFrame(std::size_t animation, std::size_t frame, float factor);
  void next() noexcept;

private:
  std::vector<Keyframe> _keyframes;
  std::vector<unsigned int> _indices;
  std::vector<glm::vec2> _texture_coords;
  std::size_t _keyframe_count{0};
  std::size_t _vertex_count{0};
  std::size_t _triangle_count{0};
  GLuint _texture{0};
  std::vector<anim_t> _animations;

  std::vector<glm::vec3> _current_vertices;
  std::size_t _current_frame{0};
  std::size_t _current_animation{0};
  std::size_t _fps{0};
  bool _started{false};
};

#endif // MD2_HPP_
