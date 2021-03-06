#define __NO_STD_VECTOR
#define MAX_SOURCE_SIZE (0x100000)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <CL/cl.h>

int main(int argc, const char * argv[])
{
  srand(time(NULL));
  clock_t c_main_start, c_main_stop, c_test_start, c_test_stop;
  c_main_start = clock();
  
  if (argc != 3) {
    printf("\nArgument missing!!\n\nArgument 1 specifies the mode - 1=Array Transfer, 2=Kernel Overhead, 3=Max Array\nArgument 2 specifi\
es the OCL mode - 1=1.2, 2=2.0\n");
    exit(0);
  }
  else {
    int runMode = atoi(argv[1]);
    int oclMode = atoi(argv[2]);
    
    if (runMode > 0 && runMode < 4 && oclMode > 0 && oclMode < 3) {
    
      FILE *cl_code = fopen("kernel.cl", "r");
      if (cl_code == NULL) { printf("\nerror: clfile\n"); return(1); }
      char *source_str = (char *)malloc(MAX_SOURCE_SIZE);
      fread(source_str, 1, MAX_SOURCE_SIZE, cl_code);
      fclose(cl_code);
      size_t source_length = strlen(source_str);

      cl_int err;
      cl_platform_id platform;
      cl_context context;
      cl_command_queue queue;
      cl_device_id device;
      cl_program program;

      err = clGetPlatformIDs(1, &platform, NULL);
      if (err != CL_SUCCESS) { printf("platformid %i", err); return 1; }

      err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
      if (err != CL_SUCCESS) { printf("deviceid %i", err); return 1; }

      context = clCreateContext(0, 1, &device, NULL, NULL, &err);
      if (err != CL_SUCCESS) { printf("createcontext %i", err); return 1; }

      queue = clCreateCommandQueue(context, device, NULL, &err);
      if (err != CL_SUCCESS) { printf("commandqueue %i", err); return 1; }

      program = clCreateProgramWithSource(context, 1, &source_str, &source_length, &err);
      if (err != CL_SUCCESS) { printf("createprogram %i", err); return 1; }

      switch (oclMode)
	{
	case 1:
	  err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
	  if (err != CL_SUCCESS) { printf("buildprogram ocl12 %i", err); }
	  break;
	case 2:
	  err = clBuildProgram(program, 1, &device, "-I ./ -cl-std=CL2.0", NULL, NULL);
	  if (err != CL_SUCCESS) { printf("buildprogram ocl20 %i", err); }
	  break;
	default:
	  return 1; break;
	}
      if (err == CL_BUILD_PROGRAM_FAILURE) {
	size_t log_size;
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
	char *log = (char *) malloc(log_size);
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
	printf("%s\n", log);
	return 1;
      }

      float diff = 0;
      int i;

      switch (runMode)
	{
	case 1:
	  printf("\nmemtime int copy test:");
	  int maxnum = 1;
	  for (int x = 100; x <= 1000; x+100)
	    {
	      printf("\nTest %i, %i objects", x, x*maxnum);
	      int *indata = (int *)malloc(sizeof(int)*x*maxnum);
	      int *outdata = (int *)malloc(sizeof(int)*x*maxnum);
	      for (i = 0; i < x*maxnum; i++) { indata[i] = rand(); }
	      if (oclMode == 1)
		{
		  c_test_start = clock();
		  cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, x*maxnum*sizeof(int), indata, &err);
		  if (err != CL_SUCCESS) { printf("createbuffer ocl12 %i", err); }
		  err = clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, x*maxnum*sizeof(int), outdata, 0, NULL, NULL);
		  if (err != CL_SUCCESS) { printf("readbuffer ocl12 %i", err); }
		  clFinish(queue);
		  c_test_stop = clock();
		  clReleaseMemObject(buffer);
		}
	      else if (oclMode == 2)
		{
		  c_test_start = clock();
		  int *svm = (int *)clSVMAlloc(context, CL_MEM_READ_WRITE, sizeof(int)*x*maxnum, 0);
		  err = clEnqueueSVMMap(queue, CL_TRUE, CL_MAP_WRITE, svm, sizeof(int)*x*maxnum, 0, 0, 0);
		  if (err != CL_SUCCESS) { printf("enqueuesvmmap ocl20 %i", err); }
		  for (i = 0; i < x*maxnum; i++) { memcpy(&svm[i], &indata[i], sizeof(int)); }
		  err = clEnqueueSVMUnmap(queue, svm, 0, 0, 0);
		  if (err != CL_SUCCESS) { printf("enqueueunmap ocl20 %i", err); }
		  err = clEnqueueSVMMap(queue, CL_TRUE, CL_MAP_READ, svm, sizeof(int)*x*maxnum, 0, 0, 0);
		  if (err != CL_SUCCESS) { printf("enqueusvmmap2 ocl20 %i", err); }
		  for (i = 0; i < x*maxnum; i++) { memcpy(&outdata[i], &svm[i], sizeof(int)); }
		  c_test_stop = clock();
		  clSVMFree(context, svm);
		}
	      for (i = 0; i < x*maxnum; i++) { if (indata[i] != outdata[i]) { printf("\nNote: Memory corruption occured during transfer(s)"); break; }}
	      diff = (((float)c_test_stop - (float)c_test_start) / CLOCKS_PER_SEC ) * 1000;
	      printf("\nTest %i done, time: %f ms", x, diff);
	    }
	  break;
	case 2:
	  printf("\nKernel start overhead test:\nSubtest 1 waits for kernel completion, subtest 2 uses the command queue");
	  printf("\nNote: this test is the same for OCL1.2 and 2.0");
	  for (int y = 1; y < 11; y++)
	    {
	      printf("\nTest %i, %i kernels spawned", y, y*100);
	      cl_kernel kernel = clCreateKernel(program, "CLRunner", &err);
	      if (err != CL_SUCCESS) { printf("createkernel %i", err); }
	      cl_event event;
	      const size_t local = 1;
	      const size_t global = 1;
	      c_test_start = clock();
	      for (int i = 0; i < y*100; i++)
		{
		  err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		  if (err != CL_SUCCESS) { printf("enqueuendrangekernel %i", err); }
		  clFinish(queue);
		}
	      c_test_stop = clock();
	      diff = (((float)c_test_stop - (float)c_test_start) / CLOCKS_PER_SEC ) * 1000;
	      printf("\n\tTest %i-1 done, time: %f ms", y, diff);
	      c_test_start = clock();
	      for (int i = 0; i < y*100; i++)
		{
		  err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		  if (err != CL_SUCCESS) { printf("enqueuendrangekernel %i", err); }
		}
	      clFinish(queue);
	      c_test_stop = clock();
	      diff = (((float)c_test_stop - (float)c_test_start) / CLOCKS_PER_SEC ) * 1000;
	      printf("\n\tTest %i-2 done, time: %f ms", y, diff);
	    }
	  break;
	case 3:
	  printf("\nMax Array Test");
	  unsigned long nx, ni;
	  if (oclMode == 1)
	    {
	      for (nx = 0; 1; nx++)
		{
		  printf("\nOCL1: %ix1000 integars", nx);
		  int *indata = (int *)malloc(sizeof(int)*nx*1000);
		  for (ni = 0; ni < nx*1000; ni++) { indata[ni] = rand(); }
		  cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, nx*1000*sizeof(int), indata, &err);
		  if (err != CL_SUCCESS) { printf("createbuffer ocl12 %i", err); }
		  clFinish(queue);
		  free(indata);
		  clReleaseMemObject(buffer);
		}
	    }
	  else if (oclMode == 2)
	    {
	      for (nx = 0; 1; nx++)
		{
		  printf("\nOCL2: %i x 1000 integars", nx);
		  int *svm = (int *)clSVMAlloc(context, CL_MEM_READ_WRITE, sizeof(int)*nx*1000, 0);

		  /* int *indata = (int *)malloc(sizeof(int)*nx*1000); */
		  /* int *outdata = (int *)malloc(sizeof(int)*nx*1000); */
		  /* err = clEnqueueSVMMap(queue, CL_TRUE, CL_MAP_WRITE, svm, sizeof(int)*nx*1000, 0, 0, 0); */
		  /* if (err != CL_SUCCESS) { printf("enqueuesvmmap ocl20 %i", err); } */
		  /* for (i = 0; ni < nx*1000; ni++) { memcpy(&svm[ni], &indata[ni], sizeof(int)); } */
		  /* err = clEnqueueSVMUnmap(queue, svm, 0, 0, 0); */
		  /* if (err != CL_SUCCESS) { printf("enqueueunmap ocl20 %i", err); } */
		  /* err = clEnqueueSVMMap(queue, CL_TRUE, CL_MAP_READ, svm, sizeof(int)*nx*1000, 0, 0, 0); */
		  /* if (err != CL_SUCCESS) { printf("enqueusvmmap2 ocl20 %i", err); } */
		  /* for (ni = 0; ni < nx*1000; ni++) { memcpy(&outdata[ni], &svm[ni], sizeof(int)); } */

		  clSVMFree(context, svm);
		}
	    }
	  break;
	default:
	  printf("\nError, unknown input function");
	  break;
	}
      clReleaseContext(context);
      clReleaseCommandQueue(queue);
      printf("\n");
    }

    if (runMode > 3 || runMode == 0) {
      printf("Argument 1 should be between 1-3, 1=Array Transfer, 2=Kernel Overhead, 3=Max Array\n");
    }
    if (oclMode == 0 || oclMode > 2) {
      printf("Argument 2 should be between 1-2, OCL mode - 1=1.2, 2=2.0\n");
    }
  }
}
