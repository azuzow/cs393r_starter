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
\file    particle-filter.cc
\brief   Particle Filter Starter Code
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include <algorithm>
#include <cmath>
#include <iostream>
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Geometry"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "shared/math/geometry.h"
#include "shared/math/line2d.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"

#include "config_reader/config_reader.h"
#include "particle_filter.h"

#include "vector_map/vector_map.h"

using geometry::line2f;
using std::cout;
using std::endl;
using std::string;
using std::swap;
using std::vector;
using Eigen::Vector2f;
using Eigen::Vector2i;
using vector_map::VectorMap;

DEFINE_double(num_particles, 50, "Number of particles");

namespace particle_filter {

  config_reader::ConfigReader config_reader_({"config/particle_filter.lua"});

  ParticleFilter::ParticleFilter() :
  prev_odom_loc_(0, 0),
  prev_odom_angle_(0),
  odom_initialized_(false) {}

  void ParticleFilter::GetParticles(vector<Particle>* particles) const {
    *particles = particles_;
  }

  void ParticleFilter::GetPredictedPointCloud(const Vector2f& loc,
    const float angle,
    int num_ranges,
    float range_min,
    float range_max,
    float angle_min,
    float angle_max,
    vector<Vector2f>* scan_ptr) {


    vector<Vector2f>& scan = *scan_ptr;

  // Compute what the predicted point cloud would be, if the car was at the pose
  // loc, angle, with the sensor characteristics defined by the provided
  // parameters.
  // This is NOT the motion model predict step: it is the prediction of the
  // expected observations, to be used for the update step.

  // Note: The returned values must be set using the `scan` variable:
    scan.resize(num_ranges);
  // Fill in the entries of scan using array writes, e.g. scan[i] = ...

  // .norm() of vector is magnitude
    // float magnitude = loc.norm();
    Eigen::Vector2f lazer_offset(cos(angle),sin(angle));
    lazer_offset=lazer_offset*0.2;
    Eigen::Vector2f lazer_loc = loc+lazer_offset;
    float angle_range = angle_max-angle_min;
    float angle_increment= angle_range/float(num_ranges);
    float current_ray_angle = angle-angle_range;
    for (size_t i = 0; i < scan.size(); ++i) {

      Eigen::Vector2f ray_start(cos(current_ray_angle),sin(current_ray_angle));
      Eigen::Vector2f ray_end(cos(current_ray_angle),sin(current_ray_angle));
      ray_start*=range_min;
      ray_end*=range_max;
      line2f current_ray(ray_start.x(), ray_start.y(), ray_end.x(), ray_end.y());
      current_ray_angle+=angle_increment;

      // The line segments in the map are stored in the `map_.lines` variable. You
      // can iterate through them as:
      for (size_t j = 0; j < map_.lines.size(); ++j) {
        const line2f map_line = map_.lines[j];
        Eigen::Vector2f closest_point(0,0);
        Eigen::Vector2f intersection_point(0,0);  
        bool intersects = map_line.Intersection(current_ray, &intersection_point);
        if (intersects) {

          float distance = sqrt(pow(intersection_point.x()-lazer_loc.x(),2)+pow(intersection_point.y()-lazer_loc.y(),2));
          float closest_point_distance = sqrt(pow(intersection_point.x()-closest_point.x(),2)+pow(intersection_point.y()-closest_point.y(),2));
          if(distance<closest_point_distance){
            closest_point=intersection_point;
          }
        } else {
          printf("No intersection\n");
        }

        scan[i]=closest_point;
      }
   // scan[i] = Vector2f(0, 0);
    }
  }

  void ParticleFilter::Update(const vector<float>& ranges,
    float range_min,
    float range_max,
    float angle_min,
    float angle_max,
    Particle* p_ptr) {
  // Implement the update step of the particle filter here.
  // You will have to use the `GetPredictedPointCloud` to predict the expected
  // observations for each particle, and assign weights to the particles based
  // on the observation likelihood computed by relating the observation to the
  // predicted point cloud.

    // void ParticleFilter::GetPredictedPointCloud(const Vector2f& loc,
    // const float angle,
    // int num_ranges,
    // float range_min,
    // float range_max,
    // float angle_min,
    // float angle_max,
    // vector<Vector2f>* scan_ptr)
    Particle& particle = *p_ptr;
    std::cout<<particle.loc.x()<<" "<< particle.loc.y()<<std::endl;
    std::vector<Eigen::Vector2f> predicted_pointCloud;
    GetPredictedPointCloud(particle.loc,particle.angle,ranges.size(),range_min,range_max,angle_min,angle_max,&predicted_pointCloud);
  }

