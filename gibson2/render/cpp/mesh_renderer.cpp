#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fstream>

#ifdef USE_GLAD

#include  <glad/egl.h>

#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include  <glad/gl.h>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include "mesh_renderer.h"
#define MAX_ARRAY_SIZE 512
#define BUFFER_OFFSET(offset) (static_cast<char*>(0) + (offset))

namespace py = pybind11;


class Image {
public:
    static std::shared_ptr<Image> fromFile(const std::string &filename, int channels) {
        std::printf("Loading image: %s\n", filename.c_str());

        std::shared_ptr<Image> image{new Image};

        if (stbi_is_hdr(filename.c_str())) {
            float *pixels = stbi_loadf(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels,
                                       channels);
            if (pixels) {
                image->m_pixels.reset(reinterpret_cast<unsigned char *>(pixels));
                image->m_hdr = true;
            }
        } else {
            unsigned char *pixels = stbi_load(filename.c_str(), &image->m_width, &image->m_height, &image->m_channels,
                                              channels);
            if (pixels) {
                image->m_pixels.reset(pixels);
                image->m_hdr = false;
            }
        }
        if (channels > 0) {
            image->m_channels = channels;
        }

        if (!image->m_pixels) {
            throw std::runtime_error("Failed to load image file: " + filename);
        }
        return image;
    }


    int width() const { return m_width; }

    int height() const { return m_height; }

    int channels() const { return m_channels; }

    int bytesPerPixel() const { return m_channels * (m_hdr ? sizeof(float) : sizeof(unsigned char)); }

    int pitch() const { return m_width * bytesPerPixel(); }

    bool isHDR() const { return m_hdr; }

    template<typename T>
    const T *pixels() const {
        assert(m_channels * sizeof(T) == bytesPerPixel());
        return reinterpret_cast<const T *>(m_pixels.get());
    }

private:
    Image()
            : m_width(0), m_height(0), m_channels(0), m_hdr(false) {}

    int m_width;
    int m_height;
    int m_channels;
    bool m_hdr;
    std::unique_ptr<unsigned char> m_pixels;
};


#ifdef USE_CUDA
void MeshRendererContext::map_tensor(GLuint tid, int width, int height, std::size_t data)
{
   cudaError_t err;
   if (cuda_res[tid] == NULL)
   {
     err = cudaGraphicsGLRegisterImage(&(cuda_res[tid]), tid, GL_TEXTURE_2D, cudaGraphicsMapFlagsNone);
     if( err != cudaSuccess)
     {
       std::cout << "WARN: cudaGraphicsGLRegisterImage failed: " << err << std::endl;
     }
   }

   err = cudaGraphicsMapResources(1, &(cuda_res[tid]));
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsMapResources failed: " << err << std::endl;
   }

   cudaArray* array;
   err = cudaGraphicsSubResourceGetMappedArray(&array, cuda_res[tid], 0, 0);
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsSubResourceGetMappedArray failed: " << err << std::endl;
   }

   // copy data
   err = cudaMemcpy2DFromArray((void*)data, width*4*sizeof(char), array, 0, 0, width*4*sizeof(char), height, cudaMemcpyDeviceToDevice);
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaMemcpy2DFromArray failed: " << err << std::endl;
   }

   err = cudaGraphicsUnmapResources(1, &(cuda_res[tid]));
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsUnmapResources failed: " << err << std::endl;
   }
}

void MeshRendererContext::map_tensor_float(GLuint tid, int width, int height, std::size_t data)
{
   cudaError_t err;
   if (cuda_res[tid] == NULL)
   {
     err = cudaGraphicsGLRegisterImage(&(cuda_res[tid]), tid, GL_TEXTURE_2D, cudaGraphicsMapFlagsNone);
     if( err != cudaSuccess)
     {
       std::cout << "WARN: cudaGraphicsGLRegisterImage failed: " << err << std::endl;
     }
   }

   err = cudaGraphicsMapResources(1, &(cuda_res[tid]));
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsMapResources failed: " << err << std::endl;
   }

   cudaArray* array;
   err = cudaGraphicsSubResourceGetMappedArray(&array, cuda_res[tid], 0, 0);
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsSubResourceGetMappedArray failed: " << err << std::endl;
   }

   // copy data
   err = cudaMemcpy2DFromArray((void*)data, width*4*sizeof(float), array, 0, 0, width*4*sizeof(float), height, cudaMemcpyDeviceToDevice);
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaMemcpy2DFromArray failed: " << err << std::endl;
   }

   err = cudaGraphicsUnmapResources(1, &(cuda_res[tid]));
   if( err != cudaSuccess)
   {
     std::cout << "WARN: cudaGraphicsUnmapResources failed: " << err << std::endl;
   }
}
#endif

void MeshRendererContext::render_meshrenderer_pre(bool msaa, GLuint fb1, GLuint fb2) {

    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (msaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb1);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glEnable(GL_DEPTH_TEST);
}

void MeshRendererContext::render_meshrenderer_post() {
    glDisable(GL_DEPTH_TEST);
}

std::string MeshRendererContext::getstring_meshrenderer() {
    return reinterpret_cast<char const *>(glGetString(GL_VERSION));
}

