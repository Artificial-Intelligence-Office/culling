/***************************************************************************
 *   Copyright (C) 2015 - 2017 by                                          *
 *      Tarek Taha, KURI  <tataha@tarektaha.com>                           *
 *      Randa Almadhoun   <randa.almadhoun@kustar.ac.ae>                   *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/
#ifndef OCCLUSION_GPU_H_
#define OCCLUSION_GPU_H_

#include <eigen_conversions/eigen_msg.h>
#include <geometry_msgs/Pose.h>
#include <ros/package.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/tf.h>
#include <tf_conversions/tf_eigen.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include "ros/ros.h"

//PCL
#include <culling/frustum_culling_gpu.h>
#include <culling/voxel_grid_occlusion_estimation_gpu.h>
#include <geometry_msgs/Point32.h>
#include <geometry_msgs/PoseArray.h>
#include <pcl/common/eigen.h>
#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/range_image/range_image.h>
#include <pcl_conversions/pcl_conversions.h>
#include <iostream>

template <typename PointInT>
class OcclusionCullingGPU
{
  public:
    //attributes
    ros::NodeHandle nh;
    std::string model;
    //     ros::Publisher original_pub;
    //     ros::Publisher visible_pub;
    ros::Publisher fov_pub;
    pcl::PointCloud<PointInT>::Ptr cloud;
    pcl::PointCloud<PointInT>::Ptr cloudCopy;
    pcl::PointCloud<PointInT>::Ptr filtered_cloud;
    pcl::PointCloud<PointInT>::Ptr
        occlusionFreeCloud;  //I can add it to accumulate cloud if I want to extract visible surface from multiple locations
    pcl::PointCloud<PointInT>::Ptr FrustumCloud;  //frustum cull

    pcl::PointCloud<PointInT> freeCloud;
    float voxelRes, OriginalVoxelsSize, viewEntropy;
    double id;
    pcl::VoxelGridOcclusionEstimationGPU voxelFilterOriginal;
    Eigen::Vector3i max_b1, min_b1;
    visualization_msgs::Marker linesList1, linesList2, linesList3, linesList4;
    visualization_msgs::MarkerArray marker_array;
    pcl::FrustumCullingGPU fc;
    double maxAccuracyError, minAccuracyError;
    bool AccuracyMaxSet;
    std::string frame_id;

    //methods
    OcclusionCullingGPU(ros::NodeHandle& n, std::string modelName);
    OcclusionCullingGPU(ros::NodeHandle& n, pcl::PointCloud<PointInT>::Ptr& cloudPtr);
    OcclusionCullingGPU(std::string modelName);
    OcclusionCullingGPU();
    ~OcclusionCullingGPU();
    pcl::PointCloud<PointInT> extractVisibleSurface(geometry_msgs::Pose location);
    //    float calcCoveragePercent(geometry_msgs::Pose location);
    float calcCoveragePercent(pcl::PointCloud<PointInT>::Ptr cloud_filtered);
    double calcAvgAccuracy(pcl::PointCloud<PointInT> pointCloud);
    double calcAvgAccuracy(pcl::PointCloud<PointInT> pointCloud, geometry_msgs::Pose cameraPose);
    void transformPointMatVec(tf::Vector3 translation, tf::Matrix3x3 rotation,
                              geometry_msgs::Point32 in, geometry_msgs::Point32& out);
    pcl::PointCloud<PointInT> pointCloudViewportTransform(pcl::PointCloud<PointInT> pointCloud,
                                                          geometry_msgs::Pose cameraPose);
    void SSMaxMinAccuracy(std::vector<geometry_msgs::PoseArray> sensorsPoses);
    void visualizeFOV(geometry_msgs::Pose location);
    visualization_msgs::Marker drawLines(std::vector<geometry_msgs::Point> links, int id,
                                         int c_color[]);
    bool contains(pcl::PointCloud<PointInT> c, PointInT p);
    pcl::PointCloud<PointInT> pointsDifference(pcl::PointCloud<PointInT> c2);
};

#endif