  std::vector<Particle> ParticleFilter::Resample() {
  // Resample the particles, proportional to their weights.
  // The current particles are in the `particles_` variable. 
  // Create a variable to store the new particles, and when done, replace the
  // old set of particles:
  // vector<Particle> new_particles';
  // During resampling: 
  //    new_particles.push_back(...)
  // After resampling:
  // particles_ = new_particles;

  // You will need to use the uniform random number generator provided. For
  // example, to generate a random number between 0 and 1:

  //compute sum of all particle weights
  float totalWeightSum = 0;
  for(auto& particle: particles_){
    totalWeightSum += particle.weight;
  }


  unsigned int total_particles=FLAGS_num_particles;
  float weightSum = 0;
  std::vector<Vector2f> binSet;
  for(unsigned int i=0; i< total_particles; ++i){

      float start = weightSum;
      weightSum += particles_[i].weight;
      float end = weightSum / totalWeightSum;
      Vector2f bin = Vector2f(start,end);
      binSet.push_back(bin);
  }


  //new particle set
  vector<Particle> newParticles_;
  unsigned int j =0;

while(j  < total_particles){
  //pick a random number between 0 and 1
  float randNum = rng_.UniformRandom(0, 1);
  for(unsigned int i = 0; i  < total_particles; i++){
    if(binSet[i].x() < randNum && randNum < binSet[i].y() ){
      newParticles_.push_back(particles_[i]);
    }
  }
  j++;
}

 //   printf("Random number drawn from uniform distribution between 0 and 1: %f\n",
  //   x);

  return newParticles_;
  }

  void ParticleFilter::ObserveLaser(const vector<float>& ranges,
    float range_min,
    float range_max,
    float angle_min,
    float angle_max) {
  // A new laser scan observation is available (in the laser frame)
  // Call the Update and Resample steps as necessary.
  }
  void ParticleFilter::TransformParticle(Particle& particle,const Eigen::Vector2f& transform, const float& rotation,const float& k1,const float& k2,const float& k3,const float& k4){
  //k1 : translation error from translation
  //k2 : rotation error from translation
  //k3 : rotation error from rotation
  //k4 : translation error from rotation

  //k1 and k2 should be larger than k3 and k4 to create a oval that is longer along the x axis


    float magnitude_of_transform = sqrt((transform.x()*transform.x())+(transform.y()*transform.y()) );
    float magnitude_of_rotation = abs(rotation);
    //adding some constant to k1 and k2 to make the the probability density contour more oval like
    float x_translation_error_stdev= (k1)*magnitude_of_transform+ (k2)*magnitude_of_rotation;
    float y_translation_error_stdev= k1*magnitude_of_transform+ k2*magnitude_of_rotation;
    float rotation_error_stdev= k3*magnitude_of_transform+ k4*magnitude_of_rotation;

    // std::cout<<magnitude_of_transform<<magnitude_of_rotation<<x_translation_error_stdev<<y_translation_error_stdev<<rotation_error_stdev<<std::endl;
    float epsilon_x= rng_.Gaussian(0.0, x_translation_error_stdev);
    float epsilon_y= rng_.Gaussian(0.0, y_translation_error_stdev);
    float epsilon_theta=rng_.Gaussian(0.0, rotation_error_stdev);

    particle.loc.x() += transform.x()+ epsilon_x;
    particle.loc.y() += transform.y()+ epsilon_y;
    // not sure if this is correct either
    particle.angle+= rotation+epsilon_theta;

  }

  void ParticleFilter::Predict(const Vector2f& odom_loc,
   const float odom_angle) {
  // Implement the predict step of the particle filter here.
  // A new odometry value is available (in the odom frame)
  // Implement the motion model predict step here, to propagate the particles
  // forward based on odometry.

   for(auto& particle: particles_){

    Eigen::Rotation2Df rotation(-prev_odom_angle_);
    Eigen::Vector2f deltaTransformBaseLink=  rotation * (odom_loc-prev_odom_loc_) ;
    
    float deltaTransformAngle = odom_angle - prev_odom_angle_;


    TransformParticle(particle,deltaTransformBaseLink,deltaTransformAngle,0.1,0.1,0.1,0.1);
  }

  // You will need to use the Gaussian random number generator provided. For
  // example, to generate a random number from a Gaussian with mean 0, and
  // standard deviation 2:
  float x = rng_.Gaussian(0.0, 2.0);
  printf("Random number drawn from Gaussian distribution with 0 mean and "
   "standard deviation of 2 : %f\n", x);
}

void ParticleFilter::Initialize(const string& map_file,
  const Vector2f& loc,
  const float angle) {
  // The "set_pose" button on the GUI was clicked, or an initialization message
  // was received from the log. Initialize the particles accordingly, e.g. with
  // some distribution around the provided location and angle.

  unsigned int total_particles=FLAGS_num_particles;

  for(unsigned int i=0; i< total_particles; ++i){

    Particle particle;

    particle.loc.x() = loc.x()+ rng_.Gaussian(0.0, 0.05);
    particle.loc.y() = loc.y()+ rng_.Gaussian(0.0, 0.05);
      //angle within theta of 30
    particle.angle = angle+rng_.Gaussian(0.0, M_PI/6);
    particle.weight = (1.0)/FLAGS_num_particles;
    particles_.push_back(particle);
  }
  map_.Load(map_file);
}

void ParticleFilter::GetLocation(Eigen::Vector2f* loc_ptr, 
 float* angle_ptr) const {
  Vector2f& loc = *loc_ptr;
  float& angle = *angle_ptr;
  // Compute the best estimate of the robot's location based on the current set
  // of particles. The computed values must be set to the `loc` and `angle`
  // variables to return them. Modify the following assignments:
  loc = Vector2f(0, 0);
  angle = 0;
}


}  // namespace particle_filter


