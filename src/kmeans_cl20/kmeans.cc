#include <stdio.h>/* for printf */
#include <stdint.h>/* for uint64 definition */
#include <stdlib.h>/* for exit() definition */
#include <time.h>/* for clock_gettime */
#include <string.h>
#include <math.h>
#include <iostream>
#include <string>
#include <cassert>
#include <clUtil.h>

#include "kmeans.h"

#define BILLION 1000000000L

using namespace std;

KMEANS::KMEANS(){}

KMEANS::~KMEANS()
{	
	CleanUpKernels();
	// Free_mem()
}

void KMEANS::CleanUpKernels()
{
	checkOpenCLErrors(clReleaseKernel(kernel_s),
			"Failed to release kernel kernel_s");
	
	checkOpenCLErrors(clReleaseKernel(kernel2),
			"Failed to release kernel kernel2");
}

void KMEANS::map_feature_svm(int mode)
{
	// 0 : read
	// 1 : write

	if (mode == 1)
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_WRITE,  // to write
				              feature_svm,
				              bytes_pf,
				              0, 0, 0);
	}
	else
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_READ,  
				              feature_svm,
				              bytes_pf,
				              0, 0, 0);
	}
	
	checkOpenCLErrors(err, "Failed to clEnqueueSVMMap");
}



void KMEANS::map_feature_swap_svm(int mode)
{
	if (mode == 1)
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_WRITE,  // to write
				              feature_swap_svm,
				              bytes_pf,
				              0, 0, 0);
	}
	else
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_READ,  
				              feature_swap_svm,
				              bytes_pf,
				              0, 0, 0);
	}
	
	checkOpenCLErrors(err, "Failed to clEnqueueSVMMap");
	
}



void KMEANS::map_membership_svm(int mode)
{
	if (mode == 1)
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_WRITE,  // to write
				              membership_svm,
				              bytes_p,
				              0, 0, 0);
	}
	else
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_READ,  
				              membership_svm,
				              bytes_p,
				              0, 0, 0);
	}
	
	checkOpenCLErrors(err, "Failed to clEnqueueSVMMap");
	
}



void KMEANS::map_cluster_svm(int mode)
{
	if (mode == 1)
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_WRITE,  // to write
				              cluster_svm,
				              bytes_cf,
				              0, 0, 0);
	}
	else
	{
		err = clEnqueueSVMMap(cmd_queue,
				              CL_TRUE,       // blocking map
				              CL_MAP_READ,  
				              cluster_svm,
				              bytes_cf,
				              0, 0, 0);
	}
	
	checkOpenCLErrors(err, "Failed to clEnqueueSVMMap");
}


void KMEANS::unmap_feature_svm()
{
	err = clEnqueueSVMUnmap(cmd_queue, feature_svm, 0, 0, 0);
	checkOpenCLErrors(err, "Failed to clEnqueueSVMUnmap");	
}



void KMEANS::unmap_feature_swap_svm()
{
	err = clEnqueueSVMUnmap(cmd_queue, feature_swap_svm, 0, 0, 0);
	checkOpenCLErrors(err, "Failed to clEnqueueSVMUnmap");	
}



void KMEANS::unmap_membership_svm()
{
	err = clEnqueueSVMUnmap(cmd_queue, membership_svm, 0, 0, 0);
	checkOpenCLErrors(err, "Failed to clEnqueueSVMUnmap");	
}



void KMEANS::unmap_cluster_svm()
{
	err = clEnqueueSVMUnmap(cmd_queue, cluster_svm, 0, 0, 0);
	checkOpenCLErrors(err, "Failed to clEnqueueSVMUnmap");	
}



void KMEANS::Usage(char *argv0)
{
	const char *help =
		"\nUsage: %s [switches] -i filename\n\n"
		"    -i filename      :file containing data to be clustered\n"		
		"    -m max_nclusters :maximum number of clusters allowed    [default=5]\n"
		"    -n min_nclusters :minimum number of clusters allowed    [default=5]\n"
		"    -t threshold     :threshold value                       [default=0.001]\n"
		"    -l nloops        :iteration for each number of clusters [default=1]\n"
		"    -b               :input file is in binary format\n"
		"    -r               :calculate RMSE                        [default=off]\n"
		"    -o               :output cluster center coordinates     [default=off]\n";
	fprintf(stderr, help, argv0);
	exit(-1);
}







