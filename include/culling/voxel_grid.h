/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

#include <pcl/filters/boost.h>
#include <pcl/filters/filter.h>
#include <map>
#include <vector>
namespace pcl
{
template <typename PointInT>
void getMinMax3D(const typename pcl::PointCloud<PointInT>::ConstPtr &cloud,
                 const std::string &distance_field_name, float min_distance, float max_distance,
                 Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt, bool limit_negative = false);

/** \brief Get the minimum and maximum values on each of the 3 (x-y-z) dimensions
    * in a given pointcloud, without considering points outside of a distance threshold from the laser origin
    * \param[in] cloud the point cloud data message
    * \param[in] indices the vector of indices to use
    * \param[in] distance_field_name the field name that contains the distance values
    * \param[in] min_distance the minimum distance a point will be considered from
    * \param[in] max_distance the maximum distance a point will be considered to
    * \param[out] min_pt the resultant minimum bounds
    * \param[out] max_pt the resultant maximum bounds
    * \param[in] limit_negative if set to true, then all points outside of the interval (min_distance;max_distace) are considered
    * \ingroup filters
    */
template <typename PointInT>
void getMinMax3D(const typename pcl::PointCloud<PointInT>::ConstPtr &cloud,
                 const std::vector<int> &indices, const std::string &distance_field_name,
                 float min_distance, float max_distance, Eigen::Vector4f &min_pt,
                 Eigen::Vector4f &max_pt, bool limit_negative = false);

/** \brief VoxelGrid assembles a local 3D grid over a given PointCloud, and downsamples + filters the data.
    *
    * The VoxelGrid class creates a *3D voxel grid* (think about a voxel
    * grid as a set of tiny 3D boxes in space) over the input point cloud data.
    * Then, in each *voxel* (i.e., 3D box), all the points present will be
    * approximated (i.e., *downsampled*) with their centroid. This approach is
    * a bit slower than approximating them with the center of the voxel, but it
    * represents the underlying surface more accurately.
    *
    * \author Radu B. Rusu, Bastian Steder
    * \ingroup filters
    */
template <typename PointInT>
class VoxelGridT : public Filter<PointInT>
{
  protected:
    using Filter<PointInT>::filter_name_;
    using Filter<PointInT>::getClassName;
    using Filter<PointInT>::input_;
    using Filter<PointInT>::indices_;

    typedef typename Filter<PointInT>::PointCloud PointCloud;
    typedef typename PointCloud::Ptr PointCloudPtr;
    typedef typename PointCloud::ConstPtr PointCloudConstPtr;
    typedef boost::shared_ptr<VoxelGridT> Ptr;
    typedef boost::shared_ptr<const VoxelGridT> ConstPtr;

  public:
    //new variables
    std::vector<std::vector<std::vector<std::vector<PointInT> > > >
        voxelSet;  //include the set of points of the voxel
    Eigen::Vector4f minbb;
    Eigen::Vector4f maxbb;

    /** \brief Empty constructor. */
    VoxelGridT()
        : leaf_size_(Eigen::Vector4f::Zero())
        , inverse_leaf_size_(Eigen::Array4f::Zero())
        , downsample_all_data_(true)
        , save_leaf_layout_(false)
        , leaf_layout_()
        , voxelSet()
        , min_b_(Eigen::Vector4i::Zero())
        , max_b_(Eigen::Vector4i::Zero())
        , div_b_(Eigen::Vector4i::Zero())
        , divb_mul_(Eigen::Vector4i::Zero())
        , filter_field_name_("")
        , filter_limit_min_(-FLT_MAX)
        , filter_limit_max_(FLT_MAX)
        , filter_limit_negative_(false)
    {
        filter_name_ = "VoxelGrid";
    }

    /** \brief Destructor. */
    virtual ~VoxelGridT()
    {
    }

    /** \brief Set the voxel grid leaf size.
        * \param[in] leaf_size the voxel grid leaf size
        */
    inline void setLeafSize(const Eigen::Vector4f &leaf_size)
    {
        leaf_size_ = leaf_size;
        // Avoid division errors
        if (leaf_size_[3] == 0)
            leaf_size_[3] = 1;
        // Use multiplications instead of divisions
        inverse_leaf_size_ = Eigen::Array4f::Ones() / leaf_size_.array();
    }

