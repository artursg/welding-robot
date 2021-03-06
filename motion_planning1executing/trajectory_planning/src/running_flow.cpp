#include <running_flow.h>


//All the Rviz_TrajectoryPoses
vector< geometry_msgs::Pose > Rviz_TrajectoryPose;
vector< pcl::PointXYZ > all_realsense_position;

vector< geometry_msgs::Pose > trajectory_6DOF_generation(pcl::PointXYZ singleFrame_realsense_position, 
                                                         Cloud::Ptr cloud_ptr, 
                                                         Cloud::Ptr cloud_ptr_modelSeam, 
  
                                                         bool &trajectoryPlanning_flag, 
                                                         int  &receive_capture_count,

                                                         sensor_msgs::PointCloud2 pub_pointcloud, 
                                                         ros::Publisher pointcloud_publisher, 
                                                         ros::Publisher Welding_Trajectory_publisher,
                                                         
                                                         ros::Rate naptime)
{
  if(trajectoryPlanning_flag == true)
  {
    trajectoryPlanning_flag = false;
    receive_capture_count++;

    input_pointcloud_filter(cloud_ptr);

    welding_seam_location(cloud_ptr, 
                          singleFrame_realsense_position, 
                          pub_pointcloud, 
                          pointcloud_publisher);

    integrate_allsingle_pointcloudFrame(cloud_ptr, 
                                        cloud_ptr_modelSeam, 
                                        pub_pointcloud, 
                                        pointcloud_publisher);

    all_realsense_position.push_back(singleFrame_realsense_position);
    cout << "all_realsense_position: " << all_realsense_position.size() << endl;
    Rviz_TrajectoryPose = trajectory_planning(cloud_ptr_modelSeam, 
                                              all_realsense_position,
                                              pub_pointcloud, 
                                              pointcloud_publisher,   
                                              Welding_Trajectory_publisher,
                                              naptime);
  }

  return Rviz_TrajectoryPose;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool welding_seam_location( Cloud::Ptr cloud_ptr, 
                            pcl::PointXYZ realsense_position, 
                            sensor_msgs::PointCloud2 pub_pointcloud, 
                            ros::Publisher pointcloud_publisher)
{
  float seam_detection_radius = 0.01;
  clock_t begin = clock();

  int show_Pointcloud_timeMax = 100;
 

   PointCloud::Ptr cloud_ptr_show (new PointCloud);

  cout << "要不要处理这帧点云"  << endl;
  bool process_flag = processing_frame_ornot( cloud_ptr, 
                                              show_Pointcloud_timeMax, 
                                              cloud_ptr_show, 
                                              pub_pointcloud, 
                                              pointcloud_publisher);
  if(!process_flag)
  {
    cloud_ptr->points.clear();
    bool no;
    return no;
  }


  //Seam Location:    
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << endl << "1.读入原始pointcloud" << endl << endl;
  // Cloud::Ptr cloud_ptr    = read_pointcloud(seam_detection_radius, cloud_ptr_show);
  SurfaceProfile_Reconstruction(seam_detection_radius, 
                                cloud_ptr, 
                                cloud_ptr_show);
  Cloud::Ptr cloud_ptr_origin = cloud_ptr_origin_copy(cloud_ptr);

  show_pointcloud_Rviz(show_Pointcloud_timeMax,   
                        cloud_ptr_show, 
                        pub_pointcloud, 
                        pointcloud_publisher);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "2.算出所有点的法向量" << endl << endl;
  Point3f Cam_Position; 
  Cam_Position.x = realsense_position.x; 
  Cam_Position.y = realsense_position.y; 
  Cam_Position.z = realsense_position.z; 
  vector<Point3f> Normal = PointNormal_Computation(seam_detection_radius, 
                                                   cloud_ptr, 
                                                   cloud_ptr_show, 
                                                   Cam_Position);
  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "3.分割出所有可能的焊接缝" << endl << endl;
  Delete_SmoothChange_Plane(seam_detection_radius, 
                            cloud_ptr, 
                            cloud_ptr_show, 
                            Normal, 
                            pub_pointcloud, 
                            pointcloud_publisher);

  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "4.人工筛选出目标焊接缝" << endl << endl;
  Screen_Candidate_Seam(cloud_ptr, 
                        cloud_ptr_show, 
                        pub_pointcloud, 
                        pointcloud_publisher);  // geometry_msgs::Pose path_point;
  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);
    

  
  cout << endl;
  clock_t end = clock();
  double elapsed_secs = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
  cout << elapsed_secs << " s" << endl;
}