void KMEANS::CL_initialize()
{

	runtime    = clRuntime::getInstance();
	// OpenCL objects get from clRuntime class
	platform   = runtime->getPlatformID();
	context    = runtime->getContext();
	device     = runtime->getDevice();
	cmd_queue  = runtime->getCmdQueue(0);

}



void KMEANS::CL_build_program()
{
	// Helper to read kernel file
	file = clFile::getInstance();
	file->open("kmeans.cl");

	const char *source = file->getSourceChar();
	prog = clCreateProgramWithSource(context, 1, (const char **)&source, NULL, &err);
	checkOpenCLErrors(err, "Failed to create Program with source...\n");


	// Create program with OpenCL 2.0 support
	err = clBuildProgram(prog, 0, NULL, "-I ./ -cl-std=CL2.0", NULL, NULL);
	checkOpenCLErrors(err, "Failed to build program...\n");

}



void KMEANS::CL_create_kernels()
{
	// Create kernels
	kernel_s = clCreateKernel(prog, "kmeans_kernel_c", &err);
	checkOpenCLErrors(err, "Failed to create kmeans_kernel_c");

	kernel2 = clCreateKernel(prog, "kmeans_swap", &err);
	checkOpenCLErrors(err, "Failed to create kernel kmeans_swap");
}

/*
 * Create Device Memory Using SVM 
 */
void KMEANS::Create_mem_svm()
{
	bytes_cf = nclusters * nfeatures  * sizeof(float);


	feature_svm = (float *)clSVMAlloc(context, 
	                                  CL_MEM_READ_WRITE, 
									  bytes_pf, 0);
	if ( !feature_svm )
	{
		printf("Failed to allocate feature_svm.\n%s-%d\n",
				__FILE__, __LINE__);
		exit(-1);
	}

	feature_swap_svm = (float *)clSVMAlloc(context, 
	                                       CL_MEM_READ_WRITE, 
										   bytes_pf, 0);
	if ( !feature_swap_svm )
	{
		printf("Failed to allocate feature_swap_svm.\n%s-%d\n",
				__FILE__, __LINE__);
		exit(-1);
	}

	
	cluster_svm = (float *)clSVMAlloc(context, 
                                      CL_MEM_READ_WRITE, 
                                      bytes_cf, 
									  0);
	if ( !cluster_svm )
	{
		printf("Failed to allocate cluster_svm.\n%s-%d\n",
				__FILE__, __LINE__);
		exit(-1);
	}

	membership_svm = (int *)clSVMAlloc(context, CL_MEM_READ_WRITE, bytes_p, 0);
	if ( !membership_svm )
	{
		printf("Failed to allocate SVM memory : membership_svm.\n%s-%d\n",
				__FILE__, __LINE__);
		exit(-1);
	}

	membership_OCL = (int*) malloc(npoints * sizeof(int));
}


	

void KMEANS::Swap_features_svm()
{
	// copy feature to feature_svm
	map_feature_svm(1);
	memcpy(feature_svm, feature_1, bytes_pf);

//	cout << "feature svm[0-19] :";
//	for(int i=0; i<20; i++)
//	{
//		cout << feature_svm[i] << " ";	
//	} 	cout << endl;

	unmap_feature_svm();

//	map_feature_swap_svm(0);
//	cout << "feature swap svm[0-19] :";
//	for(int i=0; i<20; i++)
//	{
//		cout << feature_swap_svm[i] << " ";	
//	} 	cout << endl;
//	
//	unmap_feature_swap_svm();

//	cout << npoints << endl;
//	cout << nfeatures << endl;
//

	clSetKernelArgSVMPointer(kernel2, 0, feature_svm);
	clSetKernelArgSVMPointer(kernel2, 1, feature_swap_svm);
	clSetKernelArg(kernel2,           2, sizeof(cl_int), (void*) &npoints);
	clSetKernelArg(kernel2,           3, sizeof(cl_int), (void*) &nfeatures);	

	size_t global_work     = (size_t) npoints;
	size_t local_work_size = BLOCK_SIZE;

	if(global_work % local_work_size != 0)
		global_work = (global_work / local_work_size + 1) * local_work_size;

	err = clEnqueueNDRangeKernel(cmd_queue, 
	                             kernel2, 
								 1, 
								 NULL, 
								 &global_work, 
								 &local_work_size, 
								 0, 0, 0);
	checkOpenCLErrors(err, "ERROR: clEnqueueNDRangeKernel()");

//	map_feature_swap_svm(0);
//	cout << "feature swap svm[0-19] :";
//	for(int i=0; i<20; i++)
//	{
//		cout << feature_swap_svm[i] << " ";	
//	} 	cout << endl;
//
//	unmap_feature_swap_svm();
}