void MeshRendererContext::blit_buffer(int width, int height, GLuint fb1, GLuint fb2) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb2);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    for (int i = 0; i < 4; i++) {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

py::array_t<float> MeshRendererContext::readbuffer_meshrenderer(char *mode, int width, int height, GLuint fb2) {
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    if (!strcmp(mode, "rgb")) {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else if (!strcmp(mode, "normal")) {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
    } else if (!strcmp(mode, "seg")) {
        glReadBuffer(GL_COLOR_ATTACHMENT2);
    } else if (!strcmp(mode, "3d")) {
        glReadBuffer(GL_COLOR_ATTACHMENT3);
    } else {
        fprintf(stderr, "ERROR: Unknown buffer mode.\n");
        exit(EXIT_FAILURE);
    }
    py::array_t<float> data = py::array_t<float>(4 * width * height);
    py::buffer_info buf = data.request();
    float *ptr = (float *) buf.ptr;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, ptr);
    return data;
}

void MeshRendererContext::clean_meshrenderer(std::vector<GLuint> texture1, std::vector<GLuint> texture2,
                                             std::vector<GLuint> fbo, std::vector<GLuint> vaos,
                                             std::vector<GLuint> vbos) {
    glDeleteTextures(texture1.size(), texture1.data());
    glDeleteTextures(texture2.size(), texture2.data());
    glDeleteFramebuffers(fbo.size(), fbo.data());
    glDeleteBuffers(vaos.size(), vaos.data());
    glDeleteBuffers(vbos.size(), vbos.data());
}

py::list MeshRendererContext::setup_framebuffer_meshrenderer(int width, int height) {
    GLuint *fbo_ptr = (GLuint *) malloc(sizeof(GLuint));
    GLuint *texture_ptr = (GLuint *) malloc(5 * sizeof(GLuint));
    glGenFramebuffers(1, fbo_ptr);
    glGenTextures(5, texture_ptr);
    int fbo = fbo_ptr[0];
    int color_tex_rgb = texture_ptr[0];
    int color_tex_normal = texture_ptr[1];
    int color_tex_semantics = texture_ptr[2];
    int color_tex_3d = texture_ptr[3];
    int depth_tex = texture_ptr[4];
    glBindTexture(GL_TEXTURE_2D, color_tex_rgb);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, color_tex_normal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, color_tex_semantics);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, color_tex_3d);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_rgb, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, color_tex_normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, color_tex_semantics, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, color_tex_3d, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
    glViewport(0, 0, width, height);
    GLenum *bufs = (GLenum *) malloc(4 * sizeof(GLenum));
    bufs[0] = GL_COLOR_ATTACHMENT0;
    bufs[1] = GL_COLOR_ATTACHMENT1;
    bufs[2] = GL_COLOR_ATTACHMENT2;
    bufs[3] = GL_COLOR_ATTACHMENT3;
    glDrawBuffers(4, bufs);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    py::list result;
    result.append(fbo);
    result.append(color_tex_rgb);
    result.append(color_tex_normal);
    result.append(color_tex_semantics);
    result.append(color_tex_3d);
    result.append(depth_tex);
    return result;
}

py::list MeshRendererContext::setup_framebuffer_meshrenderer_ms(int width, int height) {
    GLuint *fbo_ptr = (GLuint *) malloc(sizeof(GLuint));
    GLuint *texture_ptr = (GLuint *) malloc(5 * sizeof(GLuint));
    glGenFramebuffers(1, fbo_ptr);
    glGenTextures(5, texture_ptr);
    int fbo = fbo_ptr[0];
    int color_tex_rgb = texture_ptr[0];
    int color_tex_normal = texture_ptr[1];
    int color_tex_semantics = texture_ptr[2];
    int color_tex_3d = texture_ptr[3];
    int depth_tex = texture_ptr[4];
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, color_tex_rgb);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, color_tex_normal);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, color_tex_semantics);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, color_tex_3d);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA32F, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depth_tex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH24_STENCIL8, width, height, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, color_tex_rgb, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, color_tex_normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, color_tex_semantics, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D_MULTISAMPLE, color_tex_3d, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
    glViewport(0, 0, width, height);
    GLenum *bufs = (GLenum *) malloc(4 * sizeof(GLenum));
    bufs[0] = GL_COLOR_ATTACHMENT0;
    bufs[1] = GL_COLOR_ATTACHMENT1;
    bufs[2] = GL_COLOR_ATTACHMENT2;
    bufs[3] = GL_COLOR_ATTACHMENT3;
    glDrawBuffers(4, bufs);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    py::list result;
    result.append(fbo);
    result.append(color_tex_rgb);
    result.append(color_tex_normal);
    result.append(color_tex_semantics);
    result.append(color_tex_3d);
    result.append(depth_tex);
    return result;
}

int MeshRendererContext::compile_shader_meshrenderer(char *vertexShaderSource, char *fragmentShaderSource) {
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

py::list MeshRendererContext::load_object_meshrenderer(int shaderProgram, py::array_t<float> vertexData) {
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    py::buffer_info buf = vertexData.request();
    float *ptr = (float *) buf.ptr;
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), ptr, GL_STATIC_DRAW);
    GLuint positionAttrib = glGetAttribLocation(shaderProgram, "position");
    GLuint normalAttrib = glGetAttribLocation(shaderProgram, "normal");
    GLuint coordsAttrib = glGetAttribLocation(shaderProgram, "texCoords");
    GLuint tangentlAttrib = glGetAttribLocation(shaderProgram, "tangent");
    GLuint bitangentAttrib = glGetAttribLocation(shaderProgram, "bitangent");

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 0);
    glVertexAttribPointer(normalAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 12);
    glVertexAttribPointer(coordsAttrib, 2, GL_FLOAT, GL_TRUE, 56, (void *) 24);
    glVertexAttribPointer(tangentlAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 32);
    glVertexAttribPointer(bitangentAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 44);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    py::list result;
    result.append(VAO);
    result.append(VBO);
    return result;
}

