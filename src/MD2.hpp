// Copyright 2021 Bastian Kuolt
#ifndef MD2_HPP_
#define MD2_HPP_

#include "FileHeader.hpp"

#include <GL/gl.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bgl {

// Resolves asset path across workspace/binary directories
[[nodiscard]] std::filesystem::path GetAssetPath(const std::filesystem::path &filename);

// Loads texture using DevIL and returns OpenGL texture ID
[[nodiscard]] GLuint LoadImage(std::string_view name);

// Renders bitmap font string using FreeGLUT
void glutBitmapString(void *font, std::string_view string);

class MD2 {
public:
  struct Keyframe {
    std::vector<glm::vec3> vertices;
    std::string name;
  };

  explicit MD2(const std::filesystem::path &filepath);
  ~MD2() noexcept;

  MD2(const MD2 &) = delete;
  MD2 &operator=(const MD2 &) = delete;
  MD2(MD2 &&) noexcept = default;
  MD2 &operator=(MD2 &&) noexcept = default;

  // Load MD2 model file from filesystem
  void load(const std::filesystem::path &filepath);

  // Start playing animation sequence by index and FPS rate
  void start(std::size_t animation, std::size_t fps);

  // Update and render model animation frame
  void animate();

  // Normalize model vertices to [-1, 1] range around origin
  void normalize();

  [[nodiscard]] std::size_t getKeyframeCount() const noexcept { return _keyframe_count; }
  [[nodiscard]] std::size_t getVertexCount() const noexcept { return _vertex_count; }
  [[nodiscard]] std::size_t getTriangleCount() const noexcept { return _triangle_count; }

private:
  void render(std::span<const glm::vec3> vertices) const;
  void createAnimationList();
  void interpolate(std::span<glm::vec3> dest, std::size_t first, std::size_t second, float factor) const noexcept;
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

} // namespace bgl

#endif // MD2_HPP_
