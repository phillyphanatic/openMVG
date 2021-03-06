// Copyright (c) 2013 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_BUNDLE_ADJUSTMENT_PINHOLE_CERES_FUNCTOR_HPP
#define OPENMVG_BUNDLE_ADJUSTMENT_PINHOLE_CERES_FUNCTOR_HPP

#include "ceres/ceres.h"
#include "ceres/rotation.h"

namespace openMVG{
namespace bundle_adjustment{

/**
 * @group pinhole_reprojectionError
 * @{
 * Definition of a Functor for minimization of the reprojection error
 * (||x^j_i - P_{ij}(X_j)||)
 * A 3D point X_j is projected on a image plane i and compare to the observation
 *  x^j_i.
 */
namespace pinhole_reprojectionError {

/**
 * @brief Compute the residual error after reprojection.
 * residual = observed - euclidean( f * [R|t] X)
 * 
 * @warning Principal point is assumed being applied on observed points.
 * 
 * @param[in] cam_R Angle-axis camera rotation
 * @param[in] cam_t (x, y, z) Camera translation
 * @param[in] cam_f (f) Focal length
 * @param[in] pos_3dpoint The observed 3D point
 * @param[in] pos_2dpoint The image plane observation
 * @param[out] out_residuals The residuals along the x and y axis
 */
template <typename T>
void computeResidual(
  const T* const cam_R,
  const T* const cam_t,
  const T* const cam_f,
  const T* const pos_3dpoint,
  const double* pos_2dpoint,
  T* out_residuals )
{
  T pos_proj[3];
  
  // Appply the angle-axis camera rotation
  ceres::AngleAxisRotatePoint(cam_R, pos_3dpoint, pos_proj);

  // Apply the camera translation
  pos_proj[0] += cam_t[0];
  pos_proj[1] += cam_t[1];
  pos_proj[2] += cam_t[2];

  // Transform the point from homogeneous to euclidean
  T xe = pos_proj[0] / pos_proj[2];
  T ye = pos_proj[1] / pos_proj[2];

  // Apply the focal length
  const T& focal = cam_f[0];
  T predicted_x = focal * xe;
  T predicted_y = focal * ye;

  // Compute and return the error is the difference between the predicted 
  //  and observed position
  out_residuals[0] = predicted_x - T(pos_2dpoint[0]);
  out_residuals[1] = predicted_y - T(pos_2dpoint[1]);
}

/**
 * @brief Ceres functor to refine a pinhole camera model, 3D points and focal.
 * 
 *  - first the intrinsic data block [focal]
 *  - second the camera extrinsic block (camera orientation and position) [R;t]
 *    - 3 for rotation(angle axis), 3 for translation.
 *  - third the 3D point data block
 * 
 * @warning Principal point is assumed being applied on observed points.
 * 
 * @see computeResidual
 */
struct ErrorFunc_Refine_Camera_3DPoints_focal
{
  ErrorFunc_Refine_Camera_3DPoints_focal(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }

  /**
   * @param[in] cam_f: Camera focal
   * @param[in] cam_Rt: Camera parameterized using one block of 6 parameters [R;t]:
   *   - 3 for rotation(angle axis), 3 for translation
   * @param[in] pos_3dpoint
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_f,
    const T* const cam_Rt,
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    computeResidual(
      cam_Rt, // => cam_R
      & cam_Rt[3], // => cam_t
      cam_f,
      pos_3dpoint,
      m_pos_2dpoint,
      out_residuals );

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

/**
 * @brief Ceres functor to refine a pinhole camera model and 3D points.
 * 
 * @see computeResidual
 */
struct ErrorFunc_Refine_Camera_3DPoints
{
  ErrorFunc_Refine_Camera_3DPoints(const double* const pos_2dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];
  }
  
  /**
   * @param[in] cam_Rtf: Camera parameterized using one block of 7 parameters [R;t;f]:
   *   - 3 for rotation(angle axis), 3 for translation, 1 for the focal length.
   * @param[in] pos_3dpoint
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_Rtf, // [R;t;f]
    const T* const pos_3dpoint,
    T* out_residuals) const
  {
    computeResidual(
      cam_Rtf, // => cam_R
      & cam_Rtf[3], // => cam_t
      & cam_Rtf[6], // => cam_f
      pos_3dpoint,
      m_pos_2dpoint,
      out_residuals);
    
    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
};

/**
 * @brief Ceres functor to refine a pinhole camera model with static 
 *  2D 3D observation.
 *
 * @see computeResidual
 */
struct ErrorFunc_Refine_Camera
{
  ErrorFunc_Refine_Camera(const double* const pos_2dpoint, const double* const pos_3dpoint)
  {
    m_pos_2dpoint[0] = pos_2dpoint[0];
    m_pos_2dpoint[1] = pos_2dpoint[1];

    m_pos_3dpoint[0] = pos_3dpoint[0];
    m_pos_3dpoint[1] = pos_3dpoint[1];
    m_pos_3dpoint[2] = pos_3dpoint[2];
  }
  
  /**
   * @param[in] cam_Rtf: Camera parameterized using one block of 7 parameters [R;t;f]:
   *   - 3 for rotation(angle axis), 3 for translation, 1 for the focal length.
   * @param[out] out_residuals
   */
  template <typename T>
  bool operator()(
    const T* const cam_Rtf, // [R;t;f]
    T* out_residuals) const
  {
    T pos_3dpoint[3];
    pos_3dpoint[0] = T(m_pos_3dpoint[0]);
    pos_3dpoint[1] = T(m_pos_3dpoint[1]);
    pos_3dpoint[2] = T(m_pos_3dpoint[2]);
    
    computeResidual(
      cam_Rtf, // => cam_R
      & cam_Rtf[3], // => cam_t
      & cam_Rtf[6], // => cam_f
      pos_3dpoint,
      m_pos_2dpoint,
      out_residuals);

    return true;
  }

  double m_pos_2dpoint[2]; // The 2D observation
  double m_pos_3dpoint[3]; // The 3D observation
};

} // namespace pinhole_reprojectionError
/// @} 
} // namespace bundle_adjustment
} // namespace openMVG

#endif