void MeshRendererContext::render_softbody_instance(int vao, int vbo, py::array_t<float> vertexData) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    py::buffer_info buf = vertexData.request();
    float *ptr = (float *) buf.ptr;
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), ptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MeshRendererContext::initvar_instance(int shaderProgram, py::array_t<float> V, py::array_t<float> lightV,
                                           int shadow_pass, py::array_t<float> P, py::array_t<float> lightP,
                                           py::array_t<float> eye_pos,
                                           py::array_t<float> pose_trans,
                                           py::array_t<float> pose_rot, py::array_t<float> lightpos,
                                           py::array_t<float> lightcolor) {
    glUseProgram(shaderProgram);
    float *Vptr = (float *) V.request().ptr;
    float *lightVptr = (float *) lightV.request().ptr;
    float *Pptr = (float *) P.request().ptr;
    float *lightPptr = (float *) lightP.request().ptr;
    float *transptr = (float *) pose_trans.request().ptr;
    float *rotptr = (float *) pose_rot.request().ptr;
    float *lightposptr = (float *) lightpos.request().ptr;
    float *lightcolorptr = (float *) lightcolor.request().ptr;
    float *eye_pos_ptr = (float *) eye_pos.request().ptr;

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_TRUE, Vptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightV"), 1, GL_TRUE, lightVptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, Pptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightP"), 1, GL_FALSE, lightPptr);
    glUniform3f(glGetUniformLocation(shaderProgram, "eyePosition"), eye_pos_ptr[0], eye_pos_ptr[1], eye_pos_ptr[2]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "pose_trans"), 1, GL_FALSE, transptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "pose_rot"), 1, GL_TRUE, rotptr);
    glUniform3f(glGetUniformLocation(shaderProgram, "light_position"), lightposptr[0], lightposptr[1], lightposptr[2]);
    glUniform3f(glGetUniformLocation(shaderProgram, "light_color"), lightcolorptr[0], lightcolorptr[1],
                lightcolorptr[2]);
    glUniform1i(glGetUniformLocation(shaderProgram, "shadow_pass"), shadow_pass);

}


void
MeshRendererContext::init_material_instance(int shaderProgram, float instance_color, py::array_t<float> diffuse_color,
                                            float use_texture, float use_pbr, float use_pbr_mapping, float metallic,
                                            float roughness) {
    float *diffuse_ptr = (float *) diffuse_color.request().ptr;
    glUniform3f(glGetUniformLocation(shaderProgram, "instance_color"), instance_color, 0, 0);
    glUniform3f(glGetUniformLocation(shaderProgram, "diffuse_color"), diffuse_ptr[0], diffuse_ptr[1], diffuse_ptr[2]);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_texture"), use_texture);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_pbr"), use_pbr);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_pbr_mapping"), use_pbr_mapping);
    glUniform1f(glGetUniformLocation(shaderProgram, "metallic"), metallic);
    glUniform1f(glGetUniformLocation(shaderProgram, "roughness"), roughness);
    glUniform1i(glGetUniformLocation(shaderProgram, "texUnit"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "specularTexture"), 1);
    glUniform1i(glGetUniformLocation(shaderProgram, "irradianceTexture"), 2);
    glUniform1i(glGetUniformLocation(shaderProgram, "specularBRDF_LUT"), 3);
    glUniform1i(glGetUniformLocation(shaderProgram, "metallicTexture"), 4);
    glUniform1i(glGetUniformLocation(shaderProgram, "roughnessTexture"), 5);
    glUniform1i(glGetUniformLocation(shaderProgram, "normalTexture"), 6);
    glUniform1i(glGetUniformLocation(shaderProgram, "depthMap"), 7);
}

void MeshRendererContext::draw_elements_instance(bool flag, int texture_id, int metallic_texture_id,
                                                 int roughness_texture_id,
                                                 int normal_texture_id, int depth_texture_id, int vao, int face_size,
                                                 py::array_t<unsigned int> faces, GLuint fb) {
    glActiveTexture(GL_TEXTURE0);
    if (flag) glBindTexture(GL_TEXTURE_2D, texture_id);

    glActiveTexture(GL_TEXTURE1);
    if (flag) glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTexture.id);

    glActiveTexture(GL_TEXTURE2);
    if (flag) glBindTexture(GL_TEXTURE_CUBE_MAP, m_irmapTexture.id);

    glActiveTexture(GL_TEXTURE3);
    if (flag) glBindTexture(GL_TEXTURE_2D, m_spBRDF_LUT.id);

    if (metallic_texture_id != -1) {
        glActiveTexture(GL_TEXTURE4);
        if (flag) glBindTexture(GL_TEXTURE_2D, metallic_texture_id);
    }

    if (roughness_texture_id != -1) {
        glActiveTexture(GL_TEXTURE5);
        if (flag) glBindTexture(GL_TEXTURE_2D, roughness_texture_id);
    }

    if (normal_texture_id != -1) {
        glActiveTexture(GL_TEXTURE6);
        if (flag) glBindTexture(GL_TEXTURE_2D, normal_texture_id);
    }

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, depth_texture_id);

    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    unsigned int *ptr = (unsigned int *) faces.request().ptr;

    GLuint elementBuffer;
    glGenBuffers(1, &elementBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_size * sizeof(unsigned int), &ptr[0], GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, face_size, GL_UNSIGNED_INT, (void *) 0);
    glDeleteBuffers(1, &elementBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MeshRendererContext::initvar_instance_group(int shaderProgram, py::array_t<float> V, py::array_t<float> lightV,
                                                 int shadow_pass, py::array_t<float> P, py::array_t<float> lightP,
                                                 py::array_t<float> eye_pos,
                                                 py::array_t<float> lightpos, py::array_t<float> lightcolor) {
    glUseProgram(shaderProgram);
    float *Vptr = (float *) V.request().ptr;
    float *lightVptr = (float *) lightV.request().ptr;
    float *Pptr = (float *) P.request().ptr;
    float *lightPptr = (float *) lightP.request().ptr;
    float *lightposptr = (float *) lightpos.request().ptr;
    float *lightcolorptr = (float *) lightcolor.request().ptr;
    float *eye_pos_ptr = (float *) eye_pos.request().ptr;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_TRUE, Vptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightV"), 1, GL_TRUE, lightVptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, Pptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightP"), 1, GL_FALSE, lightPptr);
    glUniform3f(glGetUniformLocation(shaderProgram, "eyePosition"), eye_pos_ptr[0], eye_pos_ptr[1], eye_pos_ptr[2]);
    glUniform3f(glGetUniformLocation(shaderProgram, "light_position"), lightposptr[0], lightposptr[1], lightposptr[2]);
    glUniform3f(glGetUniformLocation(shaderProgram, "light_color"), lightcolorptr[0], lightcolorptr[1],
                lightcolorptr[2]);
    glUniform1i(glGetUniformLocation(shaderProgram, "shadow_pass"), shadow_pass);
}

