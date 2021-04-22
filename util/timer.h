#ifndef TIMER_H__
#define TIMER_H__

#include <cstdlib>
#include <unistd.h>
#include <ctime>

class Timer{
    public:
	Timer(){ };
	~Timer(){ };

	void start(){
	    clock_gettime(CLOCK_MONOTONIC, &t_start);
	}

	void end(){
	    clock_gettime(CLOCK_MONOTONIC, &t_end);
	}

	uint64_t get_time(){
	    return (t_end.tv_nsec - t_start.tv_nsec + (t_end.tv_sec - t_start.tv_sec)*1000000000);
	}
    private:
	struct timespec t_start, t_end;
};

#endif
