// Copyright 2018 ETH Zürich, Thomas Schöps
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <cuda_runtime.h>

#include <libvis/camera.h>
#include <libvis/camera_frustum_opengl.h>
#include <libvis/eigen.h>
#include <libvis/libvis.h>
#include <libvis/mesh_opengl.h>
#include <libvis/opengl.h>
#include <libvis/opengl_context.h>
#include <libvis/point_cloud_opengl.h>
#include <libvis/render_window.h>
#include <libvis/sophus.h>

#include <cuda_gl_interop.h>  // Must be included late so that glew.h is included before gl.h

#include "surfel_meshing/surfel_meshing.h"  // Must be included before any header that defines Bool

#if defined(WIN32) || defined(_Windows) || defined(_WINDOWS) || \
    defined(_WIN32) || defined(__WIN32__)

#else //linux
#include <GL/glx.h>
#endif //_WIN32 & linux

namespace vis {

class SurfelMeshing;

// Renders the SurfelMeshing visualization.
class SurfelMeshingRenderWindow : public RenderWindowCallbacks {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  
  SurfelMeshingRenderWindow(
      bool render_new_surfels_as_splats,
      float splat_half_extent_in_pixels,
      bool triangle_normal_shading,
      bool render_camera_frustum);
  
  virtual void Initialize() override;
  
  virtual void Resize(int width, int height) override;
  
  virtual void Render() override;
  
  virtual void MouseDown(MouseButton button, int x, int y) override;
  virtual void MouseMove(int x, int y) override;
  virtual void MouseUp(MouseButton button, int x, int y) override;
  virtual void WheelRotated(float degrees, Modifier modifiers) override;
  virtual void KeyPressed(char key, Modifier modifiers) override;
  virtual void KeyReleased(char key, Modifier modifiers) override;
  
  // Performs initializations, sets up CUDA interop.
  // Intended to be called from outside the Qt thread.
  void InitializeForCUDAInterop(
      usize max_point_count,
      cudaGraphicsResource** vertex_buffer_resource,
      OpenGLContext* context,
      const Camera& camera,
      bool debug_neighbor_rendering,
      bool debug_normal_rendering,
      cudaGraphicsResource** neighbor_index_buffer_resource,
      cudaGraphicsResource** normal_vertex_buffer_resource);
  
  // Intended to be called from outside the Qt thread.
  void UpdateVisualizationCloud(const std::shared_ptr<Point3fC3u8Cloud>& cloud);
  void UpdateVisualizationCloudCUDA(u32 surfel_count, u32 latest_mesh_surfel_count);
  
  // Intended to be called from outside the Qt thread.
  void UpdateVisualizationMesh(const std::shared_ptr<Mesh3fCu8>& mesh);
  void UpdateVisualizationMeshCUDA(const std::shared_ptr<vis::Mesh3fCu8>& mesh);
  
  void UpdateVisualizationCloudAndMeshCUDA(u32 surfel_count, const std::shared_ptr<vis::Mesh3fCu8>& mesh);
  
  // Intended to be called from outside the Qt thread.
  void SetUpDirection(const Vec3f& direction);
  
  // Intended to be called from outside the Qt thread.
  void CenterViewOn(const Vec3f& position);
  
  // Sets a view which is computed from the given look_at point and camera
  // position. Also sets the input camera pose.
  // Intended to be called from outside the Qt thread.
  void SetView(const Vec3f& look_at, const Vec3f& camera_pos, const SE3f& global_T_camera);
  
  // Sets an arbitrary view with the given axes and eye position. Also sets
  // the input camera pose.
  // Intended to be called from outside the Qt thread.
  void SetView2(const Vec3f& x, const Vec3f& y, const Vec3f& z, const Vec3f& eye, const SE3f& global_T_camera);
  
  // Sets the view parameters directly. Also sets the input camera pose.
  // Intended to be called from outside the Qt thread.
  void SetViewParameters(
      const Vec3f& camera_free_orbit_offset,
      float camera_free_orbit_radius,
      float camera_free_orbit_theta,
      float camera_free_orbit_phi,
      float max_depth,
      const SE3f& global_T_camera);
  
  // Sets the input camera pose.
  // Intended to be called from outside the Qt thread.
  void SetCameraFrustumPose(const SE3f& global_T_camera);
  
  // Intended to be called from outside the Qt thread.
  void RenderFrame();
  
  // Intended to be called from outside the Qt thread.
  void SaveScreenshot(const char* filepath);
  
  void GetCameraPoseParameters(
      Vec3f* camera_free_orbit_offset,
      float* camera_free_orbit_radius,
      float* camera_free_orbit_theta,
      float* camera_free_orbit_phi);
  