void MeshRendererContext::init_material_pos_instance(int shaderProgram, py::array_t<float> pose_trans,
                                                     py::array_t<float> pose_rot,
                                                     float instance_color, py::array_t<float> diffuse_color,
                                                     float use_texture, float use_pbr, float use_pbr_mapping,
                                                     float metalness,
                                                     float roughness) {
    float *transptr = (float *) pose_trans.request().ptr;
    float *rotptr = (float *) pose_rot.request().ptr;
    float *diffuse_ptr = (float *) diffuse_color.request().ptr;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "pose_trans"), 1, GL_FALSE, transptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "pose_rot"), 1, GL_TRUE, rotptr);
    glUniform3f(glGetUniformLocation(shaderProgram, "instance_color"), instance_color, 0, 0);
    glUniform3f(glGetUniformLocation(shaderProgram, "diffuse_color"), diffuse_ptr[0], diffuse_ptr[1], diffuse_ptr[2]);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_texture"), use_texture);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_pbr"), use_pbr);
    glUniform1f(glGetUniformLocation(shaderProgram, "use_pbr_mapping"), use_pbr_mapping);
    glUniform1f(glGetUniformLocation(shaderProgram, "metalness"), metalness);
    glUniform1f(glGetUniformLocation(shaderProgram, "roughness"), roughness);
    glUniform1i(glGetUniformLocation(shaderProgram, "texUnit"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "specularTexture"), 1);
    glUniform1i(glGetUniformLocation(shaderProgram, "irradianceTexture"), 2);
    glUniform1i(glGetUniformLocation(shaderProgram, "specularBRDF_LUT"), 3);
    glUniform1i(glGetUniformLocation(shaderProgram, "metallicTexture"), 4);
    glUniform1i(glGetUniformLocation(shaderProgram, "roughnessTexture"), 5);
    glUniform1i(glGetUniformLocation(shaderProgram, "normalTexture"), 6);
    glUniform1i(glGetUniformLocation(shaderProgram, "depthMap"), 7);

}


void MeshRendererContext::render_tensor_pre(bool msaa, GLuint fb1, GLuint fb2) {

    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (msaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, fb1);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glEnable(GL_DEPTH_TEST);
}


void MeshRendererContext::render_tensor_post() {
    glDisable(GL_DEPTH_TEST);
}

void MeshRendererContext::cglBindVertexArray(int vao) {
    glBindVertexArray(vao);
}

void MeshRendererContext::cglUseProgram(int shaderProgram) {
    glUseProgram(shaderProgram);
}

int MeshRendererContext::loadTexture(std::string filename) {
    //width, height = img.size
    // glTexImage2D expects the first element of the image data to be the
    // bottom-left corner of the image.  Subsequent elements go left to right,
    // with subsequent lines going from bottom to top.

    // However, the image data was created with PIL Image tostring and numpy's
    // fromstring, which means we have to do a bit of reorganization. The first
    // element in the data output by tostring() will be the top-left corner of
    // the image, with following values going left-to-right and lines going
    // top-to-bottom.  So, we need to flip the vertical coordinate (y).

    int w;
    int h;
    int comp;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb);

    if (image == nullptr)
        throw (std::string("ERROR: Failed to load texture"));


    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(image);
    return texture;
}

