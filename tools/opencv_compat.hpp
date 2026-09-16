#ifndef TOOLS__OPENCV_COMPAT_HPP
#define TOOLS__OPENCV_COMPAT_HPP

#include <opencv2/core/version.hpp>

// OpenCV 5 moved the 2D/3D geometry APIs out of imgproc/calib3d.
#if CV_VERSION_MAJOR >= 5
#include <opencv2/geometry/2d.hpp>
#include <opencv2/geometry/3d.hpp>
#else
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#endif

#endif  // TOOLS__OPENCV_COMPAT_HPP