  inline std::mutex& render_mutex() { return render_mutex_; };
  inline const std::mutex& render_mutex() const { return render_mutex_; };
  
  
  // For debugging only, not thread-safe.
  void SetReconstructionForDebugging(SurfelMeshing* reconstruction);
  u32 selected_surfel_index() const { return selected_surfel_index_; }
  
 private:
  void RenderPointSplats();
  void RenderMesh(const Mat4f& model_matrix, const Vec3f& viewing_dir);
  void RenderCameraFrustum(const SE3f& global_T_camera_frustum);
  void RenderNeighbors();
  void RenderNormals();
  
  void InitializeForCUDAInteropInRenderingThread();
  
  void SetCamera();
  void SetViewpoint();
  void ComputeProjectionMatrix();
  void SetupViewport();
  
  void CreateSplatProgram();
  void CreateMeshProgram();
  void CreateConstantColorProgram();
  void CreateTriangleNormalShadedProgram();
  
  
  // Settings.
  bool render_new_surfels_as_splats_;
  float splat_half_extent_in_pixels_;
  bool triangle_normal_shading_;
  bool render_camera_frustum_;
  
  bool render_as_wireframe_;
  bool show_surfels_;
  bool show_mesh_;
  
  int width_;
  int height_;
  
  // Input handling.
  bool dragging_;
  int last_drag_x_;
  int last_drag_y_;
  int pressed_mouse_buttons_;
  bool m_pressed_;
  
  // Render camera and pose.
  SE3f camera_T_world_;
  PinholeCamera4f render_camera_;
  
  float min_depth_;
  float max_depth_;
  
  std::mutex camera_mutex_;
  Mat3f up_direction_rotation_;
  Vec3f camera_free_orbit_offset_;
  float camera_free_orbit_radius_;
  float camera_free_orbit_theta_;
  float camera_free_orbit_phi_;
  
  Mat4f camera_matrix_;
  bool use_camera_matrix_;
  
  Mat4f projection_matrix_;
  Mat4f model_view_projection_matrix_;
  
  // Input camera frustum and pose.
  CameraFrustumOpenGL camera_frustum_;
  SE3f global_T_camera_frustum_;
  bool camera_frustum_set_;
  
  // Vertex buffer handling.
  std::mutex visualization_cloud_mutex_;
  bool have_visualization_cloud_;
  Point3fC3u8CloudOpenGL visualization_cloud_;
  std::shared_ptr<Point3fC3u8Cloud> new_visualization_cloud_;
  std::shared_ptr<Point3fC3u8Cloud> current_visualization_cloud_;
  usize visualization_cloud_size_;
  usize mesh_surfel_count_;
  usize new_visualization_cloud_size_;
  usize new_mesh_surfel_count_;
  
  // Index buffer handling.
  std::mutex visualization_mesh_mutex_;
  bool have_visualization_mesh_;
  Mesh3fC3u8OpenGL visualization_mesh_;
  std::shared_ptr<Mesh3fCu8> new_visualization_mesh_;
  
  // Buffers for debugging.
  GLuint neighbor_index_buffer_;
  GLuint normal_vertex_buffer_;
  
  // Splat program.
  ShaderProgramOpenGL splat_program_;
  GLint splat_u_model_view_projection_matrix_location_;
  GLint splat_u_point_size_x_location_;
  GLint splat_u_point_size_y_location_;
  
  // Constant color program.
  ShaderProgramOpenGL constant_color_program_;
  GLint constant_color_u_model_view_projection_matrix_location_;
  GLint constant_color_u_constant_color_location_;
  
  // TriNormalShaded program.
  ShaderProgramOpenGL tri_normal_shaded_program_;
  GLint tri_normal_shaded_u_model_matrix_location_;
  GLint tri_normal_shaded_u_model_view_projection_matrix_location_;
  GLint tri_normal_shaded_u_light_source_location_;
  
  // Mesh program.
  ShaderProgramOpenGL mesh_program_;
  GLint mesh_u_model_view_projection_matrix_location_;
  
  // Parameters passed to the render thread by InitializeForCUDAInterop().
  usize init_max_point_count_;
  cudaGraphicsResource** init_vertex_buffer_resource_;
  const Camera* init_camera_;
  bool init_debug_neighbor_rendering_;
  bool init_debug_normal_rendering_;
  cudaGraphicsResource** init_neighbor_index_buffer_resource_;
  cudaGraphicsResource** init_normal_vertex_buffer_resource_;
  std::atomic<bool> init_done_;
  mutex init_mutex_;
  condition_variable init_condition_;
  
  // Screenshot handling.
  string screenshot_path_;
  mutex screenshot_mutex_;
  condition_variable screenshot_condition_;
  
  // Other.
  std::mutex render_mutex_;
  OpenGLContext qt_gl_context_;
  
  // For debugging.
  SurfelMeshing* reconstruction_;
  u32 selected_surfel_index_;
};

}