void MeshRendererContext::setup_pbr(std::string shader_path, std::string env_texture_filename) {

    //glEnable(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    //glFrontFace(GL_CCW);


    envTextureUnfiltered = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F, 0);

    // Load & convert equirectangular environment map to a cubemap texture.
    {
        GLuint equirectToCubeProgram = linkProgram({
                                                           compileShader(shader_path + "/450/equirect2cube_cs.glsl",
                                                                         GL_COMPUTE_SHADER)
                                                   });

        envTextureEquirect = createTexture(Image::fromFile(env_texture_filename, 3), GL_RGB, GL_RGB16F, 1);

        glUseProgram(equirectToCubeProgram);
        glBindTextureUnit(0, envTextureEquirect.id);
        glBindImageTexture(0, envTextureUnfiltered.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(envTextureUnfiltered.width / 32, envTextureUnfiltered.height / 32, 6);

        //glDeleteTextures(1, &envTextureEquirect.id);
        glDeleteProgram(equirectToCubeProgram);
    }
    glGenerateTextureMipmap(envTextureUnfiltered.id);

    {
        GLuint spmapProgram = linkProgram({
                                                  compileShader(shader_path + "/450/spmap_cs.glsl", GL_COMPUTE_SHADER)
                                          });

        m_envTexture = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F, 0);

        // Copy 0th mipmap level into destination environment map.
        glCopyImageSubData(envTextureUnfiltered.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
                           m_envTexture.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
                           m_envTexture.width, m_envTexture.height, 6);

        glUseProgram(spmapProgram);
        glBindTextureUnit(0, envTextureUnfiltered.id);

        // Pre-filter rest of the mip chain.
        const float deltaRoughness = 1.0f / glm::max(float(m_envTexture.levels - 1), 1.0f);
        for (int level = 1, size = kEnvMapSize / 2; level <= m_envTexture.levels; ++level, size /= 2) {
            const GLuint numGroups = glm::max(1, size / 32);
            glBindImageTexture(0, m_envTexture.id, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            glProgramUniform1f(spmapProgram, 0, level * deltaRoughness);
            glDispatchCompute(numGroups, numGroups, 6);
        }
        glDeleteProgram(spmapProgram);
    }

    //glDeleteTextures(1, &envTextureUnfiltered.id);

    // Compute diffuse irradiance cubemap.
    {
        GLuint irmapProgram = linkProgram({
                                                  compileShader(shader_path + "/450/irmap_cs.glsl", GL_COMPUTE_SHADER)
                                          });

        m_irmapTexture = createTexture(GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, GL_RGBA16F, 1);

        glUseProgram(irmapProgram);
        glBindTextureUnit(0, m_envTexture.id);
        glBindImageTexture(0, m_irmapTexture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(m_irmapTexture.width / 32, m_irmapTexture.height / 32, 6);
        glDeleteProgram(irmapProgram);
    }

    // Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
    {
        GLuint spBRDFProgram = linkProgram({
                                                   compileShader(shader_path + "/450/spbrdf_cs.glsl", GL_COMPUTE_SHADER)
                                           });

        m_spBRDF_LUT = createTexture(GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG16F, 1);
        glTextureParameteri(m_spBRDF_LUT.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_spBRDF_LUT.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glUseProgram(spBRDFProgram);
        glBindImageTexture(0, m_spBRDF_LUT.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
        glDispatchCompute(m_spBRDF_LUT.width / 32, m_spBRDF_LUT.height / 32, 1);
        glDeleteProgram(spBRDFProgram);
    }

    glFinish();

    std::cout << "INFO: compiled pbr shaders" << std::endl;
}

GLuint MeshRendererContext::linkProgram(std::initializer_list<GLuint> shaders) {
    GLuint program = glCreateProgram();

    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }
    glLinkProgram(program);
    for (GLuint shader : shaders) {
        glDetachShader(program, shader);
        glDeleteShader(shader);
    }

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) {
        glValidateProgram(program);
        glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
    }
    if (status != GL_TRUE) {
        GLsizei infoLogSize;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogSize);
        std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
        glGetProgramInfoLog(program, infoLogSize, nullptr, infoLog.get());
        throw std::runtime_error(std::string("Program link failed\n") + infoLog.get());
    }
    return program;
}

std::string MeshRendererContext::readText(const std::string &filename) {
    std::ifstream file{filename};
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint MeshRendererContext::compileShader(const std::string &filename, GLenum type) {
    const std::string src = readText(filename);
    if (src.empty()) {
        throw std::runtime_error("Cannot read shader source file: " + filename);
    }
    const GLchar *srcBufferPtr = src.c_str();

    std::printf("Compiling GLSL shader: %s\n", filename.c_str());

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &srcBufferPtr, nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLsizei infoLogSize;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogSize);
        std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
        glGetShaderInfoLog(shader, infoLogSize, nullptr, infoLog.get());
        throw std::runtime_error(std::string("Shader compilation failed: ") + filename + "\n" + infoLog.get());
    }
    return shader;
}

int MeshRendererContext::numMipmapLevels(int width, int height)
	{
		int levels = 1;
		while((width|height) >> levels) {
			++levels;
		}
		return levels;
	}

Texture
MeshRendererContext::createTexture(GLenum target, int width, int height, GLenum internalformat, int levels) const {
    Texture texture;
    texture.width = width;
    texture.height = height;
    texture.levels = (levels > 0) ? levels : numMipmapLevels(width, height);

    glCreateTextures(target, 1, &texture.id);
    glTextureStorage2D(texture.id, texture.levels, internalformat, width, height);
    glTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER, texture.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTextureParameterf(texture.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_capabilities.maxAnisotropy);
    return texture;
}

Texture
MeshRendererContext::createTexture(const std::shared_ptr<class Image> &image, GLenum format, GLenum internalformat,
                                   int levels) const {
    Texture texture = createTexture(GL_TEXTURE_2D, image->width(), image->height(), internalformat, levels);
    if (image->isHDR()) {
        glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, GL_FLOAT,
                            image->pixels<float>());
    } else {
        glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, GL_UNSIGNED_BYTE,
                            image->pixels<unsigned char>());
    }


    //std::vector<unsigned char> emptyData(texture.width * texture.height * 3, 0);
    //glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, GL_UNSIGNED_BYTE, &emptyData);


    if (texture.levels > 1) {
        glGenerateTextureMipmap(texture.id);
    }
    return texture;
}

int MeshRendererContext::allocateTexture(int w, int h) {
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA,
                 GL_FLOAT, NULL);
    return texture;
}


py::array_t<float>
MeshRendererContext::readbuffer_meshrenderer_shadow_depth(int width, int height, GLuint fb2, GLuint texture_id) {
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    py::array_t<float> data = py::array_t<float>(3 * width * height);
    py::buffer_info buf = data.request();
    float *ptr = (float *) buf.ptr;
    glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, ptr);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, ptr);
    return data;
}