    /** \brief Set the voxel grid leaf size.
        * \param[in] lx the leaf size for X
        * \param[in] ly the leaf size for Y
        * \param[in] lz the leaf size for Z
        */
    inline void setLeafSize(float lx, float ly, float lz)
    {
        leaf_size_[0] = lx;
        leaf_size_[1] = ly;
        leaf_size_[2] = lz;
        // Avoid division errors
        if (leaf_size_[3] == 0)
            leaf_size_[3] = 1;
        // Use multiplications instead of divisions
        inverse_leaf_size_ = Eigen::Array4f::Ones() / leaf_size_.array();
    }

    /** \brief Get the voxel grid leaf size. */
    inline Eigen::Vector3f getLeafSize()
    {
        return (leaf_size_.head<3>());
    }

    /** \brief Set to true if all fields need to be downsampled, or false if just XYZ.
        * \param[in] downsample the new value (true/false)
        */
    inline void setDownsampleAllData(bool downsample)
    {
        downsample_all_data_ = downsample;
    }

    /** \brief Get the state of the internal downsampling parameter (true if
        * all fields need to be downsampled, false if just XYZ).
        */
    inline bool getDownsampleAllData()
    {
        return (downsample_all_data_);
    }

    /** \brief Set to true if leaf layout information needs to be saved for later access.
        * \param[in] save_leaf_layout the new value (true/false)
        */
    inline void setSaveLeafLayout(bool save_leaf_layout)
    {
        save_leaf_layout_ = save_leaf_layout;
    }

    /** \brief Returns true if leaf layout information will to be saved for later access. */
    inline bool getSaveLeafLayout()
    {
        return (save_leaf_layout_);
    }

    /** \brief Get the minimum coordinates of the bounding box (after
        * filtering is performed).
        */
    inline Eigen::Vector3i getMinBoxCoordinates()
    {
        return (min_b_.head<3>());
    }

    /** \brief Get the minimum coordinates of the bounding box (after
        * filtering is performed).
        */
    inline Eigen::Vector3i getMaxBoxCoordinates()
    {
        return (max_b_.head<3>());
    }

    /** \brief Get the number of divisions along all 3 axes (after filtering
        * is performed).
        */
    inline Eigen::Vector3i getNrDivisions()
    {
        return (div_b_.head<3>());
    }

    /** \brief Get the multipliers to be applied to the grid coordinates in
        * order to find the centroid index (after filtering is performed).
        */
    inline Eigen::Vector3i getDivisionMultiplier()
    {
        return (divb_mul_.head<3>());
    }

    /** \brief Returns the index in the resulting downsampled cloud of the specified point.
        *
        * \note for efficiency, user must make sure that the saving of the leaf layout is enabled and filtering
        * performed, and that the point is inside the grid, to avoid invalid access (or use
        * getGridCoordinates+getCentroidIndexAt)
        *
        * \param[in] p the point to get the index at
        */
    inline int getCentroidIndex(const PointInT &p)
    {
        return (leaf_layout_.at(
            (Eigen::Vector4i(static_cast<int>(floor(p.x * inverse_leaf_size_[0])),
                             static_cast<int>(floor(p.y * inverse_leaf_size_[1])),
                             static_cast<int>(floor(p.z * inverse_leaf_size_[2])), 0) -
             min_b_)
                .dot(divb_mul_)));
    }

