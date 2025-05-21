#include <iostream>
#include <complex>
#include <vector>
#include "gfx.h"


#if defined(NO_DISPLAY)
#define DISPLAY(X)
#else
#define DISPLAY(X) X
#endif

// a very large value
const int max_iter = 50000;  

// size in pixels of the picture window
const int XSIZE = 1280;
const int YSIZE = 800;

/* Coordinates of the bounding box of the Mandelbrot set */
const double XMIN = -2.3, XMAX = 1.0;
const double SCALE = (XMAX - XMIN)*YSIZE / XSIZE;
const double YMIN = -SCALE/2, YMAX = SCALE/2;

struct pixel {
    int r, g, b;
};

const pixel colors[] = {
    { 66,  30,  15}, 
    { 25,   7,  26},
    {  9,   1,  47},
    {  4,   4,  73},
    {  0,   7, 100},
    { 12,  44, 138},
    { 24,  82, 177},
    { 57, 125, 209},
    {134, 181, 229},
    {211, 236, 248},
    {241, 233, 191},
    {248, 201,  95},
    {255, 170,   0},
    {204, 128,   0},
    {153,  87,   0},
    {106,  52,   3} };
const int NCOLORS = sizeof(colors)/sizeof(pixel);


//  z_(0)   = 0;
//  z_(n+1) = z_n * z_n + (cx + i*cy);
//
// iterates until ||z_n|| > 2, or max_iter
//
int iterate(const std::complex<double>& c) {
	std::complex<double> z=0;
	int iter = 0;
	while(iter < max_iter && std::abs(z) <= 2.0) {
		z = z*z + c;
		++iter;
	}
	return iter;
}

void drawpixel(int x, int y, int iter) {
	int r=0,g=0,b=0;
    if (iter < max_iter) {
		const int m= iter % NCOLORS;
		r = colors[m].r;
		g = colors[m].g;
		b = colors[m].b;
	}
	gfx_color(r, g, b);
    gfx_point(x, y);
}

void drawrowpixels(int y, const std::vector<int>& row) {
	int row_pixels[XSIZE];
	for (int x = 0; x < XSIZE; ++x) {
		int r = 0, g = 0, b = 0;
		if (row[x] < max_iter) {
			int m = row[x] % NCOLORS;
			r = colors[m].r;
			g = colors[m].g;
			b = colors[m].b;
		}
		// Construct a 24-bit pixel value (BGR ordering as used in gfx_color)
		row_pixels[x] = ((b & 0xff) | ((g & 0xff) << 8) | ((r & 0xff) << 16));
	}
	// Draw the entire row at once.
	gfx_draw_row(y, row_pixels, XSIZE);
}

// FastFlow part -----------
#include <ff/ff.hpp>
using namespace ff;

struct task_t {
	task_t(int y, int size):y(y),results(size) {}
	int y;
	std::vector<int> results;
};
struct Emitter: ff_node_t<task_t> {
	Emitter(std::vector<task_t>& tp):taskPool(tp) {}
	task_t* svc(task_t*) {
		for (int y = 0; y < YSIZE; ++y) {
			taskPool.emplace_back(y,XSIZE);
			auto task = &taskPool.back();
			ff_send_out(task);
		}
		return EOS;
	}
	std::vector<task_t>& taskPool;
};
struct Worker: ff_node_t<task_t> {
	task_t* svc(task_t* task) {
		const double im = YMAX - (YMAX - YMIN) * task->y / (YSIZE - 1);	   	  
		auto& results = task->results;
		for (int x = 0; x < XSIZE; ++x) {
            const double re = XMIN + (XMAX - XMIN) * x / (XSIZE - 1);
            results[x] = iterate(std::complex<double>(re, im));
		}
#if defined(NO_DISPLAY)
		return GO_ON;
#endif
		return task;
	}
};
struct Collector: ff_node_t<task_t> {
	task_t* svc(task_t* task) {
		drawrowpixels(task->y, task->results);
		return GO_ON;
	}
};


int main(int argc, char *argv[]) {
	int nworkers = ff_numCores();
	if (argc>1)
		nworkers=std::stol(argv[1]);
    DISPLAY(gfx_open(XSIZE, YSIZE, "Mandelbrot Set"));
	ffTime(START_TIME);
	std::vector<task_t> taskPool;
	taskPool.reserve(YSIZE);
	ff_farm farm;
	Emitter emitter(taskPool);
	farm.add_emitter(&emitter);
	std::vector<ff_node*> Workers;
	for(int i=0; i<nworkers;++i)
		Workers.push_back(new Worker);
	farm.add_workers(Workers);
	farm.set_scheduling_ondemand();
	DISPLAY(Collector collector);
	DISPLAY(farm.add_collector(&collector));
	farm.run_and_wait_end();
	ffTime(STOP_TIME);
	std::printf("Time: %f (ms)\n", ffTime(GET_TIME));
	DISPLAY(std::cout << "Click to finish\n"; gfx_wait());
}
