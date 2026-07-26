// Copyright 2021 Bastian Kuolt
#ifndef SHADER_HPP_
#define SHADER_HPP_

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace bgl {

// Forward declaration of asset path helper
[[nodiscard]] std::filesystem::path GetAssetPath(const std::filesystem::path &filename);

class Shader {
public:
  Shader() = default;
  ~Shader() noexcept { cleanup(); }

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;
  Shader(Shader &&other) noexcept : _program(other._program) { other._program = 0; }
  Shader &operator=(Shader &&other) noexcept {
    if (this != &other) {
      cleanup();
      _program = other._program;
      other._program = 0;
    }
    return *this;
  }

  void loadFromFile(const std::filesystem::path &vertex_path, const std::filesystem::path &fragment_path) {
    auto read_file = [](const std::filesystem::path &path) -> std::string {
      std::ifstream file(path);
      if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
      }
      return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    };

    const std::string vert_code = read_file(GetAssetPath(vertex_path));
    const std::string frag_code = read_file(GetAssetPath(fragment_path));
    compile(vert_code, frag_code);
  }

  void compile(std::string_view vertex_source, std::string_view fragment_source) {
    cleanup();

    const GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_source);

    _program = glCreateProgram();
    glAttachShader(_program, vertex_shader);
    glAttachShader(_program, fragment_shader);
    glLinkProgram(_program);

    GLint success = 0;
    glGetProgramiv(_program, GL_LINK_STATUS, &success);
    if (!success) {
      char info_log[512];
      glGetProgramInfoLog(_program, sizeof(info_log), nullptr, info_log);
      spdlog::error("Shader program linking error: {}", info_log);
      glDeleteShader(vertex_shader);
      glDeleteShader(fragment_shader);
      glDeleteProgram(_program);
      _program = 0;
      throw std::runtime_error(std::string("Shader program linking error: ") + info_log);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
  }

  void use() const noexcept {
    if (_program != 0) {
      glUseProgram(_program);
    }
  }

  [[nodiscard]] GLuint getProgram() const noexcept { return _program; }

  void setMat4(std::string_view name, const glm::mat4 &mat) const noexcept {
    const GLint loc = glGetUniformLocation(_program, name.data());
    if (loc != -1) {
      glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
    }
  }

  void setVec3(std::string_view name, const glm::vec3 &vec) const noexcept {
    const GLint loc = glGetUniformLocation(_program, name.data());
    if (loc != -1) {
      glUniform3fv(loc, 1, &vec[0]);
    }
  }

  void setVec4(std::string_view name, const glm::vec4 &vec) const noexcept {
    const GLint loc = glGetUniformLocation(_program, name.data());
    if (loc != -1) {
      glUniform4fv(loc, 1, &vec[0]);
    }
  }

  void setInt(std::string_view name, int value) const noexcept {
    const GLint loc = glGetUniformLocation(_program, name.data());
    if (loc != -1) {
      glUniform1i(loc, value);
    }
  }

  void setBool(std::string_view name, bool value) const noexcept {
    setInt(name, value ? 1 : 0);
  }

private:
  void cleanup() noexcept {
    if (_program != 0) {
      glDeleteProgram(_program);
      _program = 0;
    }
  }

  static GLuint compileShader(GLenum type, std::string_view source) {
    const GLuint shader = glCreateShader(type);
    const char *src_ptr = source.data();
    const GLint src_len = static_cast<GLint>(source.length());

    glShaderSource(shader, 1, &src_ptr, &src_len);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char info_log[512];
      glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
      spdlog::error("Shader compilation error (type {}): {}", type, info_log);
      glDeleteShader(shader);
      throw std::runtime_error(std::string("Shader compilation error: ") + info_log);
    }
    return shader;
  }

private:
  GLuint _program{0};
};

} // namespace bgl

#endif // SHADER_HPP_