void KMEANS::Free_mem()
{
	clReleaseMemObject(d_feature);
	clReleaseMemObject(d_feature_swap);
	clReleaseMemObject(d_cluster);
	clReleaseMemObject(d_membership);

	free(membership_OCL);
}


void KMEANS::Free_mem_svm()
{
	clSVMFree(context, feature_svm);
	clSVMFree(context, feature_swap_svm);
	clSVMFree(context, cluster_svm);

	free(membership_OCL);
}

	

void KMEANS::Kmeans_ocl_svm()
{
	int i, j, k;

	size_t global_work     = (size_t) npoints;
	size_t local_work_size = BLOCK_SIZE2; 

	if(global_work % local_work_size !=0)
		global_work = (global_work / local_work_size + 1) * local_work_size;

	int size = 0; int offset = 0;

	// svm checking

	//map_cluster_svm(0);
	//cout << "clusters[0] :";                                                    
	//for(i=0; i<nfeatures; i++)                                                 
	//	cout << cluster_svm[i] << " ";                                          
	//cout << endl; 	
	//unmap_cluster_svm();

	//map_membership_svm(0);
	//cout << "membership_svm[0-29] :";                                                    
	//for(i=0; i<30; i++)                                                 
	//	cout << membership_svm[i] << " ";                                          
	//cout << endl; 	
	//unmap_membership_svm();

//	map_feature_swap_svm(0);
//	cout << "feature_swap_svm[0-30] :";                                                    
//	for(i=0; i<30; i++)                                                 
//		cout << feature_swap_svm[i] << " ";                                          
//	cout << endl; 	
//	unmap_feature_swap_svm();

	clSetKernelArgSVMPointer(kernel_s, 0, feature_swap_svm);
	clSetKernelArgSVMPointer(kernel_s, 1, cluster_svm);
	clSetKernelArgSVMPointer(kernel_s, 2, membership_svm);
	clSetKernelArg(kernel_s, 3, sizeof(cl_int), (void*) &npoints);
	clSetKernelArg(kernel_s, 4, sizeof(cl_int), (void*) &nclusters);
	clSetKernelArg(kernel_s, 5, sizeof(cl_int), (void*) &nfeatures);
	clSetKernelArg(kernel_s, 6, sizeof(cl_int), (void*) &offset);
	clSetKernelArg(kernel_s, 7, sizeof(cl_int), (void*) &size);

	err = clEnqueueNDRangeKernel(cmd_queue, 
	                             kernel_s, 
								 1, 
								 NULL, 
								 &global_work, 
								 &local_work_size, 
								 0, 0, 0);
	checkOpenCLErrors(err, "ERROR: clEnqueueNDRangeKernel(kernel_s)");

	clFinish(cmd_queue);

	// copy membership_svm to membership_OCL
	map_membership_svm(0); 
	memcpy(membership_OCL, membership_svm, bytes_p );
	unmap_membership_svm(); 

	//cout << "mem_ocl [0]: " << membership_OCL[0] << endl;

	// --------- //
	// fixme : use svm
	// --------- //
	//map_membership_svm(1); 
	map_feature_svm(0);

	int delta_tmp = 0;

	for (i = 0; i < npoints; i++)
	{
		int cluster_id = membership_OCL[i];

		new_centers_len[cluster_id]++;

		if (membership_OCL[i] != membership[i])
		{
			// update membership
			delta_tmp++;
			membership[i] = membership_OCL[i];
		}

		for (j = 0; j < nfeatures; j++)
		{
			new_centers[cluster_id][j] += feature_svm[i * nfeatures + j];
		}
	}

	unmap_feature_svm();
	// unmap_membership_svm();

	delta = (float) delta_tmp;
}




