//
// Created by kevin on 3/19/18.
//

#ifndef EXPLORE_LARGE_MAP_MAP_BUILDER_HPP
#define EXPLORE_LARGE_MAP_MAP_BUILDER_HPP

#include <chrono>
#include <cmath> // For std::exp.
#include <exception>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "Eigen/Dense"
#include "Eigen/Geometry"
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>


#include <ros/ros.h>
#include <iv_slam_ros_msgs/TraversibleArea.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/OccupancyGrid.h>
#include <tf/transform_datatypes.h>
#include <tf/transform_broadcaster.h>
#include <visualization_msgs/MarkerArray.h>

#include "explore_large_map/rigid_transform.h"
#include "explore_large_map/transform.h"

namespace explore_global_map {
    inline tf::Transform mapToWorld (const geometry_msgs::Pose &initial_pose) {
        tf::Transform world_to_map;
        tf::poseMsgToTF (initial_pose, world_to_map);
        return world_to_map;
    }

    inline tf::Transform worldToMap (const geometry_msgs::Pose &initial_pose) {
        return mapToWorld(initial_pose).inverse();
    }


    inline void geographic_to_grid(double &longitude, double &latitude) {
        double a=6378137;
        double e2= 0.0818192*0.0818192;

        cartographer::transform::GridZone zone =cartographer::transform:: UTM_ZONE_51;
        cartographer::transform::Hemisphere hemi = cartographer::transform:: HEMI_NORTH;
        double N = 0;
        double E = 0;
        cartographer::transform:: geographic_to_grid( a, e2, latitude * M_PI / 180, longitude * M_PI / 180,
                                                      &zone, &hemi,&N, &E);
        latitude = N;
        longitude = E;
    }

    inline geometry_msgs::Quaternion adjustRPYConvention(const geometry_msgs::Quaternion &origin) {
        tf::Quaternion q;
        tf::quaternionMsgToTF(origin, q);
        tf::Matrix3x3 m(q);
        double roll, pitch, yaw;
        m.getRPY(roll, pitch, yaw);
        // exchange roll and pitch, cuz different convention
        double temp = roll;
        roll = pitch;
        pitch = temp;
        // ivrc convention, north is 0 degree, counter clockwise is positive
        yaw += M_PI_2;
        if(yaw > M_PI) yaw -= 2 * M_PI;
        if(yaw < -M_PI) yaw += 2* M_PI;
        tf::Quaternion q_new = tf::createQuaternionFromRPY(roll, pitch, yaw);
        geometry_msgs::Quaternion msg;
        tf::quaternionTFToMsg(q_new, msg);
        return msg;
    }

    inline void counterClockwiseRotatePoint(double ref_x, double ref_y, double angle,
                                            double &point_x, double &point_y) {
        double s = sin(angle);
        double c = cos(angle);

        // translate point back to origin:
        point_x -= ref_x;
        point_y -= ref_y;

        // rotate point
        double xnew = point_x * c - point_y * s;
        double ynew = point_x * s + point_y * c;

        // translate point back:
        point_x = xnew + ref_x;
        point_y = ynew + ref_y;
}

    class MapBuilder {

    public:
        MapBuilder(double width, double height, double resolution);

        void grow(const nav_msgs::Odometry &vehicle_pose,
                  const nav_msgs::OccupancyGrid &local_map);

        nav_msgs::OccupancyGrid getMap() const {return map_;}
        nav_msgs::Odometry getPositionInOdomMap() const {return vehicle_pose_in_odom_map_;}

    private:

        void tailorSubmap(const iv_slam_ros_msgs::TraversibleArea &traver_map, iv_slam_ros_msgs::TraversibleArea &tailored_submap);

        void broadcastTransformBetweenVehicleAndExploreMap(geometry_msgs::Pose &current_pose);
        void broadcastTransformBetweenExploreMapAndOdom();

        void publishFootPrint(const geometry_msgs::Pose &pose, const std::string &frame);

        void updatePointOccupancy(bool use_bayes, bool occupied, size_t idx, std::vector<int8_t>& occupancy,
                                              std::vector<double>& log_odds) const;

        bool start_flag_;

        int tailored_submap_width_;
        int tailored_submap_height_;
        int tailored_submap_x2base_;

        double global_map_width_, global_map_height_;
        double initial_x_, initial_y_;
        geometry_msgs::Pose initial_vehicle_pos_in_odom;
        geometry_msgs::Pose initial_vehicle_pos_in_explore_map;
        geometry_msgs::Pose current_odom_vehicle_pos_;
        nav_msgs::Odometry vehicle_pose_in_odom_map_;
        tf::TransformBroadcaster br_;

        nav_msgs::OccupancyGrid map_; //!< local map with fixed orientation

        ros::NodeHandle private_nh_;
        ros::Publisher vehicle_footprint_pub_;

        int unknown_value_;
        std::string global_map_frame_name_, local_map_frame_name_, abso_global_map_frame_name_;

        double border_thickness_;

        // for probability update parameters
        double p_occupied_when_laser_;  //!< Probability that a point is occupied
        //!< when the laser ranger says so.
        //!< Defaults to 0.9.
        double p_occupied_when_no_laser_ ;  //!< Probability that a point is
        //!< occupied when the laser ranger
        //!< says it's free.
        //!< Defaults to 0.3.
        double large_log_odds_;  //!< Large log odds used with probability 0 and 1.
        //!< The greater, the more inertia.
        //!< Defaults to 100.
        double max_log_odds_for_belief_;  //!< Max log odds used to compute the
        //!< belief (exp(max_log_odds_for_belief)
        //!< should not overflow).
        //!< Defaults to 20.

        std::vector<double> log_odds_;  //!< log odds ratios for the binary Bayes filter
        //!< log_odd = log(p(x) / (1 - p(x)))

    };
} // namespace explore_global_map

#endif //EXPLORE_LARGE_MAP_MAP_BUILDER_HPP
