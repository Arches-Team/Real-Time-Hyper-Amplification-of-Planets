#pragma once

#include <vector>
#include <cstdio>


struct TectonicStepsTimeData
{
	double step1_secs = 0.0;
	double step2_secs = 0.0;
	double step3_secs = 0.0;
	//double step4_secs = 0.0;
};

struct RemeshingTimeData
{
	double sampling_secs = 0.0;
	double total_secs = 0.0;
};

struct RiftingTimeData
{
	double rifting_secs = 0.0;
};



class TimeLogger
{
public:

	static TimeLogger* instance()
	{
		if (!init_done)
		{
			singleton = new TimeLogger();
			init_done = true;
		}
		return singleton;
	}

	static void releaseInstance()
	{
		if (init_done) {
			delete singleton;
			init_done = false;
		}
	}
	static TimeLogger * singleton;
	static bool init_done;

public:

	inline TimeLogger() 
	{
		m_tectonics_steps_data.reserve(2048);
		m_remeshing_data.reserve(512);
		m_rifting_data.reserve(512);
	}
	inline ~TimeLogger() {}

	inline void resetTectonicsProcessData()
	{
		m_tectonics_steps_data.clear();
		m_remeshing_data.clear();
		m_rifting_data.clear();
	}

	inline void logTectonicStepsTimeData(const TectonicStepsTimeData & data)
	{
		m_tectonics_steps_data.push_back(data); 
	}

	inline void logRemeshingTimeData(const RemeshingTimeData & data)
	{
		m_remeshing_data.push_back(data);
	}

	inline void logRiftingTimeData(const RiftingTimeData & data)
	{
		m_rifting_data.push_back(data);
	}

	void persistTectonicsProcessData();


private:
	std::vector<TectonicStepsTimeData> m_tectonics_steps_data;
	std::vector<RemeshingTimeData> m_remeshing_data;
	std::vector<RiftingTimeData> m_rifting_data;
};