//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
\file    navigation.cc
\brief   Starter code for navigation.
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include "gflags/gflags.h"
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Geometry"
#include "amrl_msgs/AckermannCurvatureDriveMsg.h"
#include "amrl_msgs/Pose2Df.h"
#include "amrl_msgs/VisualizationMsg.h"
#include "glog/logging.h"
#include "ros/ros.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"
#include "shared/ros/ros_helpers.h"
#include "navigation.h"
#include "visualization/visualization.h"

using Eigen::Vector2f;
using amrl_msgs::AckermannCurvatureDriveMsg;
using amrl_msgs::VisualizationMsg;
using std::string;
using std::vector;

using namespace math_util;
using namespace ros_helpers;

namespace {
ros::Publisher drive_pub_;
ros::Publisher viz_pub_;
VisualizationMsg local_viz_msg_;
VisualizationMsg global_viz_msg_;
AckermannCurvatureDriveMsg drive_msg_;

// Epsilon value for handling limited numerical precision.
const float kEpsilon = 1e-5;
} //namespace

namespace navigation {

Navigation::Navigation(const string& map_file, ros::NodeHandle* n) :
    odom_initialized_(false),
    localization_initialized_(false),
    robot_loc_(0, 0),
    robot_angle_(0),
    robot_vel_(0, 0),
    robot_omega_(0),
    nav_complete_(true),
    nav_goal_loc_(0, 0),
    nav_goal_angle_(0),
    speed(0),
    max_speed(1),
    max_acceleration_magnitude(4),
    max_deceleration_magnitude(4) {
  drive_pub_ = n->advertise<AckermannCurvatureDriveMsg>(
      "ackermann_curvature_drive", 1);
  viz_pub_ = n->advertise<VisualizationMsg>("visualization", 1);
  local_viz_msg_ = visualization::NewVisualizationMessage(
      "base_link", "navigation_local");
  global_viz_msg_ = visualization::NewVisualizationMessage(
      "map", "navigation_global");
  InitRosHeader("base_link", &drive_msg_.header);

}

void Navigation::SetNavGoal(const Vector2f& loc, float angle) {
}

void Navigation::UpdateLocation(const Eigen::Vector2f& loc, float angle) {
  localization_initialized_ = true;
  robot_loc_ = loc;
  robot_angle_ = angle;
}

void Navigation::UpdateOdometry(const Vector2f& loc,
                                float angle,
                                const Vector2f& vel,
                                float ang_vel) {
  robot_omega_ = ang_vel;
  robot_vel_ = vel;
  if (!odom_initialized_) {
    odom_start_angle_ = angle;
    odom_start_loc_ = loc;
    odom_initialized_ = true;
    odom_loc_ = loc;
    odom_angle_ = angle;
    return;
  }
  odom_loc_ = loc;
  odom_angle_ = angle;
}

Eigen::Vector2f Navigation::get_robot_loc()
{
  std::cout << robot_loc_ << std::endl;
  return robot_loc_;
}

float Navigation::get_robot_angle()
{
  return robot_angle_;
}

void Navigation::ObservePointCloud(const vector<Vector2f>& cloud, double time) {
  point_cloud_ = cloud;
}

// void Navigation::calculate_distance_to_target(){
//   float min_distance = -1000000;
//   if (point_cloud_.size() == 0) {
//     // return -1;
// }
//   for (unsigned int i=0; i < point_cloud_.size(); i++)
//   {
//     float distance = sqrt( pow(point_cloud_[i][0], 2) + pow(point_cloud_[i][1], 2) );

//     if ( abs(point_cloud_[i][1]) < car_length + margin ) // Checking if i-th point is in straight line or not.
//     {
//       // std::cout << "Information about minimum point: Distance " << distance << " index: " << i << std::endl;
//       min_distance = distance;
//     }
//   }
//   // return min_distance;
// }