void KMEANS::Kmeans_clustering()
{
	int      i, j, n = 0;		// counters
	int		 loop=0, temp;
	int      initial_points = npoints;
	int		 c = 0;

	// nclusters should never be > npoints
	// that would guarantee a cluster without points
	if (nclusters > npoints) {
		nclusters = npoints;
	}

	// cout << nclusters << endl;

	// fixme : use svm 
	// allocate space for and initialize returning variable clusters[]
//	clusters    = (float**) malloc(nclusters *             sizeof(float*));
//	clusters[0] = (float*)  malloc(nclusters * nfeatures * sizeof(float));
//	for (i = 1; i < nclusters; i++) 
//	{
//		clusters[i] = clusters[i-1] + nfeatures;
//	}
//

	// initialize the random clusters
	initial = (int *) malloc (npoints * sizeof(int));
	for (i = 0; i < npoints; i++)
	{
		initial[i] = i;
	}

	//---------//
	// svm
	//---------//
	map_cluster_svm(1);
	map_feature_svm(0);


	// randomly pick cluster centers
	for (i = 0; i < nclusters && initial_points >= 0; i++) 
	{
		// svm
		for (j=0; j<nfeatures; j++)
		{
			cluster_svm[i * nfeatures + j] = feature_svm[initial[n] * nfeatures + j];
			//cout << cluster_svm[i * nfeatures + j] << " "; 
		}	//cout << endl;

		// swap the selected index to the end (not really necessary,
		// could just move the end up)
		temp                      = initial[n];
		initial[n]                = initial[initial_points-1];
		initial[initial_points-1] = temp;
		initial_points--;
		n++;
	}

	// initialize the membership to -1 for all
	for (i=0; i < npoints; i++)
		membership[i] = -1;
//	map_membership_svm(1);
//	for (i=0; i < npoints; i++) 
//	{
//		membership_svm[i] = -1;
//	}
//	unmap_membership_svm();


	//-----------//
	// svm 
	//-----------//
	unmap_cluster_svm();
	unmap_feature_svm();

	// allocate space for and initialize new_centers_len and new_centers
	new_centers_len = (int*) calloc(nclusters, sizeof(int));

	new_centers    = (float**) malloc(nclusters *            sizeof(float*));
	new_centers[0] = (float*)  calloc(nclusters * nfeatures, sizeof(float));
	for (i=1; i<nclusters; i++)
		new_centers[i] = new_centers[i-1] + nfeatures;

	// iterate until convergence
	do 
	{
		delta = 0.0;

		// return delta
		Kmeans_ocl_svm();

		//cout << "delta : " << delta << endl;

		//-----------//
		//fixme: svm
		//-----------//
		map_cluster_svm(1);

		// replace old cluster centers with new_centers
		// CPU side of reduction
		for (i=0; i<nclusters; i++) 
		{
			for (j=0; j<nfeatures; j++) 
			{
				if (new_centers_len[i] > 0)
				{	// take average i.e. sum/n 
					// clusters[i][j] = new_centers[i][j] / new_centers_len[i];

					// svm
					cluster_svm[i * nfeatures + j] = new_centers[i][j] / new_centers_len[i];
				}
				new_centers[i][j] = 0.0;	// set back to 0
			}
			new_centers_len[i] = 0;			// set back to 0 
		}	 

		unmap_cluster_svm();

		c++;

	} while ((delta > threshold) && (loop++ < 500));	// makes sure loop terminates
	

	printf("iterated %d times\n", c);

	free(new_centers[0]);
	free(new_centers);
	free(new_centers_len);

	// clusters is pointed to tmp_cluster_centres 
	// return clusters;
//	tmp_cluster_centres = clusters;

	// pass the data 
	map_cluster_svm(0); // map for read, remember to unmap
	tmp_cluster_centres_1 = cluster_svm;
}


// multi-dimensional spatial Euclid distance square
float KMEANS::euclid_dist_2(float *pt1, 
                            float *pt2)
{                                                                               
	int i;                                                                      
	float ans=0.0;                                                              

	for (i=0; i<nfeatures; i++)                                                   
		ans += (pt1[i]-pt2[i]) * (pt1[i]-pt2[i]);                               

	return(ans);                                                                
}