py::list MeshRendererContext::generateArrayTextures(std::vector<std::string> filenames, int texCutoff, bool shouldShrinkSmallTextures, int smallTexBucketSize) {
		int num_textures = filenames.size();
		std::vector<unsigned char*> image_data;
		std::vector<int> texHeights;
		std::vector<int> texWidths;
		std::vector<int> texChannels;

		printf("number of textures %d\n", num_textures);
		for (int i = 0; i < num_textures; i++) {
			std::string filename = filenames[i];
			std::cout << "Filename is: " << filename << std::endl;
			int w;
			int h;
			int comp;
			stbi_set_flip_vertically_on_load(true);
			unsigned char* image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb); // force to 3 channels
			if (image == nullptr)
				throw(std::string("Failed to load texture"));
			comp = 3;
			image_data.push_back(image);
			texHeights.push_back(h);
			texWidths.push_back(w);
			texChannels.push_back(comp);
		}


		GLuint texId1, texId2;
		glGenTextures(1, &texId1);
		glGenTextures(1, &texId2);

		py::list texInfo;
		py::list texLayerData;

		// Larger textures
		int firstTexLayerNum = 0;
		// Smaller textures
		int secondTexLayerNum = 0;

		std::vector<std::vector<int>> texIndices;
		std::vector<int> firstTexIndices, secondTexIndices;
		texIndices.push_back(firstTexIndices);
		texIndices.push_back(secondTexIndices);

		// w1, h1, w2, h2
		std::vector<int> texLayerDims;
		for (int i = 0; i < 4; i++) {
			texLayerDims.push_back(0);
		}

		for (int i = 0; i < image_data.size(); i++) {
			// Figure out if this texture goes in left group or right group based on w * h
			int w = texWidths[i];
			int h = texHeights[i];
			int score = w * h;

			py::list tex_info_i;

			// Texture goes in larger bucket if larger than cutoff
			if (score >= texCutoff) {
				std::cout << "Appending texture with name: " << filenames[i] << " to large bucket" << std::endl;
				texIndices[0].push_back(i);
				tex_info_i.append(0);
				tex_info_i.append(firstTexLayerNum);
				if (w > texLayerDims[0]) texLayerDims[0] = w;
				if (h > texLayerDims[1]) texLayerDims[1] = h;
				firstTexLayerNum++;
			}
			else {
				std::cout << "Appending texture with name: " << filenames[i] << " to small bucket" << std::endl;
				texIndices[1].push_back(i);
				tex_info_i.append(1);
				tex_info_i.append(secondTexLayerNum);
				if (w > texLayerDims[2]) texLayerDims[2] = w;
				if (h > texLayerDims[3]) texLayerDims[3] = h;
				secondTexLayerNum++;
			}

			texLayerData.append(tex_info_i);
		}

		printf("Texture 1 is w:%d by h:%d by depth:%d 3D array texture. ID %d\n", texLayerDims[0], texLayerDims[1], firstTexLayerNum, texId1);
		if (shouldShrinkSmallTextures) {
			printf("Texture 2 is w:%d by h:%d by depth:%d 3D array texture. ID %d\n", smallTexBucketSize, smallTexBucketSize, secondTexLayerNum, texId2);
		}
		else {
			printf("Texture 2 is w:%d by h:%d by depth:%d 3D array texture. ID %d\n", texLayerDims[2], texLayerDims[3], secondTexLayerNum, texId2);
		}

		for (int i = 0; i < 2; i++) {
			GLuint currTexId = texId1;
			if (i == 1) currTexId = texId2;

			glBindTexture(GL_TEXTURE_2D_ARRAY, currTexId);

			// Print texture array data
			if (i == 0) {
				GLint max_layers, max_size;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
				glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max_size);
				printf("Max layer number: %d\n", max_layers);
				printf("Max texture size: %d\n", max_size);
			}

			int layerNum = firstTexLayerNum;
			if (i == 1) layerNum = secondTexLayerNum;

			int out_w = texLayerDims[2 * static_cast<long long int>(i)];
			int out_h = texLayerDims[2 * static_cast<long long int>(i) + 1];


			// Gibson tends to have many more smaller textures, so we reduce their size to avoid memory overload
			if (i == 1 && shouldShrinkSmallTextures) {
				out_w = smallTexBucketSize;
				out_h = smallTexBucketSize;
			}

			// Deal with empty texture - create placeholder
			if (out_w == 0 || out_h == 0 || layerNum == 0) {
				glTexImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					GL_RGB,
					1,
					1,
					1,
					0,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					NULL
				);
			}

			glTexImage3D(GL_TEXTURE_2D_ARRAY,
				0,
				GL_RGB,
				out_w,
				out_h,
				layerNum,
				0,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				NULL
			);

			// Add all textures in texture array i to that array texture
			for (int j = 0; j < layerNum; j++) {

				int idx = texIndices[i][j];

				int orig_w = texWidths[idx];
				int orig_h = texHeights[idx];
				int n_channels = texChannels[idx];
				unsigned char* input_data = image_data[idx];
				unsigned char* tex_bytes = input_data;
				bool shouldResize = (orig_w != out_w || orig_h != out_h);
				// Resize image to fit biggest texture in texture array
				if (shouldResize) {
					unsigned char* output_data = (unsigned char*)malloc(out_w * out_h * n_channels);
					stbir_resize_uint8(input_data, orig_w, orig_h, 0, output_data, out_w, out_h, 0, n_channels);
					tex_bytes = output_data;
				}

				glBindTexture(GL_TEXTURE_2D_ARRAY, currTexId);
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					0,
					0,
					j,
					out_w,
					out_h,
					1,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					tex_bytes
				);

				stbi_image_free(input_data);
				if (shouldResize) {
					free(tex_bytes);
				}
			}

			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		texInfo.append(texId1);
		texInfo.append(texId2);
		texInfo.append(texLayerData);

		// texid1, texid2, list of idx - texid/layer data
		return texInfo;
	}

	// Performs optimized render setup
	py::list MeshRendererContext::renderSetup(int shaderProgram, py::array_t<float> V, py::array_t<float> P, py::array_t<float> lightpos, py::array_t<float> lightcolor,
		py::array_t<float> mergedVertexData, py::array_t<int> index_ptr_offsets, py::array_t<int> index_counts,
		py::array_t<int> indices, py::array_t<float> mergedFragData, py::array_t<float> mergedFragRMData,
		py::array_t<float> mergedFragNData,
		py::array_t<float> mergedDiffuseData,
		int tex_id_1, int tex_id_2, GLuint fb,
		float use_pbr) {
		// First set up VAO and corresponding attributes
		GLuint VAO;
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		GLuint EBO;
		glGenBuffers(1, &EBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		int* indicesPtr = (int*)indices.request().ptr;
		std::vector<unsigned int> indexData;
		for (int i = 0; i < indices.size(); i++) {
			indexData.push_back((unsigned int)indicesPtr[i]);
		}

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(unsigned int), &indexData[0], GL_STATIC_DRAW);

		GLuint VBO;
		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		float* mergedVertexDataPtr = (float*)mergedVertexData.request().ptr;
		glBufferData(GL_ARRAY_BUFFER, mergedVertexData.size() * sizeof(float), mergedVertexDataPtr, GL_STATIC_DRAW);

		GLuint positionAttrib = glGetAttribLocation(shaderProgram, "position");
        GLuint normalAttrib = glGetAttribLocation(shaderProgram, "normal");
        GLuint coordsAttrib = glGetAttribLocation(shaderProgram, "texCoords");
        GLuint tangentlAttrib = glGetAttribLocation(shaderProgram, "tangent");
        GLuint bitangentAttrib = glGetAttribLocation(shaderProgram, "bitangent");

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);

        glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 0);
        glVertexAttribPointer(normalAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 12);
        glVertexAttribPointer(coordsAttrib, 2, GL_FLOAT, GL_TRUE, 56, (void *) 24);
        glVertexAttribPointer(tangentlAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 32);
        glVertexAttribPointer(bitangentAttrib, 3, GL_FLOAT, GL_FALSE, 56, (void *) 44);

		glBindVertexArray(0);

		multidrawCount = index_ptr_offsets.size();
		int* indexOffsetPtr = (int*)index_ptr_offsets.request().ptr;

		for (int i = 0; i < multidrawCount; i++) {
			unsigned int offset = (unsigned int)indexOffsetPtr[i];
			this->multidrawStartIndices.push_back(BUFFER_OFFSET((offset * sizeof(unsigned int))));
			printf("multidraw start idx %d\n", offset);
		}

		// Store for rendering
		int* indices_count_ptr = (int*)index_counts.request().ptr;
		for (int i = 0; i < multidrawCount; i++) {
			this->multidrawCounts.push_back(indices_count_ptr[i]);
		}

		// Set up shaders
		float* fragData = (float*)mergedFragData.request().ptr;
        float* fragRMData = (float*)mergedFragRMData.request().ptr;
		float* fragNData = (float*)mergedFragNData.request().ptr;
		float* diffuseData = (float*)mergedDiffuseData.request().ptr;
		int fragDataSize = mergedFragData.size();
		int diffuseDataSize = mergedDiffuseData.size();

		glUseProgram(shaderProgram);

		float* Vptr = (float*)V.request().ptr;
		float* Pptr = (float*)P.request().ptr;
		float* lightposptr = (float*)lightpos.request().ptr;
		float* lightcolorptr = (float*)lightcolor.request().ptr;
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_TRUE, Vptr);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, Pptr);

		glUniform3f(glGetUniformLocation(shaderProgram, "light_position"), lightposptr[0], lightposptr[1], lightposptr[2]);
		glUniform3f(glGetUniformLocation(shaderProgram, "light_color"), lightcolorptr[0], lightcolorptr[1], lightcolorptr[2]);
        glUniform1f(glGetUniformLocation(shaderProgram, "use_pbr"), use_pbr);

		printf("multidrawcount %d\n", multidrawCount);

		glGenBuffers(1, &uboTexColorData);
		glBindBuffer(GL_UNIFORM_BUFFER, uboTexColorData);
		texColorDataSize = 4 * 16 * MAX_ARRAY_SIZE;
		glBufferData(GL_UNIFORM_BUFFER, texColorDataSize, NULL, GL_STATIC_DRAW);
		GLuint texColorDataIdx = glGetUniformBlockIndex(shaderProgram, "TexColorData");
		glUniformBlockBinding(shaderProgram, texColorDataIdx, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboTexColorData);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, fragDataSize * sizeof(float), fragData);
        glBufferSubData(GL_UNIFORM_BUFFER, 16 * MAX_ARRAY_SIZE, fragDataSize * sizeof(float), fragRMData);
		glBufferSubData(GL_UNIFORM_BUFFER, 2 * 16 * MAX_ARRAY_SIZE, fragDataSize * sizeof(float), fragNData);
		glBufferSubData(GL_UNIFORM_BUFFER, 3 * 16 * MAX_ARRAY_SIZE, diffuseDataSize * sizeof(float), diffuseData);

		glGenBuffers(1, &uboTransformData);
		glBindBuffer(GL_UNIFORM_BUFFER, uboTransformData);
		transformDataSize = 2 * 64 * MAX_ARRAY_SIZE;
		glBufferData(GL_UNIFORM_BUFFER, transformDataSize, NULL, GL_DYNAMIC_DRAW);
		GLuint transformDataIdx = glGetUniformBlockIndex(shaderProgram, "TransformData");
		glUniformBlockBinding(shaderProgram, transformDataIdx, 1);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboTransformData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		GLuint bigTexLoc = glGetUniformLocation(shaderProgram, "bigTex");
		GLuint smallTexLoc = glGetUniformLocation(shaderProgram, "smallTex");

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id_1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id_2);

        glActiveTexture(GL_TEXTURE2);
        if (use_pbr == 1) glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTexture.id);

        glActiveTexture(GL_TEXTURE3);
        if (use_pbr == 1) glBindTexture(GL_TEXTURE_CUBE_MAP, m_irmapTexture.id);

        glActiveTexture(GL_TEXTURE4);
        if (use_pbr == 1) glBindTexture(GL_TEXTURE_2D, m_spBRDF_LUT.id);

		glUniform1i(bigTexLoc, 0);
		glUniform1i(smallTexLoc, 1);
		glUniform1i(glGetUniformLocation(shaderProgram, "specularTexture"), 2);
        glUniform1i(glGetUniformLocation(shaderProgram, "irradianceTexture"), 3);
        glUniform1i(glGetUniformLocation(shaderProgram, "specularBRDF_LUT"), 4);

		glUseProgram(0);

		py::list renderData;
		renderData.append(VAO);
		renderData.append(VBO);
		renderData.append(EBO);

		return renderData;
	}

	// Updates positions and rotations in vertex shader
	void MeshRendererContext::updateDynamicData(int shaderProgram, py::array_t<float> pose_trans_array,
	py::array_t<float> pose_rot_array, py::array_t<float> V, py::array_t<float> P, py::array_t<float> eye_pos) {
		glUseProgram(shaderProgram);

		float* transPtr = (float*)pose_trans_array.request().ptr;
		float* rotPtr = (float*)pose_rot_array.request().ptr;
		int transDataSize = pose_trans_array.size();
		int rotDataSize = pose_rot_array.size();

		glBindBuffer(GL_UNIFORM_BUFFER, uboTransformData);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, transDataSize * sizeof(float), transPtr);
		glBufferSubData(GL_UNIFORM_BUFFER, transformDataSize / 2, rotDataSize * sizeof(float), rotPtr);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		float* Vptr = (float*)V.request().ptr;
		float* Pptr = (float*)P.request().ptr;
        float *eye_pos_ptr = (float *) eye_pos.request().ptr;

		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_TRUE, Vptr);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, Pptr);
        glUniform3f(glGetUniformLocation(shaderProgram, "eyePosition"), eye_pos_ptr[0], eye_pos_ptr[1], eye_pos_ptr[2]);

	}

	// Optimized rendering function that is called once per frame for all merged data
	void MeshRendererContext::renderOptimized(GLuint VAO) {
		glBindVertexArray(VAO);
		glMultiDrawElements(GL_TRIANGLES, &this->multidrawCounts[0], GL_UNSIGNED_INT, &this->multidrawStartIndices[0], this->multidrawCount);
	}