  void Navigation::updateSpeed(PathOption optimal_path){
  float x=robot_loc_.x();
  float y=robot_loc_.y();
  speed= sqrt(x*x + y*y);
  float distance = optimal_path.free_path_length;


  // time_needed_to_stop= (speed*speed)/max_deceleration_magnitude;
  std::cout<<"==================="<<std::endl;
  float distance_needed_to_stop= (speed*speed)/(2*max_deceleration_magnitude);
  std::cout<<"distance needed to stop "<<distance_needed_to_stop<<std::endl;
  std::cout<<"==================="<<std::endl;
  // distance_needed_to_cruise=(speed*speed)/(2*max_acceleration_magnitude);
  std::cout<<"distance remaining "<<distance<<std::endl;
  std::cout<<speed<<" "<<max_speed<<" "<<distance<<std::endl;
  if(speed<max_speed && distance>0  ){

    std::cout<<"accelerating"<<std::endl;
    drive_msg_.velocity=robot_omega_+max_acceleration_magnitude;
  }
  else if(speed>max_speed && distance>0){
    drive_msg_.velocity=max_speed;
  }
  else if(robot_omega_<=0 ){
    std::cout<<"stopped"<<std::endl;
    drive_msg_.velocity=0;
  }
  else if (distance_needed_to_stop>=distance){
    // decelerate
    std::cout<<speed<<"decelerating"<<std::endl;

  drive_msg_.velocity=robot_omega_-max_deceleration_magnitude;
  }
  else{
  // otherwise keep going max speed
    std::cout<<"cruising"<<std::endl;
  drive_msg_.velocity=max_speed;
  }
}