vector< geometry_msgs::Pose > trajectory_planning(Cloud::Ptr cloud_ptr_modelSeam,
                                                  vector< pcl::PointXYZ > all_realsense_position,
                                                  sensor_msgs::PointCloud2 pub_pointcloud, 
                                                  ros::Publisher pointcloud_publisher,
                                                  ros::Publisher Welding_Trajectory_publisher,
                                                  ros::Rate naptime)
{
  cout << endl<< endl<< endl << "Generate the welding seam or not!!!!!!!!!! yes or xxxx ";
  string flag;
  cin >> flag;
  bool process_flag = false;
  vector< geometry_msgs::Pose > no;
  if(flag != "yes")
  {
    return no;
  }


  int show_Pointcloud_timeMax = 100;
  PointCloud::Ptr cloud_ptr_show (new PointCloud);

  //Trajectory Planning:
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "5.提取焊接缝边界" << endl << endl;
  Cloud::Ptr seam_edge = Extract_Seam_edge(cloud_ptr_modelSeam, cloud_ptr_show);
  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);  
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "6.焊接缝三维轨迹点" << endl << endl; 
  Cloud::Ptr PathPoint_Position = PathPoint_Position_Generation(seam_edge, 
                                                                cloud_ptr_modelSeam, 
                                                                cloud_ptr_show);
  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  cout << "7.焊接缝轨迹点的焊枪方向" << endl << endl;
  vector<Point3f> Torch_Normal_Vector = PathPoint_Orientation_Generation(PathPoint_Position, 
                                                                         cloud_ptr_modelSeam, 
                                                                         cloud_ptr_show, 
                                                                         all_realsense_position);
  show_pointcloud_Rviz(show_Pointcloud_timeMax, 
                       cloud_ptr_show, 
                       pub_pointcloud, 
                       pointcloud_publisher);
 
  // //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // cout << "8.生成最终6DOF轨迹" << endl << endl;
  vector< geometry_msgs::Pose > Welding_Trajectory ;
  vector< geometry_msgs::Pose > Rviz_TrajectoryPose = Ultimate_6DOF_TrajectoryGeneration(Welding_Trajectory, 
                                                                                         PathPoint_Position, 
                                                                                         Torch_Normal_Vector);

  if(Welding_Trajectory.size() > 0)
  {
    for(int i = 0; i < Welding_Trajectory.size(); i++)
    {
      Welding_Trajectory_publisher.publish(Welding_Trajectory[i]);
      naptime.sleep();
    }
    cout << "3D path is published !!!!!!!!!" << endl;
  }


  return Rviz_TrajectoryPose;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void input_pointcloud_filter(Cloud::Ptr cloud_ptr)
{
  ////start filter
  Cloud::Ptr Cloud_filtered (new Cloud);

  float radius = 0.001;

  pcl::VoxelGrid<pcl::PointXYZ> voxel;
  voxel.setLeafSize( radius, radius, radius );
  voxel.setInputCloud( cloud_ptr );
  voxel.filter( *Cloud_filtered );

  cout << "input pointcloud filtering done!!!" << endl;

  ////////////////////////////////////////////////////////////////
  cloud_ptr->points.clear();
  for(float i = 0; i < Cloud_filtered->points.size(); i++)
  {
    pcl::PointXYZ p;
    p.x = Cloud_filtered->points[i].x;
    p.y = Cloud_filtered->points[i].y;
    p.z = Cloud_filtered->points[i].z;
    cloud_ptr->points.push_back( p );    
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void integrate_allsingle_pointcloudFrame(Cloud::Ptr cloud_ptr, 
                                         Cloud::Ptr cloud_ptr_modelSeam, 
                                         sensor_msgs::PointCloud2 pub_pointcloud, 
                                         ros::Publisher pointcloud_publisher)
{
  int show_Pointcloud_timeMax = 100;

  for(float i = 0; i < cloud_ptr->points.size(); i++)
  {
    pcl::PointXYZ p;
    p.x = cloud_ptr->points[i].x; 
    p.y = cloud_ptr->points[i].y;
    p.z = cloud_ptr->points[i].z;
    cloud_ptr_modelSeam->points.push_back( p );    
  }
  cloud_ptr->points.clear();

   
  Cloud::Ptr Cloud_filtered (new Cloud);

  float radius = 0.001;

  pcl::VoxelGrid<pcl::PointXYZ> voxel;
  voxel.setLeafSize( radius, radius, radius );
  voxel.setInputCloud( cloud_ptr_modelSeam );
  voxel.filter( *Cloud_filtered );

  cloud_ptr_modelSeam->points.clear();
  for(float i = 0; i < Cloud_filtered->points.size(); i++)
  {
    pcl::PointXYZ p;
    p.x = Cloud_filtered->points[i].x; 
    p.y = Cloud_filtered->points[i].y;
    p.z = Cloud_filtered->points[i].z;
    cloud_ptr_modelSeam->points.push_back( p );    
  }


  PointCloud::Ptr cloud_ptr_show (new PointCloud);
  for(float i = 0; i < cloud_ptr_modelSeam->points.size(); i++)
  {
    pcl::PointXYZRGB p;
    p.x = cloud_ptr_modelSeam->points[i].x;
    p.y = cloud_ptr_modelSeam->points[i].y;
    p.z = cloud_ptr_modelSeam->points[i].z;
    p.b = 200; 
    p.g = 200;
    p.r = 200;
    cloud_ptr_show->points.push_back( p );    
  }
  show_pointcloud_Rviz(show_Pointcloud_timeMax, cloud_ptr_show, pub_pointcloud, pointcloud_publisher);
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int count_pointcloud_frameNum(string dataset_folder_path)
{
  DIR *dp;
	struct dirent *dirp;

	if((dp = opendir(dataset_folder_path.c_str())) == NULL)
		cout << "Can't open " << dataset_folder_path << endl;

  int count = 0;
	while((dirp = readdir(dp)) != NULL)
    count++;
		// cout << dirp->d_name << endl;

	closedir(dp);

  cout << "pointcloud_frameNum: " << (count-2)/2 <<endl;

  return (count-2)/2;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void build_model_pointcloud(string dataset_folder_path, 
                            int pointcloud_frameNum,
                            PointCloud::Ptr model_pointcloud)
{
  Cloud::Ptr cloud_ptr_all (new Cloud);

  for (int receive_capture_count = 1; receive_capture_count <= pointcloud_frameNum; receive_capture_count++)
  {
    ostringstream stream;
    stream << receive_capture_count;
    string ith_frame = stream.str();

    string file_pointcloud_frame;
    file_pointcloud_frame = dataset_folder_path + "/" + ith_frame + "_frame.pcd";

    Cloud::Ptr cloud_ptr (new Cloud);   
    PointCloud::Ptr model_cloud_all (new PointCloud);
    
    pcl::PCDReader reader;
    reader.read(file_pointcloud_frame, *cloud_ptr);

    for(float i = 0; i < cloud_ptr->points.size(); i++)
    {
      pcl::PointXYZ p;
      p.x = cloud_ptr->points[i].x; 
      p.y = cloud_ptr->points[i].y;
      p.z = cloud_ptr->points[i].z;
      cloud_ptr_all->points.push_back( p );    
    }
  }

  /////////////////////////////////////////////////////////////////
  Cloud::Ptr Cloud_filtered (new Cloud);

  float radius = 0.005;

  pcl::VoxelGrid<pcl::PointXYZ> voxel;
  voxel.setLeafSize( radius, radius, radius );
  voxel.setInputCloud( cloud_ptr_all );
  voxel.filter( *Cloud_filtered );

  cout << "input pointcloud filtering done!!!" << endl;

  ////////////////////////////////////////////////////////////////

//   pcl::search::KdTree<pcl::PointXYZ>::Ptr ec_tree (new pcl::search::KdTree<pcl::PointXYZ>);
//   ec_tree->setInputCloud (Cloud_filtered);//创建点云索引向量，用于存储实际的点云信息

//   std::vector<pcl::PointIndices> cluster_indices;
//   pcl::EuclideanClusterExtraction<pcl::PointXYZ> EC;

//   EC.setClusterTolerance (radius*2); //设置近邻搜索的搜索半径为2cm
//   EC.setMinClusterSize (100);//设置一个聚类需要的最少点数目为100
//   EC.setMaxClusterSize (10000000); //设置一个聚类需要的最大点数目为25000
//   EC.setSearchMethod (ec_tree);//设置点云的搜索机制
//   EC.setInputCloud (Cloud_filtered);// input cloud
//   EC.extract (cluster_indices);//从点云中提取聚类，并将点云索引保存在cluster_indices中
  
//   cout << "ec_tree_cloud->points.size(): "  << Cloud_filtered->points.size() << endl ;
//   cout << "cluster_indices.size(): " << cluster_indices.size() << endl;
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//   // put largest cluster into final seam cloud
//   Cloud::Ptr cloud_cluster (new Cloud);
//   for (vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it)
//   {
//     for (std::vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); ++pit)
//     {
//       cloud_cluster->points.push_back (Cloud_filtered->points[*pit]);  
//     }
//     break;
//   }
 
//////////////////////////////////////////////////////////////////////

  for(float i = 0; i < Cloud_filtered->points.size(); i++)
  {
    pcl::PointXYZRGB p;
    p.x = Cloud_filtered->points[i].x;
    p.y = Cloud_filtered->points[i].y;
    p.z = Cloud_filtered->points[i].z;
    p.b = 200;
    p.g = 200;
    p.r = 200;
    model_pointcloud->points.push_back( p );    
  }
  
  cout << "model_pointcloud->points.size(): "  << model_pointcloud->points.size() << endl << endl ;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pcl::PointXYZ read_realtime_pointcloud_frame( string dataset_folder_path,
                                              int pointcloud_frameNum,
                                              int receive_capture_count,
                                              int &process_frame_count,
                                              bool &trajectoryPlanning_flag,
                                              Cloud::Ptr cloud_ptr)
{
  pcl::PointXYZ no;
  if(receive_capture_count > pointcloud_frameNum)
  {
    if(receive_capture_count > process_frame_count)
    {
      cout << endl << "process_frame_count:" << process_frame_count << "   no more pointcloud data!" << endl;
      process_frame_count = process_frame_count + 100;
    }

    return no;
  }

  pcl::PointXYZ realsense_position;
  
  ostringstream stream;
  stream << receive_capture_count;
  string ith_frame = stream.str();

  string file_realsense_position;
  file_realsense_position = dataset_folder_path + "/" + ith_frame + "_frame.txt";

  ifstream fin(file_realsense_position.c_str());
  if(fin)
  {
    string s;
    getline(fin,s);

    int _1_T = s.find("\t", 0); //从哪个位置开始搜索x
    int _2_T = s.find("\t", _1_T+1);   

    string cam_x = s.substr(0, _1_T);
    stringstream ss_cam_x;
    ss_cam_x<<cam_x;
    ss_cam_x>>realsense_position.x;

    string cam_y   = s.substr(_1_T+1, _2_T-_1_T-1);
    stringstream ss_cam_y;
    ss_cam_y<<cam_y;
    ss_cam_y>>realsense_position.y;

    string cam_z   = s.substr(_2_T+1, s.length()-_2_T-1);
    stringstream ss_cam_z;
    ss_cam_z << cam_z;
    ss_cam_z >> realsense_position.z;

    cout << "read " << process_frame_count << "th pointcloud frame" << endl;
    cout << "realsense_position" << realsense_position << endl << endl;

    process_frame_count++;
    trajectoryPlanning_flag = true;
  }
  
  string file_pointcloud_frame;
  file_pointcloud_frame = dataset_folder_path + "/" + ith_frame + "_frame.pcd";

  pcl::PCDReader reader;
  reader.read(file_pointcloud_frame, *cloud_ptr);

  return realsense_position;
}


 


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void show_pointcloud_Rviz(int show_Pointcloud_timeMax, 
                          PointCloud::Ptr show_Rviz_cloud, 
                          sensor_msgs::PointCloud2 pub_pointcloud, 
                          ros::Publisher pointcloud_publisher)
{
  for(float i = 0; i < show_Pointcloud_timeMax; i++)
  {
    pcl::toROSMsg(*show_Rviz_cloud, pub_pointcloud);
    pub_pointcloud.header.frame_id = "base_link";
    pub_pointcloud.header.stamp = ros::Time::now();
    pointcloud_publisher.publish(pub_pointcloud);
  }
  show_Rviz_cloud->points.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void publish_pointcloud_Rviz(string coordinate, 
                             PointCloud::Ptr pointloud, 
                             sensor_msgs::PointCloud2 pub_pointcloud, 
                             ros::Publisher pointcloud_publisher)
{
    pcl::toROSMsg(*pointloud, pub_pointcloud);
    pub_pointcloud.header.frame_id = coordinate;// 
    pub_pointcloud.header.stamp = ros::Time::now();
    pointcloud_publisher.publish(pub_pointcloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool processing_frame_ornot(Cloud::Ptr cloud_ptr, 
                            int show_Pointcloud_timeMax, 
                            PointCloud::Ptr cloud_ptr_show, 
                            sensor_msgs::PointCloud2 pub_pointcloud, 
                            ros::Publisher pointcloud_publisher)
{
  cloud_ptr_show->clear();
  for(float i = 0; i < cloud_ptr->points.size(); i++)
  {
    pcl::PointXYZRGB p;
    p.x = cloud_ptr->points[i].x;
    p.y = cloud_ptr->points[i].y;
    p.z = cloud_ptr->points[i].z;
    p.b = 200; 
    p.g = 200;
    p.r = 200;
    cloud_ptr_show->points.push_back( p );    
  }
  show_pointcloud_Rviz(show_Pointcloud_timeMax, cloud_ptr_show, pub_pointcloud, pointcloud_publisher);


  cout << endl << "process the pointcloud or not? yes or xxxx ";
  string flag;
  cin >> flag;
  bool process_flag = false;
  if(flag == "yes")
  {
    process_flag = true;
  }
  cloud_ptr_show->clear();
  cout << endl;
  
  return process_flag;
}
