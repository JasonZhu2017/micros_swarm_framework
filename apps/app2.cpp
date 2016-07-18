/**
Software License Agreement (BSD)
\file      app2.cpp 
\authors Xuefeng Chang <changxuefengcn@163.com>
\copyright Copyright (c) 2016, the micROS Team, HPCL (National University of Defense Technology), All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the
   following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of micROS Team, HPCL, nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WAR-
RANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, IN-
DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <iostream>
#include "std_msgs/String.h"
#include "nav_msgs/Odometry.h"
#include "geometry_msgs/Twist.h"

#include "micros_swarm_framework/micros_swarm_framework.h"

#define BARRIER_VSTIG  1
#define ROBOT_SUM 20

#define RED_SWARM 1
#define BLUE_SWARM 2

#include <boost/any.hpp> 


namespace micros_swarm_framework{

    struct XY
    {
        float x;
        float y;
    };
    
    class App2 : public nodelet::Nodelet
    {
        public:
            ros::NodeHandle node_handle_;
            boost::shared_ptr<RuntimePlatform> rtp_;
            boost::shared_ptr<CommunicationInterface> communicator_;
            ros::Timer red_timer_;
            ros::Timer blue_timer_;
            ros::Publisher pub_;
            ros::Subscriber sub_;
            
            //app parameters
            int delta_kin;
            int epsilon_kin;
            int delta_nonkin;
            int epsilon_nonkin;
            
            App2();
            ~App2();
            virtual void onInit();
            
            //app functions
            float force_mag_kin(float dist);
            float force_mag_nonkin(float dist);
            XY force_sum_kin(micros_swarm_framework::NeighborBase n, XY &s);
            XY force_sum_nonkin(micros_swarm_framework::NeighborBase n, XY &s);
            XY direction_red();
            XY direction_blue();
            bool red(int id);
            bool blue(int id);
            void motion_red();
            void motion_blue();
            void publish_red_cmd(const ros::TimerEvent&);
            void publish_blue_cmd(const ros::TimerEvent&);
            void baseCallback(const nav_msgs::Odometry& lmsg);  
    };

    App2::App2()
    {
        //set parameters
        delta_kin = 5;
        epsilon_kin = 100;

        delta_nonkin = 30;
        epsilon_nonkin = 1000;
    }
    
    App2::~App2()
    {
    }
    
    float App2::force_mag_kin(float dist)
    {
        return -(epsilon_kin/(dist+0.1)) *(pow(delta_kin/(dist+0.1), 4) - pow(delta_kin/(dist+0.1), 2));
    }

    float App2::force_mag_nonkin(float dist)
    {
        return -(epsilon_nonkin/(dist+0.1)) *(pow(delta_nonkin/(dist+0.1), 4) - pow(delta_nonkin/(dist+0.1), 2));
    }

    XY App2::force_sum_kin(micros_swarm_framework::NeighborBase n, XY &s)
    {
        micros_swarm_framework::Base l=rtp_->getRobotBase();
        float xl=l.getX();
        float yl=l.getY();
    
        float xn=n.getX();
        float yn=n.getY();
    
        float dist=sqrt(pow((xl-xn),2)+pow((yl-yn),2));
    
        float fm = force_mag_kin(dist)/1000;
        if(fm>0.5) fm=0.5;
    
        float fx=(fm/(dist+0.1))*(xn-xl);
        float fy=(fm/(dist+0.1))*(yn-yl);
    
        s.x+=fx;
        s.y+=fy;
        return s;
    }

    XY App2::force_sum_nonkin(micros_swarm_framework::NeighborBase n, XY &s)
    {
        micros_swarm_framework::Base l=rtp_->getRobotBase();
        float xl=l.getX();
        float yl=l.getY();
    
        float xn=n.getX();
        float yn=n.getY();
    
        float dist=sqrt(pow((xl-xn),2)+pow((yl-yn),2));
    
        float fm = force_mag_nonkin(dist)/1000;
        if(fm>0.5) fm=0.5;
    
        float fx=(fm/(dist+0.1))*(xn-xl);
        float fy=(fm/(dist+0.1))*(yn-yl);
    
        s.x+=fx;
        s.y+=fy;
        return s;
    }

    XY App2::direction_red()
    {
        XY sum;
        sum.x=0;
        sum.y=0;
    
        micros_swarm_framework::Neighbors<micros_swarm_framework::NeighborBase> n(true);
        boost::function<XY(NeighborBase, XY &)> bf_kin=boost::bind(&App2::force_sum_kin, this, _1, _2);
        boost::function<XY(NeighborBase, XY &)> bf_nonkin=boost::bind(&App2::force_sum_nonkin, this, _1, _2);
        sum=n.neighborsKin(RED_SWARM).neighborsReduce(bf_kin, sum);
        sum=n.neighborsNonKin(RED_SWARM).neighborsReduce(bf_nonkin, sum);
    
        return sum;
    }

    XY App2::direction_blue()
    {
        XY sum;
        sum.x=0;
        sum.y=0;
    
        micros_swarm_framework::Neighbors<micros_swarm_framework::NeighborBase> n(true);
        boost::function<XY(NeighborBase, XY &)> bf_kin=boost::bind(&App2::force_sum_kin, this, _1, _2);
        boost::function<XY(NeighborBase, XY &)> bf_nonkin=boost::bind(&App2::force_sum_nonkin, this, _1, _2);
        sum=n.neighborsKin(BLUE_SWARM).neighborsReduce(bf_kin, sum);
        sum=n.neighborsNonKin(BLUE_SWARM).neighborsReduce(bf_nonkin, sum);
    
        return sum;
    }

    bool App2::red(int id)
    {
        if(id<=9)
            return true;
        return false;
    }

    bool App2::blue(int id)
    {
        if(id>=10)
            return true;
        return false;
    }
    
    void App2::publish_red_cmd(const ros::TimerEvent&)
    {
        
        XY v=direction_red();
        geometry_msgs::Twist t;
        t.linear.x=v.x;
        t.linear.y=v.y;
        
        pub_.publish(t);
        
    }
    
    void App2::publish_blue_cmd(const ros::TimerEvent&)
    {
        
        XY v=direction_blue();
        geometry_msgs::Twist t;
        t.linear.x=v.x;
        t.linear.y=v.y;
        
        pub_.publish(t);
        
    }

    void App2::motion_red()
    {
        red_timer_ = node_handle_.createTimer(ros::Duration(0.1), &App2::publish_red_cmd, this);
    }

    void App2::motion_blue()
    {
        blue_timer_ = node_handle_.createTimer(ros::Duration(0.1), &App2::publish_blue_cmd, this);
    }
    
    void App2::baseCallback(const nav_msgs::Odometry& lmsg)
    {
        float x=lmsg.pose.pose.position.x;
        float y=lmsg.pose.pose.position.y;
    
        float vx=lmsg.twist.twist.linear.x;
        float vy=lmsg.twist.twist.linear.y;
    
        micros_swarm_framework::Base l(x, y, 0, vx, vy, 0);
        rtp_->setRobotBase(l);
    }
    
    void App2::onInit()
    {
        node_handle_ = getNodeHandle();
        rtp_=Singleton<RuntimePlatform>::getSingleton();
        #ifdef ROS
        communicator_=Singleton<ROSCommunication>::getSingleton();
        #endif
        #ifdef OPENSPLICE_DDS
        communicator_=Singleton<OpenSpliceDDSCommunication>::getSingleton();
        #endif
    
        sub_ = node_handle_.subscribe("base_pose_ground_truth", 1000, &App2::baseCallback, this, ros::TransportHints().udp());
        pub_ = node_handle_.advertise<geometry_msgs::Twist>("cmd_vel", 1000);
        
        boost::function<bool()> bfred=boost::bind(&App2::red, this, rtp_->getRobotID());
        boost::function<bool()> bfblue=boost::bind(&App2::blue, this, rtp_->getRobotID());
    
        micros_swarm_framework::Swarm red_swarm(RED_SWARM);
        red_swarm.selectSwarm(bfred);
        micros_swarm_framework::Swarm blue_swarm(BLUE_SWARM);
        blue_swarm.selectSwarm(bfblue);
    
        red_swarm.printSwarm();
        blue_swarm.printSwarm();
        
        red_swarm.execute(boost::bind(&App2::motion_red, this));
        blue_swarm.execute(boost::bind(&App2::motion_blue, this));
        
        //micros_swarm_framework::VirtualStigmergy<bool> barrier(1);
        //std::string robot_id_string=boost::lexical_cast<std::string>(rtp_->getRobotID());
        //barrier.virtualStigmergyPut(robot_id_string, 1);
    }
};

// Register the nodelet
PLUGINLIB_EXPORT_CLASS(micros_swarm_framework::App2, nodelet::Nodelet)