  //Distnace of  point from curvature is equal to the difference of the distance of the point from origin and the radius of curvature
float Navigation::findDistanceofPointfromCurve(float x, float y, float curvature){
  float radius = 1 / curvature;
  //use difference of distance formula of point from origin and radius to find distance of point from curve
  return abs(sqrt(pow(x,2) + pow(y,2)) - radius);
}

bool Navigation::isClockwise(float x, float y){
  float counterX = - y;
  float counterY = x;
  return (-x * counterY + y * counterX > 0);
}


bool Navigation::checkPoint(float angle, float curvature, float x, float y){
  float radius = 1 / curvature;
  float pointX = radius * cos(angle);
  float pointY = radius * sin(angle);
  return (x > pointX) && (y > pointY);
}

float Navigation::distanceAlongPath(float x, float y, float curvature){
  float xCoord = x;
  float yCoord = y;
  float radius = 1 / curvature;
  //radiusfromPoint = abs(sqrt(pow(xCoord,2) + pow((yCoord - radius),2)));
  float distancebetweenPoints = abs(sqrt(pow(xCoord,2) + pow((yCoord),2)));
  float theta = acos(1 - (pow(distancebetweenPoints, 2)/ (2 * pow(radius,2))));
  float length = radius * theta;
  return length;
}

Eigen::Vector2f Navigation::latency_compensation(const float& latency, unsigned int iterations)
{
    previous_velocities.push_back(robot_vel_);
    previous_locations.push_back(robot_loc_);
    previous_omegas.push_back(robot_omega_);
    previous_speeds.push_back(speed);
    previous_angles.push_back(robot_angle_);

    Eigen::Vector2f predicted_location(robot_loc_);

    if (previous_omegas.size()>iterations){
      previous_omegas.pop_front();
      previous_locations.pop_front();
      previous_velocities.pop_front();
      previous_speeds.pop_front();
      previous_angles.pop_front();
    }

    if (previous_omegas.size()== iterations){
    for (unsigned int i=0; i < iterations; i++)
    {
      predicted_location.x()= predicted_location.x()+ previous_speeds[i] * cos( previous_angles[i] )/20;
      predicted_location.y()= predicted_location.y() + previous_speeds[i] * sin( previous_angles[i] )/20;
    }
    // predicted_location=predicted_location*latency;
    std::cout<<"predicted_location"<<predicted_location.x()<<" "<<predicted_location.y()<<std::endl;
    std::cout<<"actual location"<<robot_loc_.x()<<" "<<robot_loc_.y()<<std::endl;
    }
    visualization::DrawCross(robot_loc_, 0.4, 0x32a852,global_viz_msg_);
    visualization::DrawCross(predicted_location, 0.2, 0xFF0000 ,global_viz_msg_);
    return predicted_location;
}


void Navigation::updatePointCloudToGlobalFrame()
{
  float x_p, y_p;
  unsigned int i;
  for (i=0; i < point_cloud_.size(); i++)
  {
    std::cout << point_cloud_[i] << "before" << std::endl;
    x_p = point_cloud_[i][0] * cos( -robot_angle_ ) - point_cloud_[i][1] * sin( -robot_angle_ ) - robot_loc_[0];
    y_p = point_cloud_[i][0] * sin( -robot_angle_ ) + point_cloud_[i][1] * cos( -robot_angle_ ) - robot_loc_[1];
    point_cloud_[i][0] = x_p;
    point_cloud_[i][1] = y_p;
    std::cout << "\n" << point_cloud_[i] << "after" << std::endl;
  }
  std::cout << robot_angle_ << std::endl;
}


// Change this function to give if collision is in innner part or outer part
float Navigation::check_if_collision(float curvature, Eigen::Vector2f& target_point, float inner_radius, float mid_radius, float outer_radius)
{
  // Returns 0 if no collision, 1 if collision on inner side of car, 2 if collision is on outer side of circle
  float x= 0;
  float y=1/curvature;
  // NOT SURE IF CENTER OF CIRCLE IS ALWAYS (0,R)
  float target_x=target_point[0];
  float target_y=target_point[1];
  float distance_from_center = sqrt(pow((x-target_x),2) + pow((y-target_y),2));
  if ( (distance_from_center<inner_radius) || (distance_from_center>outer_radius) )
  {
    return 0;
  }
  else if ( (distance_from_center >= inner_radius) && (distance_from_center <= mid_radius) )
  {
    return 1;
  }
  else if ( (distance_from_center >= mid_radius) && (distance_from_center <= outer_radius) )
  {
    return 2;
  }
  else
  {
    printf("Error in comparing distances");
    exit(0);
    return -1;
  }
}


// find nearest point in point cloud
float Navigation::findNearestPoint(float curvature, float angle){
  if (point_cloud_.size() == 0) return {};
  float radius = 1 /curvature;

  float minimumDistance = 10000;
  float innerRadius = .5 * radius;
  float outerRadius = 1.5 * radius;

    for(unsigned int i = 0; i < point_cloud_.size(); i++){
        float isInsideRange = check_if_collision(curvature, point_cloud_[i], innerRadius, (innerRadius + outerRadius)/2, outerRadius);
        if(isInsideRange == 0){
          if(checkPoint(angle, curvature, point_cloud_[i][0], point_cloud_[i][1])){
            float distance = findDistanceofPointfromCurve(point_cloud_[i][0] , point_cloud_[i][1], curvature);
            if(distance < minimumDistance){
              minimumDistance = distance;
              //nearestPoint.x = point_cloud_[i][0];
              //nearestPoint.y = point_cloud_[i][1];
          }
        }
    }
  }
  return minimumDistance;
}


Eigen::Vector2f  Navigation::findVectorOfNearestPoint(float curvature, float angle){
  if (point_cloud_.size() == 0) return {};
  float radius = 1 /curvature;
  Eigen::Vector2f  nearestPoint;
  float minimumDistance = 10000;
  float innerRadius = .5 * radius;
  float outerRadius = 1.5 * radius;

    for(unsigned int i = 0; i < point_cloud_.size(); i++){
        float isInsideRange = check_if_collision(curvature, point_cloud_[i], innerRadius, (innerRadius + outerRadius)/2, outerRadius);
        if(isInsideRange == 0){
          if(checkPoint(angle, curvature, point_cloud_[i][0], point_cloud_[i][1])){
            float distance = findDistanceofPointfromCurve(point_cloud_[i][0] , point_cloud_[i][1], curvature);
            if(distance < minimumDistance){
              minimumDistance = distance;
              nearestPoint.x() = point_cloud_[i][0];
              nearestPoint.y() = point_cloud_[i][1];
          }
        }
    }
  }
  return nearestPoint;
}


std::pair<float, float> Navigation::free_path_length_function(float curvature)
{
      //   find inner radius and outer radius
    const float MAX_LENGTH = 4;
    float r=1/curvature;
    float inner_radius = abs(r) - car_width/2 - margin;
    float outer_radius = sqrt( pow( abs(r)+ margin + car_width/2, 2) + pow( car_base_length + (car_length - car_base_length)/2 + margin, 2 ) );
    float mid_radius = sqrt( pow( abs(r) - car_width/2 - margin , 2) + pow( car_base_length + (car_length - car_base_length)/2 + margin, 2 ) );
    float collision = 0, collision_point_angle, total_angle, free_path_angle=0.0, free_path_length=0.0, min_free_path_length=0.0, min_free_path_angle=0.0;
    float x, y;
    min_free_path_length = 1000000;

    for (unsigned int i=0; i < point_cloud_.size(); i++)
    {
      x = point_cloud_[i][0], y = point_cloud_[i][1];
      if( y * r < 0 )
      {
        // point on opposite side of turning
        continue;
      }
      if (x < 0) continue; // points with angle>pi/2
      collision = check_if_collision(curvature, point_cloud_[i], inner_radius, mid_radius, outer_radius);
      if( collision == 0)
      {
        // No collision
        continue;
      }
      else if(collision == 1)
      {
        //inner collision
        collision_point_angle = acos( (( abs(r) - car_width/2 - margin ) / ( sqrt( pow(x, 2) + pow( y - r, 2 ) ) ) ) );
        total_angle = acos( ( (y-r)*(y-r) + r*r - y*y ) / ( 2*sqrt( x*x + (y-r)*(y-r) )*abs(r) ) );
        free_path_angle = total_angle - collision_point_angle;
        free_path_length = free_path_angle * abs(r);
        // min_free_path_length = std::min( min_free_path_length, free_path_length );
      }

      else if(collision == 2)
      {
        //outer collision
        collision_point_angle = asin( ( ( car_base_length + ( car_length  - car_base_length )/2 + margin ) / ( sqrt( pow( x, 2 ) + pow( y - r, 2 ) ) ) ) );
        total_angle = acos( ( (y-r)*(y-r) + r*r - y*y ) / ( 2*sqrt( x*x + (y-r)*(y-r) )*abs(r) ) );
        free_path_angle = total_angle - collision_point_angle;
        free_path_length = free_path_angle * abs(r);
        // min_free_path_length = std::min( min_free_path_length, free_path_length );
      }

      if (min_free_path_length > free_path_length)
      {
        min_free_path_length  = free_path_length;
        min_free_path_angle = free_path_angle;
      }
      // std::cout << "Minimum free path length" << min_free_path_length << " " <<  collision << " " << point_cloud_[i] << " " << i << " " << collision_point_angle << " " << total_angle << std::endl;
    }
    std::cout<<"===================="<<std::endl;
    std::cout<<"collision "<<collision<<std::endl;
    std::cout<<"===================="<<std::endl;
    std::cout << "Minimum free path length" << min_free_path_length << std::endl;
    if(min_free_path_length>MAX_LENGTH){
      min_free_path_length=MAX_LENGTH;
    }
    // if (min_free_path_length < 0)
    std::pair<float, float> min_free_path_variables;
    min_free_path_variables.first = min_free_path_length;
    min_free_path_variables.second = min_free_path_angle;
    return min_free_path_variables;
}



/*
float return_path_length()
{
  if collision at inner part
    find angle beta and free path length

  else if collision at outer part
    find angle beta and free path length
}
*/


void Navigation::DrawCar()
{
  Eigen::Vector2f front_left = Vector2f( car_base_length + (car_length - car_base_length) / 2 + margin, car_width/2 + margin );
  Eigen::Vector2f back_left = Vector2f( - (car_length - car_base_length) / 2 - margin, car_width/2 + margin );

  Eigen::Vector2f front_right = Vector2f( car_base_length + (car_length - car_base_length) / 2 + margin, -car_width/2 - margin );
  Eigen::Vector2f back_right = Vector2f( - (car_length - car_base_length) / 2 - margin, -car_width/2 - margin );

  visualization::DrawLine( front_left, back_left, 0x32a852,local_viz_msg_);
  visualization::DrawLine( front_right, back_right, 0x32a852,local_viz_msg_);

  visualization::DrawLine( front_left, front_right, 0x32a852,local_viz_msg_);
  visualization::DrawLine( back_left, back_right, 0x32a852,local_viz_msg_);
}


// float findBestCurvature()
// {
//   float best_curvature = 1;
//   free_path_length_function(best_curvature);
//   return best_curvature;
// }

PathOption Navigation::find_optimal_path(unsigned int total_curves, float min_curve, const Eigen::Vector2f target_point)
{

  float current_curvature=-1000.0;
  float current_free_path_length=-1000.0;
  float current_clearance=-1000.0;
  float current_free_path_angle=-1000.0;
  float current_distance_score=-10000;
  float max_score = -100000.0;

  float current_score=0;
  std::pair<float, float> free_path_length_angle;

  PathOption optimal_path;
  for(unsigned int i =0; i<total_curves;i++)
  {

    current_curvature =  min_curve + i*0.1;
    std::cout<<"curves "<<current_curvature<<std::endl;
    std::pair<float,float>free_path_pair= free_path_length_function( current_curvature );
    // first is length second is angle
    current_free_path_length = free_path_pair.first;
    current_free_path_angle = free_path_length_angle.second;

    current_clearance = findNearestPoint( current_curvature, current_free_path_angle );

    current_distance_score= findDistanceofPointfromCurve(target_point.x(),target_point.y(),current_curvature);

    current_score = current_free_path_length + current_clearance*3 + current_distance_score*-50;

    std::cout << current_free_path_length << " " << current_clearance << std::endl;

    visualization::DrawPathOption(current_curvature,current_free_path_length,current_clearance,local_viz_msg_);
    if ( max_score < current_score )
    {
  optimal_path.curvature=current_curvature;
  optimal_path.clearance=current_clearance;
  optimal_path.free_path_length=current_free_path_length;
  optimal_path.score=current_score;
    }
  }
  std::cout<<"OPTIMAL CURVE"<<optimal_path.curvature<<std::endl;
  return optimal_path;
}


void Navigation::Run() {
  // This function gets called 20 times a second to form the control loop.


  // Clear previous visualizations.
  visualization::ClearVisualizationMsg(local_viz_msg_);
  visualization::ClearVisualizationMsg(global_viz_msg_);

  // If odometry has not been initialized, we can't do anything.
  if (!odom_initialized_) return;

  Eigen::Vector2f test_cross1 = Vector2f( 1, 1 );
  Eigen::Vector2f test_cross2 = Vector2f( 2, 2 );

  visualization::DrawLine( test_cross1, test_cross2, 0x32a852, local_viz_msg_);
  // The control iteration goes here.
  // Feel free to make helper functions to structure the control appropriately.

  // The latest observed point cloud is accessible via "point_clouds_"

  // Eventually, you will have to set the control values to issue drive commands:
  // curvature=0;

  // predict location for when commands are executed

  //uncomment when we actually know how to transform points based on new location
  // Eigen::Vector2f predicted_location= latency_compensation(0.3, 6);
  // reconstruct point cloud based on predicted location and rotation

  // find best path based predicted location
  Eigen::Vector2f target_point{10,0};
  best_path= find_optimal_path(20, -1.0,target_point);

  // decide wether to speed up stay the same or slow down based on distance to target
      updateSpeed(best_path);
  // set trajectory for future time step
  drive_msg_.curvature = best_path.curvature;
  std::cout << "Robot variables:" << robot_loc_ << "\n Robot velocity: " << robot_vel_ << robot_angle_ << std::endl;
  std::cout << "Odom variables:" << odom_loc_ << "\n Odom angle: "  << odom_angle_ << "\n Odom start angle:" << odom_start_angle_ << odom_start_loc_ << std::endl;
  // if (point_cloud_set) {std::cout << "Yes, it worked" << point_cloud_.size() << std::endl;
  // }

  // updatePointCloudToGlobalFrame();
  std::cout<<"robot location"<<robot_loc_.x()<<" "<<robot_loc_.y()<<std::endl;
  visualization::DrawCross(robot_loc_, 3, 0x32a852,local_viz_msg_);
  DrawCar();





  // std::cout<<robot_loc_.x()<<" "<<robot_loc_.y()<<std::endl;




  // Add timestamps to all messages.
  local_viz_msg_.header.stamp = ros::Time::now();
  global_viz_msg_.header.stamp = ros::Time::now();

  drive_msg_.header.stamp = ros::Time::now();
  // Publish messages.
  viz_pub_.publish(local_viz_msg_);
  viz_pub_.publish(global_viz_msg_);
  drive_pub_.publish(drive_msg_);
  // exit(0);
}

}  // namespace navigation