float KMEANS::euclid_dist_2_1(float *pt1, 
                            float *pt2)
{                                                                               
	int i;                                                                      
	float ans=0.0;                                                              

	for (i=0; i<nfeatures; i++)                                                   
		ans += (pt1[i]-pt2[i]) * (pt1[i]-pt2[i]);                               

	return(ans);                                                                
}



int KMEANS::find_nearest_point(float  *pt,           // [nfeatures]
                               float  **pts)         // [npts][nfeatures]
{                                                                               
	int index_local, i;                                                               
	float max_dist = FLT_MAX;                                                     

	// find the cluster center id with min distance to pt
	for (i=0; i<nclusters; i++) 
	{                                                    
		float dist;                                                             
		dist = euclid_dist_2(pt, pts[i]);  // no need square root
		if (dist < max_dist) 
		{                                                  
			max_dist = dist;                                                    
			index_local   = i;                                                       
		}                                                                       
	}                                                                           
	return(index_local);                                                              
}                                                                           



int KMEANS::find_nearest_point_1(float  *pt,           // [nfeatures]
                                 float  *pts)         // [npts][nfeatures]
{                                                                               
	int index_local, i;                                                               
	float max_dist = FLT_MAX;                                                     

	// find the cluster center id with min distance to pt
	for (i=0; i<nclusters; i++) 
	{                                                    
		float dist;                                                             
		dist = euclid_dist_2(pt, &pts[i * nfeatures]);  // no need square root
		if (dist < max_dist) 
		{                                                  
			max_dist = dist;                                                    
			index_local   = i;                                                       
		}                                                                       
	}                                                                           
	return(index_local);                                                              
}                                                                           



void KMEANS::RMS_err()
{
	int    i;                                                                   
	int   nearest_cluster_index;    // cluster center id with min distance to pt
	float  sum_euclid = 0.0;        // sum of Euclidean distance squares
	float  ret;                     // return value

	// pass data pointers
	float **feature_loc, **cluster_centres_loc; 	
	feature_loc = feature;
	cluster_centres_loc = tmp_cluster_centres;

	// calculate and sum the sqaure of euclidean distance
#pragma omp parallel for \
  shared(feature_loc, cluster_centres_loc) \
  firstprivate(npoints, nfeatures, nclusters) \
  private(i, nearest_cluster_index) \
  schedule (static)  

		for (i=0; i<npoints; i++) 
		{                                                 

			nearest_cluster_index = find_nearest_point(feature_loc[i],
					                                   cluster_centres_loc);

			
			sum_euclid += euclid_dist_2(feature_loc[i],                                 
					                    cluster_centres_loc[nearest_cluster_index]);
				
		}                                                                           

	// divide by n, then take sqrt
	rmse = sqrt(sum_euclid / npoints);                                           
}



void KMEANS::RMS_err_svm()
{
	int    i;                                                                   
	int   nearest_cluster_index;    // cluster center id with min distance to pt
	float  sum_euclid = 0.0;        // sum of Euclidean distance squares
	float  ret;                     // return value

	// pass data pointers
	float *feature_loc, *cluster_centres_loc; 	
	
	// svm : map for reading
	map_feature_svm(0);
	feature_loc = feature_svm;

	cluster_centres_loc = tmp_cluster_centres_1;

	// calculate and sum the sqaure of euclidean distance
#pragma omp parallel for \
  shared(feature_loc, cluster_centres_loc) \
  firstprivate(npoints, nfeatures, nclusters) \
  private(i, nearest_cluster_index) \
  schedule (static)  

		for (i=0; i<npoints; i++) 
		{                                                 

			nearest_cluster_index = find_nearest_point_1(&feature_loc[i * nfeatures],
					                                   cluster_centres_loc);

			
			sum_euclid += euclid_dist_2_1(&feature_loc[i * nfeatures],                                 
					                      &cluster_centres_loc[nearest_cluster_index * nfeatures]);
				
		}                                                                           

	// divide by n, then take sqrt
	rmse_1 = sqrt(sum_euclid / npoints);                                           
}