    /** \brief Returns the indices in the resulting downsampled cloud of the points at the specified grid coordinates,
        * relative to the grid coordinates of the specified point (or -1 if the cell was empty/out of bounds).
        * \param[in] reference_point the coordinates of the reference point (corresponding cell is allowed to be empty/out of bounds)
        * \param[in] relative_coordinates matrix with the columns being the coordinates of the requested cells, relative to the reference point's cell
        * \note for efficiency, user must make sure that the saving of the leaf layout is enabled and filtering performed
        */
    inline std::vector<int> getNeighborCentroidIndices(const PointInT &reference_point,
                                                       const Eigen::MatrixXi &relative_coordinates)
    {
        Eigen::Vector4i ijk(static_cast<int>(floor(reference_point.x * inverse_leaf_size_[0])),
                            static_cast<int>(floor(reference_point.y * inverse_leaf_size_[1])),
                            static_cast<int>(floor(reference_point.z * inverse_leaf_size_[2])), 0);
        Eigen::Array4i diff2min = min_b_ - ijk;
        Eigen::Array4i diff2max = max_b_ - ijk;
        std::vector<int> neighbors(relative_coordinates.cols());
        for (int ni = 0; ni < relative_coordinates.cols(); ni++)
        {
            Eigen::Vector4i displacement =
                (Eigen::Vector4i() << relative_coordinates.col(ni), 0).finished();
            // checking if the specified cell is in the grid
            if ((diff2min <= displacement.array()).all() &&
                (diff2max >= displacement.array()).all())
                neighbors[ni] = leaf_layout_[(
                    (ijk + displacement - min_b_).dot(divb_mul_))];  // .at() can be omitted
            else
                neighbors[ni] = -1;  // cell is out of bounds, consider it empty
        }
        return (neighbors);
    }

    /** \brief Returns the layout of the leafs for fast access to cells relative to current position.
        * \note position at (i-min_x) + (j-min_y)*div_x + (k-min_z)*div_x*div_y holds the index of the element at coordinates (i,j,k) in the grid (-1 if empty)
        */
    inline std::vector<int> getLeafLayout()
    {
        return (leaf_layout_);
    }

    /** \brief Returns the corresponding (i,j,k) coordinates in the grid of point (x,y,z).
        * \param[in] x the X point coordinate to get the (i, j, k) index at
        * \param[in] y the Y point coordinate to get the (i, j, k) index at
        * \param[in] z the Z point coordinate to get the (i, j, k) index at
        */
    inline Eigen::Vector3i getGridCoordinates(float x, float y, float z)
    {
        return (Eigen::Vector3i(static_cast<int>(floor(x * inverse_leaf_size_[0])),
                                static_cast<int>(floor(y * inverse_leaf_size_[1])),
                                static_cast<int>(floor(z * inverse_leaf_size_[2]))));
    }

    //added from voxel grid occlusion estimation code in order to get the voxel coordinates not the ijk
    // it is important and used in the other codes
    inline Eigen::Vector4f getCentroidCoordinate(const Eigen::Vector3i &ijk)
    {
        int i, j, k;
        i = ((minbb[0] < 0) ? (abs(min_b_[0]) + ijk[0]) : (ijk[0] - min_b_[0]));
        j = ((minbb[1] < 0) ? (abs(min_b_[1]) + ijk[1]) : (ijk[1] - min_b_[1]));
        k = ((minbb[2] < 0) ? (abs(min_b_[2]) + ijk[2]) : (ijk[2] - min_b_[2]));

        Eigen::Vector4f xyz;
        xyz[0] = minbb[0] + (leaf_size_[0] * 0.5f) + (static_cast<float>(i) * leaf_size_[0]);
        xyz[1] = minbb[1] + (leaf_size_[1] * 0.5f) + (static_cast<float>(j) * leaf_size_[1]);
        xyz[2] = minbb[2] + (leaf_size_[2] * 0.5f) + (static_cast<float>(k) * leaf_size_[2]);
        xyz[3] = 0;
        return xyz;
    }

    /** \brief Returns the index in the downsampled cloud corresponding to a given set of coordinates.
        * \param[in] ijk the coordinates (i,j,k) in the grid (-1 if empty)
        */
    inline int getCentroidIndexAt(const Eigen::Vector3i &ijk)
    {
        int idx = ((Eigen::Vector4i() << ijk, 0).finished() - min_b_).dot(divb_mul_);
        if (idx < 0 ||
            idx >=
                static_cast<int>(
                    leaf_layout_
                        .size()))  // this checks also if leaf_layout_.size () == 0 i.e. everything was computed as needed
        {
            //if (verbose)
            //  PCL_ERROR ("[pcl::%s::getCentroidIndexAt] Specified coordinate is outside grid bounds, or leaf layout is not saved, make sure to call setSaveLeafLayout(true) and filter(output) first!\n", getClassName ().c_str ());
            return (-1);
        }
        return (leaf_layout_[idx]);
    }

