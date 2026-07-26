// Copyright 2021 Bastian Kuolt
#ifndef MD2_HPP_
#define MD2_HPP_

#include "FileHeader.hpp"
#include "Shader.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bgl {

// Resolves asset path across workspace/binary directories
[[nodiscard]] std::filesystem::path GetAssetPath(const std::filesystem::path &filename);

// Loads texture using DevIL and returns OpenGL texture ID with maximum anisotropic filtering and mipmapping
[[nodiscard]] GLuint LoadImage(std::string_view name);

class MD2Model {
public:
  struct Keyframe {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::string name;
  };

  explicit MD2Model(const std::filesystem::path &filepath);
  ~MD2Model() noexcept = default;

  MD2Model(const MD2Model &) = delete;
  MD2Model &operator=(const MD2Model &) = delete;
  MD2Model(MD2Model &&) noexcept = default;
  MD2Model &operator=(MD2Model &&) noexcept = default;

  // Load MD2 model file with strict error handling and calculate smooth vertex normals
  void load(const std::filesystem::path &filepath);

  // Start animation sequence by index and FPS rate
  void start(std::size_t animation, std::size_t fps);

  // Update animated frame geometry and interpolate smooth normals
  void update(float delta_time);

  // Normalize model vertices around origin
  void normalize();

  [[nodiscard]] std::size_t getKeyframeCount() const noexcept { return _keyframe_count; }
  [[nodiscard]] std::size_t getVertexCount() const noexcept { return _vertex_count; }
  [[nodiscard]] std::size_t getTriangleCount() const noexcept { return _triangle_count; }

  [[nodiscard]] const std::vector<glm::vec3> &getCurrentVertices() const noexcept { return _current_vertices; }
  [[nodiscard]] const std::vector<glm::vec3> &getCurrentNormals() const noexcept { return _current_normals; }
  [[nodiscard]] const std::vector<std::uint32_t> &getIndices() const noexcept { return _indices; }
  [[nodiscard]] const std::vector<glm::vec2> &getTextureCoords() const noexcept { return _texture_coords; }

private:
  void computeSmoothKeyframeNormals();
  void createAnimationList();
  void interpolate(std::span<glm::vec3> dest_verts, std::span<glm::vec3> dest_norms,
                   std::size_t first, std::size_t second, float factor) const noexcept;
  void createFrame(std::size_t animation, std::size_t frame, float factor);
  void next() noexcept;

private:
  std::vector<Keyframe> _keyframes;
  std::vector<std::uint32_t> _indices;
  std::vector<glm::vec2> _texture_coords;
  std::size_t _keyframe_count{0};
  std::size_t _vertex_count{0};
  std::size_t _triangle_count{0};
  std::vector<anim_t> _animations;

  std::vector<glm::vec3> _current_vertices;
  std::vector<glm::vec3> _current_normals;
  std::size_t _current_frame{0};
  std::size_t _current_animation{0};
  std::size_t _fps{0};
  bool _started{false};
  float _elapsed_frame_time{0.0f};
};

class MD2Renderer {
public:
  explicit MD2Renderer(const MD2Model &model, std::string_view texture_name = "igdosh.jpeg");
  ~MD2Renderer() noexcept;

  MD2Renderer(const MD2Renderer &) = delete;
  MD2Renderer &operator=(const MD2Renderer &) = delete;
  MD2Renderer(MD2Renderer &&) noexcept = default;
  MD2Renderer &operator=(MD2Renderer &&) noexcept = default;

  // Upload updated vertex positions and smooth normals to VBOs
  void updateBuffers(const MD2Model &model);

  // Render model using OpenGL 4.6 VAO/VBO/IBO pipeline
  void render(const Shader &shader, const glm::mat4 &model_matrix) const;

private:
  void setupGLBuffers(const MD2Model &model);

private:
  GLuint _texture{0};
  GLuint _vao{0};
  GLuint _vbo_pos{0};
  GLuint _vbo_tex{0};
  GLuint _vbo_norm{0};
  GLuint _ibo{0};
  std::size_t _index_count{0};
};

} // namespace bgl

#endif // MD2_HPP_