void KMEANS::Display_results_svm()
{
	int i, j;

	// cluster center coordinates : displayed only for when k=1
	if((min_nclusters == max_nclusters) && (isOutput == 1)) 
	{                   
		printf("\n================= Centroid Coordinates =================\n"); 
		for(i = 0; i < max_nclusters; i++){                                     
			printf("%d:", i);                                                   
			for(j = 0; j < nfeatures; j++){                                     
				printf(" %.2f", cluster_centres[i * nfeatures + j]);                         
			}                                                                   
			printf("\n\n");                                                     
		}                                                                       
	}                                                                           

	float	len = (float) ((max_nclusters - min_nclusters + 1)*nloops);                 

	printf("Number of Iteration: %d\n", nloops);                                
	//printf("Time for I/O: %.5fsec\n", io_timing);                             
	//printf("Time for Entire Clustering: %.5fsec\n", cluster_timing);          

	if(min_nclusters != max_nclusters)
	{                                         
		if(nloops != 1)
		{                                                        
			//range of k, multiple iteration                                    
			//printf("Average Clustering Time: %fsec\n",                        
			//      cluster_timing / len);                                      
			printf("Best number of clusters is %d\n", best_nclusters_1);          
		}                                                                       
		else
		{                                                                   
			//range of k, single iteration                                      
			//printf("Average Clustering Time: %fsec\n",                        
			//      cluster_timing / len);                                      
			printf("Best number of clusters is %d\n", best_nclusters_1);          
		}                                                                       
	}                                                                           
	else
	{                                                                       
		if(nloops != 1)
		{                                                        
			// single k, multiple iteration                                     
			//printf("Average Clustering Time: %.5fsec\n",                      
			//      cluster_timing / nloops);                                   
			if(isRMSE) 
			{// if calculated RMSE                                   
				printf("Number of trials to approach the best RMSE of %.3f is %d\n",
						min_rmse_1, index_1 + 1);
			}                                                                   
		}                                                                       
		else
		{                                                                   
			// single k, single iteration                                       
			if(isRMSE)
			{                                                         
				// if calculated RMSE                                           
				printf("Root Mean Squared Error: %.3f\n", min_rmse_1);                
			}                                                                   
		}                                                                       
	}                                                                           

}



void KMEANS::Clustering()
{
	// cluster_centres = NULL;
	// index =0;			    // number of iteration to reach the best RMSE

	cluster_centres_1 = NULL;
	index_1 = 0;

	// fixme
	membership = (int*) malloc(npoints * sizeof(int));



	//min_rmse_ref   = FLT_MAX;		
	min_rmse_ref_1 = FLT_MAX;		

	int i;


	// sweep k from min to max_nclusters to find the best number of clusters
	for(nclusters = min_nclusters; nclusters <= max_nclusters; nclusters++)
	{
		// cannot have more clusters than points
		if (nclusters > npoints) 
			break;	

		// allocate device memory, invert data array
		// Create_mem();
		//Swap_features();

		//---------------------//
		// svm :
		//      feature_svm, feature_swap_svm, cluster_svm, membership_OCL 
		//---------------------//
		Create_mem_svm();

		Swap_features_svm();

		// iterate nloops times for each number of clusters //
		for(i = 0; i < nloops; i++)
		{

			// kmeans clustering, return tmp_cluster_centres_1
			Kmeans_clustering();

			// when cluster_centres_1 is occupied, free it
			if (cluster_centres_1) 
			{
				free(cluster_centres_1);
			}

			cluster_centres_1 = tmp_cluster_centres_1;

			// save the last round for display
			if( (nclusters == max_nclusters) && (i == (nloops - 1)) )
			{
				cluster_centres = (float *) malloc (bytes_cf);		
				memcpy(cluster_centres, cluster_centres_1, bytes_cf);	
			}




			// find the number of clusters with the best RMSE //
			if(isRMSE)
			{
				RMS_err_svm();

		//		if(rmse < min_rmse_ref){
		//			min_rmse_ref = rmse;		//update reference min RMSE
		//			min_rmse = min_rmse_ref;	//update return min RMSE
		//			best_nclusters = nclusters;	//update optimum number of clusters
		//			index = i;			//update number of iteration to reach best RMSE
		//		}

				//svm
				if(rmse_1 < min_rmse_ref_1){
					min_rmse_ref_1 = rmse_1;		//update reference min RMSE
					min_rmse_1 = min_rmse_ref_1;	//update return min RMSE
					best_nclusters_1 = nclusters;	//update optimum number of clusters
					index_1 = i;			//update number of iteration to reach best RMSE
				}
			}			

			// remeber to unmap cluster_svm
			unmap_cluster_svm();
		}

		Free_mem_svm();
	}

	clSVMFree(context, membership_svm);
}