    /** \brief Provide the name of the field to be used for filtering data. In conjunction with  \a setFilterLimits,
        * points having values outside this interval will be discarded.
        * \param[in] field_name the name of the field that contains values used for filtering
        */
    inline void setFilterFieldName(const std::string &field_name)
    {
        filter_field_name_ = field_name;
    }

    /** \brief Get the name of the field used for filtering. */
    inline std::string const getFilterFieldName()
    {
        return (filter_field_name_);
    }

    /** \brief Set the field filter limits. All points having field values outside this interval will be discarded.
        * \param[in] limit_min the minimum allowed field value
        * \param[in] limit_max the maximum allowed field value
        */
    inline void setFilterLimits(const double &limit_min, const double &limit_max)
    {
        filter_limit_min_ = limit_min;
        filter_limit_max_ = limit_max;
    }

    /** \brief Get the field filter limits (min/max) set by the user. The default values are -FLT_MAX, FLT_MAX.
        * \param[out] limit_min the minimum allowed field value
        * \param[out] limit_max the maximum allowed field value
        */
    inline void getFilterLimits(double &limit_min, double &limit_max)
    {
        limit_min = filter_limit_min_;
        limit_max = filter_limit_max_;
    }

    /** \brief Set to true if we want to return the data outside the interval specified by setFilterLimits (min, max).
        * Default: false.
        * \param[in] limit_negative return data inside the interval (false) or outside (true)
        */
    inline void setFilterLimitsNegative(const bool limit_negative)
    {
        filter_limit_negative_ = limit_negative;
    }

    /** \brief Get whether the data outside the interval (min/max) is to be returned (true) or inside (false).
        * \param[out] limit_negative true if data \b outside the interval [min; max] is to be returned, false otherwise
        */
    inline void getFilterLimitsNegative(bool &limit_negative)
    {
        limit_negative = filter_limit_negative_;
    }

    /** \brief Get whether the data outside the interval (min/max) is to be returned (true) or inside (false).
        * \return true if data \b outside the interval [min; max] is to be returned, false otherwise
        */
    inline bool getFilterLimitsNegative()
    {
        return (filter_limit_negative_);
    }

  protected:
    /** \brief The size of a leaf. */
    Eigen::Vector4f leaf_size_;

    /** \brief Internal leaf sizes stored as 1/leaf_size_ for efficiency reasons. */
    Eigen::Array4f inverse_leaf_size_;

    /** \brief Set to true if all fields need to be downsampled, or false if just XYZ. */
    bool downsample_all_data_;

    /** \brief Set to true if leaf layout information needs to be saved in \a leaf_layout_. */
    bool save_leaf_layout_;

    /** \brief The leaf layout information for fast access to cells relative to current position **/
    std::vector<int> leaf_layout_;

    /** \brief The minimum and maximum bin coordinates, the number of divisions, and the division multiplier. */
    Eigen::Vector4i min_b_, max_b_, div_b_, divb_mul_;

    /** \brief The desired user filter field name. */
    std::string filter_field_name_;

    /** \brief The minimum allowed filter value a point will be considered from. */
    double filter_limit_min_;

    /** \brief The maximum allowed filter value a point will be considered from. */
    double filter_limit_max_;

    /** \brief Set to true if we want to return the data outside (\a filter_limit_min_;\a filter_limit_max_). Default: false. */
    bool filter_limit_negative_;

    typedef typename pcl::traits::fieldList<PointInT>::type FieldList;

    /** \brief Downsample a Point Cloud using a voxelized grid approach
        * \param[out] output the resultant point cloud message
        */
    void applyFilter(PointCloud &output);
};
}  // namespace pcl