void MeshRendererContext::clean_meshrenderer_optimized(std::vector<GLuint> color_attachments, std::vector<GLuint> textures, std::vector<GLuint> fbo, std::vector<GLuint> vaos, std::vector<GLuint> vbos, std::vector<GLuint> ebos) {
		glDeleteTextures(color_attachments.size(), color_attachments.data());
		glDeleteTextures(textures.size(), textures.data());
		glDeleteFramebuffers(fbo.size(), fbo.data());
		glDeleteBuffers(vaos.size(), vaos.data());
		glDeleteBuffers(vbos.size(), vbos.data());
		glDeleteBuffers(ebos.size(), ebos.data());
		glDeleteBuffers(1, &uboTexColorData);
		glDeleteBuffers(1, &uboTransformData);
	}


void MeshRendererContext::loadSkyBox(int shaderProgram, float skybox_size){
    GLint vertex = glGetAttribLocation(shaderProgram, "position");
    GLfloat cube_vertices[] = {
	  -1.0,  1.0,  1.0,
	  -1.0, -1.0,  1.0,
	   1.0, -1.0,  1.0,
	   1.0,  1.0,  1.0,
	  -1.0,  1.0, -1.0,
	  -1.0, -1.0, -1.0,
	   1.0, -1.0, -1.0,
	   1.0,  1.0, -1.0,
	};
	for (int i = 0; i < 24; i++) cube_vertices[i] *= skybox_size;
	GLuint vbo_cube_vertices;
	glGenBuffers(1, &vbo_cube_vertices);
	m_skybox_vbo = vbo_cube_vertices;
	glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(vertex);
    glVertexAttribPointer(vertex, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// cube indices for index buffer object
	GLushort cube_indices[] = {
	  0, 1, 2, 3,
	  3, 2, 6, 7,
	  7, 6, 5, 4,
	  4, 5, 1, 0,
	  0, 3, 7, 4,
	  1, 2, 6, 5,
	};
	GLuint ibo_cube_indices;
	glGenBuffers(1, &ibo_cube_indices);
	m_skybox_ibo = ibo_cube_indices;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}
void MeshRendererContext::renderSkyBox(int shaderProgram, py::array_t<float> V, py::array_t<float> P){
    glUseProgram(shaderProgram);
    float* Vptr = (float*)V.request().ptr;
    float* Pptr = (float*)P.request().ptr;

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "V"), 1, GL_TRUE, Vptr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "P"), 1, GL_FALSE, Pptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTexture.id);
    glUniform1i(glGetUniformLocation(shaderProgram, "envTexture"), 0);
    glBindBuffer(GL_ARRAY_BUFFER, m_skybox_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_skybox_ibo);

    glDrawElements(GL_QUADS, 24, GL_UNSIGNED_SHORT, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

}