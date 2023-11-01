#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <complex>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

// Import standard library components
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::complex;
using std::cout;
using std::endl;
using std::ofstream;
using std::thread;
using std::vector;
using std::mutex;
using std::unique_lock;
using std::condition_variable;

// Define alias for clock type
typedef std::chrono::steady_clock the_clock;

// Image size constants
const int WIDTH = 1920;
const int HEIGHT = 1200;
const int MAX_ITERATIONS = 500;

// Image data as a 2D array
uint32_t image[HEIGHT][WIDTH];

mutex image_mutex;
condition_variable cv;
int active_threads = 0;

// Function to write image data to TGA file
void write_tga(const char* filename)
{
	ofstream outfile(filename, ofstream::binary);

	// TGA header
	uint8_t header[18] = {
		0, 0, 2,
		0, 0, 0, 0, 0,
		0, 0,
		0, 0,
		WIDTH & 0xFF, (WIDTH >> 8) & 0xFF,
		HEIGHT & 0xFF, (HEIGHT >> 8) & 0xFF,
		24,
		0,
	};
	outfile.write((const char*)header, 18);

	// Write pixel data
	for (int y = 0; y < HEIGHT; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			uint8_t pixel[3] = {
				image[y][x] & 0xFF,
				(image[y][x] >> 8) & 0xFF,
				(image[y][x] >> 16) & 0xFF,
			};
			outfile.write((const char*)pixel, 3);
		}
	}

	outfile.close();
	if (!outfile)
	{
		cout << "Error writing to " << filename << endl;
		exit(1);
	}
}

// Function to render Mandelbrot set
void compute_mandelbrot(double left, double right, double top, double bottom, int y_start, int y_end)
{
	for (int y = y_start; y < y_end; ++y)
	{
		for (int x = 0; x < WIDTH; ++x)
		{
			// Map pixel to complex plane
			complex<double> c(left + (x * (right - left) / WIDTH),
				top + (y * (bottom - top) / HEIGHT));

			complex<double> z(0.0, 0.0);
			int iterations = 0;
			while (abs(z) < 2.0 && iterations < MAX_ITERATIONS)
			{
				z = (z * z) + c;
				++iterations;
			}

			// Color pixel based on number of iterations
			image[y][x] = (iterations == MAX_ITERATIONS) ? 0xFFC400 : 0x000000;
		}
	}
}

// Thread function for Mandelbrot set computation
void compute_mandelbrot_thread(double left, double right, double top, double bottom, int y_start, int y_end)
{
	compute_mandelbrot(left, right, top, bottom, y_start, y_end);

	unique_lock<mutex> lock(image_mutex);
	--active_threads;
	cv.notify_one();
}

// Function to start threads for Mandelbrot set computation
void start_threads(double left, double right, double top, double bottom, int num_threads)
{
	int slice_height = HEIGHT / num_threads;

	for (int i = 0; i < num_threads; i++) {
		unique_lock<mutex> lock(image_mutex);
		++active_threads;
		int y_start = i * slice_height;
		int y_end = y_start + slice_height;
		thread t(compute_mandelbrot_thread, left, right, top, bottom, y_start, y_end);
		t.detach();
	}

	unique_lock<mutex> lock(image_mutex);
	cv.wait(lock, [] { return active_threads == 0; });
}

int main(int argc, char* argv[])
{
	cout << "Enter number of threads to use: ";
	int num_threads;
	std::cin >> num_threads;
	cout << "Please wait..." << endl;

	the_clock::time_point start = the_clock::now();

	double left = -2.0;
	double right = 1.0;
	double top = 1.125;
	double bottom = -1.125;

	start_threads(left, right, top, bottom, num_threads);

	the_clock::time_point end = the_clock::now();

	auto time_taken = duration_cast<milliseconds>(end - start).count();
	cout << "Computing the Mandelbrot set took " << time_taken << " ms." << endl;

	write_tga("output.tga");

	return 0;
}