void KMEANS::Read(int argc, char **argv)
{
	// ------------------------- command line options -----------------------//
	int     opt;                                                                
	extern char   *optarg;                                                      
	isBinaryFile = 0;                                                       
	threshold = 0.001;          // default value
	max_nclusters=5;            // default value
	min_nclusters=5;            // default value
	isRMSE = 0;                                                             
	isOutput = 0;                                            
	nloops = 1;                 // default value

	char    line[1024];                                                         
	ssize_t ret; // add return value for read

	float  *buf;                                                                
	npoints = 0;                                                            
	nfeatures = 0;                                                          

	best_nclusters = 0;                                                     

	int i, j;

	// obtain command line arguments and change appropriate options
	while ( (opt=getopt(argc,argv,"i:t:m:n:l:bro"))!= EOF) {                    
		switch (opt) {                                                          
			case 'i': filename=optarg;                                          
					  break;                                                      
			case 'b': isBinaryFile = 1;                                         
					  break;                                                      
			case 't': threshold=atof(optarg);                                   
					  break;                                                      
			case 'm': max_nclusters = atoi(optarg);                             
					  break;                                                      
			case 'n': min_nclusters = atoi(optarg);                             
					  break;                                                      
			case 'r': isRMSE = 1;                                               
					  break;                                                      
			case 'o': isOutput = 1;                                             
					  break;                                                      
			case 'l': nloops = atoi(optarg);                                    
					  break;                                                      
			case '?': Usage(argv[0]);                                           
					  break;                                                      
			default: Usage(argv[0]);                                            
					 break;                                                      
		}                                                                       
	}                                                                       

	if (filename == 0) Usage(argv[0]);  


	//-----------------------------------------------------------------------//
	// Setup Opencl env
	//-----------------------------------------------------------------------//
	CL_initialize();
	CL_build_program();
	CL_create_kernels();

	//-----------------------------------------------------------------------//
	// SVM buffers 
	// 				feature, feature_swap, membership, clusters 
	//-----------------------------------------------------------------------//
	svmCoarseGrainAvail = runtime->isSVMavail(SVM_COARSE);
	svmFineGrainAvail   = runtime->isSVMavail(SVM_FINE);

	// runtime->displayDeviceInfo();
	// cout << svmCoarseGrainAvail << endl;
	// cout << svmFineGrainAvail << endl;


	// Need at least coarse grain
	if ( !svmCoarseGrainAvail )
	{
		printf("SVM coarse grain support unavailable\n");
		exit(-1);
	}
	

	// ============== I/O begin ==============//
	//io_timing = omp_get_wtime();
	if (isBinaryFile) 
	{	//Binary file input
		int infile;
		if ((infile = open(filename, O_RDONLY, "0600")) == -1) {
			fprintf(stderr, "Error: no such file (%s)\n", filename);
			exit(1);
		}

		ret = read(infile, &npoints, sizeof(int));

		if (ret == -1) {
			fprintf(stderr, "Error: failed to read, info: %s.%d\n", 
					__FILE__, __LINE__);
		}

		ret = read(infile, &nfeatures, sizeof(int));        
		if (ret == -1) {
			fprintf(stderr, "Error: failed to read, info: %s.%d\n", 
					__FILE__, __LINE__);
		}

		// bytes_pf = npoints * nfeatures * sizeof(float);

		//----------//
		// svmalloc
		//----------//
		//feature_svm = (float *)clSVMAlloc(context, CL_MEM_READ_WRITE, bytes_pf, 0);
		//if ( !feature_svm )
		//{
		//	printf("Cannot allocate SVM memory with clSVMAlloc.\n%s-%d\n",
		//	__FILE__, __LINE__);
		//	exit(-1);
		//}

		buf         = (float*) malloc(npoints*nfeatures*sizeof(float));
		feature_1   = (float*) malloc(npoints*nfeatures*sizeof(float));
		//feature     = (float**)malloc(npoints*          sizeof(float*));
		//feature[0]  = (float*) malloc(npoints*nfeatures*sizeof(float));

		//for (i=1; i<npoints; i++)
		//	feature[i] = feature[i-1] + nfeatures;

		ret = read(infile, buf, npoints*nfeatures*sizeof(float));

		if (ret == -1) {
			fprintf(stderr, "Error: failed to read, info: %s.%d\n", 
					__FILE__, __LINE__);
		}

		close(infile);
	}
	else 
	{
		FILE *infile;
		if ((infile = fopen(filename, "r")) == NULL) {
			fprintf(stderr, "Error: no such file (%s)\n", filename);
			exit(1);
		}		

		while (fgets(line, 1024, infile) != NULL) {
			if (strtok(line, " \t\n") != 0)
				npoints++;			
		}


		rewind(infile);

		while (fgets(line, 1024, infile) != NULL) {
			if (strtok(line, " \t\n") != 0) {
				// ignore the id (first attribute): nfeatures = 1;
				while (strtok(NULL, " ,\t\n") != NULL) nfeatures++;
				break;
			}
		}        


		//----------//
		// svmalloc
		//----------//
		//feature_svm = (float *)clSVMAlloc(context, CL_MEM_READ_WRITE, bytes_pf, 0);
		//if ( !feature_svm )
		//{
		//	printf("Failed to allocate SVM memory : feature_svm.\n%s-%d\n",
		//	__FILE__, __LINE__);
		//	exit(-1);
		//}

		// allocate space for features[] and read attributes of all objects
		buf         = (float*) malloc(npoints * nfeatures * sizeof(float));
		feature_1   = (float*) malloc(npoints*nfeatures*sizeof(float));
		//feature     = (float**)malloc(npoints*          sizeof(float*));
		//feature[0]  = (float*) malloc(npoints*nfeatures*sizeof(float));

		// fixme : svm buffer
		//for (i=1; i<npoints; i++)
		//	feature[i] = feature[i-1] + nfeatures;

		rewind(infile);

		i = 0;

		while (fgets(line, 1024, infile) != NULL) {

			if (strtok(line, " \t\n") == NULL) continue;            

			for (j=0; j<nfeatures; j++) {
				buf[i] = atof(strtok(NULL, " ,\t\n"));             
				i++;
			}            
		}

		fclose(infile);
	}

	bytes_pf = npoints * nfeatures * sizeof(float);
	bytes_p = sizeof(int) * npoints;

	//io_timing = omp_get_wtime() - io_timing;

	printf("\nI/O completed\n");
	printf("\nNumber of objects: %d\n", npoints);
	printf("Number of features: %d\n", nfeatures);
	
	// error check for clusters
	if (npoints < min_nclusters)
	{
		printf("Error: min_nclusters(%d) > npoints(%d) -- cannot proceed\n", 
				min_nclusters, npoints);
		exit(0);
	}

	// now features holds 2-dimensional array of features //
	//memcpy(feature[0], buf, npoints*nfeatures*sizeof(float));
	memcpy(feature_1, buf, npoints*nfeatures*sizeof(float));

	//---------------------//
	// svm map for writing
	//---------------------//
	//map_feature_svm(1);
	//memcpy(feature_svm, buf, bytes_pf);
	//unmap_feature_svm();



	free(buf);
}





void KMEANS::Run(int argc, char **argv)                                         
{                                                                               
	//--- read features and command line options ----//
	Read(argc, argv);                                                         

	//--- process clustering ---//
	//cluster_timing = omp_get_wtime();		// Total clustering time
	Clustering();
	//cluster_timing = omp_get_wtime() - cluster_timing;	

	Display_results_svm();	                                                                                
}    



int main( int argc, char** argv) 
{
  uint64_t diff;
  struct timespec start, end;

  clock_gettime(CLOCK_MONOTONIC, &start);/* mark start time */

	std::unique_ptr<KMEANS> kmeans(new KMEANS);

	printf("WG size of kernel_swap = %d, WG size of kernel_kmeans = %d \n", 
		BLOCK_SIZE, BLOCK_SIZE2);

	kmeans->Run(argc, argv);

	clock_gettime(CLOCK_MONOTONIC, &end);/* mark the end time */

	diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	printf("Total elapsed time = %llu nanoseconds\n", (long long unsigned int) diff);


	return 0;
}
