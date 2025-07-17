#include "log.h"

#include <iostream>


TimeLogger * TimeLogger::singleton = nullptr;
bool TimeLogger::init_done = false;



void TimeLogger::persistTectonicsProcessData()
{
	// Tectonics Steps STATS:
	TectonicStepsTimeData tecto_average, tecto_worst;
	for (const TectonicStepsTimeData & data : m_tectonics_steps_data)
	{
		tecto_average.step1_secs += data.step1_secs;
		tecto_average.step2_secs += data.step2_secs;
		tecto_average.step3_secs += data.step3_secs;
		//tecto_average.step4_secs += data.step4_secs;

		if (data.step1_secs > tecto_worst.step1_secs)
			tecto_worst.step1_secs = data.step1_secs;
		if (data.step2_secs > tecto_worst.step2_secs)
			tecto_worst.step2_secs = data.step2_secs;
		if (data.step3_secs > tecto_worst.step3_secs)
			tecto_worst.step3_secs = data.step3_secs;
		//if (data.step4_secs > tecto_worst.step4_secs)
			//tecto_worst.step4_secs = data.step4_secs;
	}

	int tecto_num_runs = m_tectonics_steps_data.size();
	double n = (double)tecto_num_runs;
	tecto_average.step1_secs /= n;
	tecto_average.step2_secs /= n;
	tecto_average.step3_secs /= n;
	//tecto_average.step4_secs /= n;

	// Re-meshing STATS:
	RemeshingTimeData remesh_average, remesh_worst;
	for (const RemeshingTimeData & data : m_remeshing_data)
	{
		remesh_average.sampling_secs += data.sampling_secs;
		remesh_average.total_secs += data.total_secs;

		if (data.sampling_secs > remesh_worst.sampling_secs)
			remesh_worst.sampling_secs = data.sampling_secs;
		if (data.total_secs > remesh_worst.total_secs)
			remesh_worst.total_secs = data.total_secs;
	}

	int remesh_num_runs = m_remeshing_data.size();
	n = (double)remesh_num_runs;
	remesh_average.sampling_secs /= n;
	remesh_average.total_secs /= n;

	// Rifting STATS:
	RiftingTimeData rift_average, rift_worst;
	for (const RiftingTimeData & data : m_rifting_data)
	{
		rift_average.rifting_secs += data.rifting_secs;
		
		if (data.rifting_secs > rift_worst.rifting_secs)
			rift_worst.rifting_secs = data.rifting_secs;		
	}

	int rift_num_runs = m_rifting_data.size();
	n = (double)rift_num_runs;
	rift_average.rifting_secs /= n;	

	// SAVE TO DISK:
	const char * logfile = "../log/execution_times.txt";
	FILE * file = std::fopen(logfile, "w");
	if (file == NULL)
	{
		std::cout << "ERROR opening log file " << logfile << " ! " << std::endl;
		return;
	}

	std::printf("# ALL TIMES IN SECONDS\n\n");
	
	std::fprintf(file, "### Tectonics Steps ###\n");
	std::fprintf(file, "Num = %d\n", tecto_num_runs);
	std::fprintf(file, "Step 1:\n");
	std::fprintf(file, "  average = %f\n", tecto_average.step1_secs);
	std::fprintf(file, "  worst = %f\n", tecto_worst.step1_secs);
	std::fprintf(file, "Step 2:\n");
	std::fprintf(file, "  average = %f\n", tecto_average.step2_secs);
	std::fprintf(file, "  worst = %f\n", tecto_worst.step2_secs);
	std::fprintf(file, "Step 3:\n");
	std::fprintf(file, "  average = %f\n", tecto_average.step3_secs);
	std::fprintf(file, "  worst = %f\n", tecto_worst.step3_secs);
	//std::fprintf(file, "Step 4:\n");
	//std::fprintf(file, "  average = %f\n", tecto_average.step4_secs);
	//std::fprintf(file, "  worst = %f\n", tecto_worst.step4_secs);

	std::fprintf(file, "\n");
	std::fprintf(file, "### Global Re-meshing ###\n");
	std::fprintf(file, "Num = %d\n", remesh_num_runs);
	std::fprintf(file, "Sampling:\n");
	std::fprintf(file, "  average = %f\n", remesh_average.sampling_secs);
	std::fprintf(file, "  worst = %f\n", remesh_worst.sampling_secs);
	std::fprintf(file, "Total:\n");
	std::fprintf(file, "  average = %f\n", remesh_average.total_secs);
	std::fprintf(file, "  worst = %f\n", remesh_worst.total_secs);

	std::fprintf(file, "\n");
	std::fprintf(file, "### Plate Rifting ###\n");
	std::fprintf(file, "Num = %d\n", rift_num_runs);
	std::fprintf(file, "  average = %f\n", rift_average.rifting_secs);
	std::fprintf(file, "  worst = %f\n", rift_worst.rifting_secs);

	std::fclose(file);
	std::cout << "Timing log file " << logfile << " written successfully." << std::endl;
}