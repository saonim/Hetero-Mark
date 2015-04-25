#ifndef HMM_BKBETABHSALAUNCHER_H 
#define HMM_BKBETABHSALAUNCHER_H

#include "../common/HsaKernelLauncher.h"

class BkBetaBHsaLauncher : public HsaKernelLauncher
{

	// Arguments
	struct __attribute__((aligned(16))) args_t
	{
		uint64_t __global_offset_0;
		uint64_t __global_offset_1;
		uint64_t __global_offset_2;
		uint64_t __printf_buffer;
		uint64_t __vqueue_pointer;
		uint64_t __aqlwrap_pointer;
		uint32_t N;
		uint32_t pos;
		uint64_t *ret;
		void *beta;
		void *b;
		void *betaB;
	} args;

public:

	/**
	 * Constructor
	 */
	BkBetaBHsaLauncher(HsaHelper *helper) : 
		HsaKernelLauncher(helper) {};

	/**
	 * Init
	 */
	void Init() override
	{
		name = "&__OpenCL_BK_BetaB_kernel";
		timer->BeginTimer();
		memset(&args, 0, sizeof(args_t));
		timer->EndTimer({"CPU", "memory"});
		HsaKernelLauncher::Init();
	}

	/**
	 * Launcher kernel
	 */
	void LaunchKernel() override
	{
		arguments = &args;
		args.ret = new uint64_t();
		printf("Ret: 0x%016lx\n", *args.ret);
		for (int i = 0; i < args.N; i++)
		{
			float *ptr = (float *)args.betaB;
			ptr[i] = 0;
		}
		//printf("Launching fwd init alpha kernel\n");
		HsaKernelLauncher::LaunchKernel();
		printf("Ret: 0x%016lx\n", *args.ret);
	}

	/**
	 * Set argument
	 */
	void setArgument(int index, size_t size, void *value, 
			const char *option = NULL) override
	{
		timer->BeginTimer();
		switch (index) {
		case 0:
			memcpy(&args.N, value, size);
			break;
		case 1:
			memcpy(&args.pos, value, size);
			break;
		case 2:
			memcpy(&args.beta, value, size);
			break;
		case 3:
			memcpy(&args.b, value, size);
			break;
		case 4:
			memcpy(&args.betaB, value, size);
			printf("BetaB: %p, args.betaB: %p\n", *(void **)value, args.betaB);
			break;
		default:
			printf("Invalid argument index %d.\n", index);
			exit(1);
		}
		timer->EndTimer({"CPU", "memory"});
	}
};

#endif
