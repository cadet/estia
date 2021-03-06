#include "Modeller.h"

#include "svm.h"
#include <fstream>
//#include <boost/date_time.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>
#include <iostream>

#include <windows.h>
#define SYSERROR()  GetLastError()

#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

static char *line = NULL;
static int max_line_len;

static char* readline(FILE *input)
{
	int len;
	
	if(fgets(line,max_line_len,input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line,max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}

void exit_input_error(int line_num)
{
	fprintf(stderr,"Wrong input format at line %d\n", line_num);
	exit(1);
}

Modeller::Modeller(ApplicationMode mode) : 
	m_modelStf(nullptr),
	m_modelLtf(nullptr),
	m_applicationMode(mode),
	m_doFixedUpdate(true),
	m_lastUpdate(0.0)
{
	if (mode == ApplicationMode::PREDICT)
	{

	}
	else
	{
		initCollection();
	}
}


Modeller::~Modeller()
{
	m_doFixedUpdate = false;

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

//	m_fixedThread->join();
}

void Modeller::readProblem(const std::string &filenameStr,
	svm_problem &prob, svm_parameter &param, svm_node *x_space)
{
#pragma warning(push)
#pragma warning(disable: 4996)

	const char *filename = filenameStr.c_str();

	int max_index, inst_max_index, i;
	size_t elements, j;
	FILE *fp = fopen(filename, "r");
	char *endptr;
	char *idx, *val, *label;

	std::cerr << "READ PROB ERROR: " << SYSERROR() << std::endl;

	if(fp == NULL)
	{
		fprintf(stderr,"can't open input file %s\n",filename);
		exit(1);
	}

	prob.l = 0;
	elements = 0;

	max_line_len = 1024;
	line = Malloc(char,max_line_len);
	while(readline(fp)!=NULL)
	{
		char *p = strtok(line," \t"); // label

		// features
		while(1)
		{
			p = strtok(NULL," \t");
			if(p == NULL || *p == '\n') // check '\n' as ' ' may be after the last feature
				break;
			++elements;
		}
		++elements;
		++prob.l;
	}
	rewind(fp);

	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct svm_node *,prob.l);
	x_space = Malloc(struct svm_node,elements);

	max_index = 0;
	j=0;
	for(i=0;i<prob.l;i++)
	{
		inst_max_index = -1; // strtol gives 0 if wrong format, and precomputed kernel has <index> start from 0
		readline(fp);
		prob.x[i] = &x_space[j];
		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(i+1);

		prob.y[i] = strtod(label,&endptr);
		if(endptr == label || *endptr != '\0')
			exit_input_error(i+1);

		while(1)
		{
			idx = strtok(NULL,":");
			val = strtok(NULL," \t");

			if(val == NULL)
				break;

			errno = 0;
			x_space[j].index = (int) strtol(idx,&endptr,10);
			if(endptr == idx || errno != 0 || *endptr != '\0' || x_space[j].index <= inst_max_index)
				exit_input_error(i+1);
			else
				inst_max_index = x_space[j].index;

			errno = 0;
			x_space[j].value = strtod(val,&endptr);
			if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
				exit_input_error(i+1);

			++j;
		}

		if(inst_max_index > max_index)
			max_index = inst_max_index;
		x_space[j++].index = -1;
	}

	if(param.gamma == 0 && max_index > 0)
		param.gamma = 1.0/max_index;

	if(param.kernel_type == PRECOMPUTED)
		for(i=0;i<prob.l;i++)
		{
			if (prob.x[i][0].index != 0)
			{
				fprintf(stderr,"Wrong input format: first column must be 0:sample_serial_number\n");
				exit(1);
			}
			if ((int)prob.x[i][0].value <= 0 || (int)prob.x[i][0].value > max_index)
			{
				fprintf(stderr,"Wrong input format: sample_serial_number out of range\n");
				exit(1);
			}
		}

	fclose(fp);

#pragma warning(pop)
}

void Modeller::trainSvm()
{
	std::stringstream filenameStf;
	filenameStf << m_filenameBase << "_stf.txt";

	std::stringstream filenameLtf;
	filenameLtf << m_filenameBase << "_ltf.txt";

	{
		svm_node *xspaceStf = nullptr;
		svm_problem probStf;
		svm_parameter paramStf;

		paramStf.svm_type = C_SVC;
		paramStf.kernel_type = RBF;
		paramStf.degree = 3;
		paramStf.gamma = 1.0f / 16.0f;
		paramStf.coef0 = 0;
		paramStf.nu = 0.5;
		paramStf.cache_size = 100;
		paramStf.C = 1;
		paramStf.eps = 1e-3;
		paramStf.p = 0.1;
		paramStf.shrinking = 1;
		paramStf.probability = 0;
		paramStf.nr_weight = 0;
		paramStf.weight_label = NULL;
		paramStf.weight = NULL;

		readProblem(filenameStf.str(), probStf, paramStf, xspaceStf);
		m_modelStf = svm_train(&probStf, &paramStf);
	}

	{
		svm_node *xspaceLtf = nullptr;
		svm_problem probLtf;
		svm_parameter paramLtf;

		paramLtf.svm_type = C_SVC;
		paramLtf.kernel_type = RBF;
		paramLtf.degree = 3;
		paramLtf.gamma = 1.0f / 16.0f;
		paramLtf.coef0 = 0;
		paramLtf.nu = 0.5;
		paramLtf.cache_size = 100;
		paramLtf.C = 1;
		paramLtf.eps = 1e-3;
		paramLtf.p = 0.1;
		paramLtf.shrinking = 1;
		paramLtf.probability = 0;
		paramLtf.nr_weight = 0;
		paramLtf.weight_label = NULL;
		paramLtf.weight = NULL;

		readProblem(filenameLtf.str(), probLtf, paramLtf, xspaceLtf);
		m_modelLtf = svm_train(&probLtf, &paramLtf);
	}

	
}

uint8_t Modeller::predict(const Human::Pattern &pattern, Modeller::AttentionType type)
{
	const svm_model *model = type == AttentionType::STF ? m_modelStf : m_modelLtf;

	if (model != nullptr)
	{
		svm_node *node = new svm_node[pattern.features.size() + 1];
		int idx = 0;
		for (double val : pattern.features)
		{
			node[idx].value = val;
			node[idx].index = idx++;
		}
		node[idx].index = -1;

		double prediction = svm_predict(model, node);

		return static_cast<uint8_t>(prediction);
	}
	return 0;
}

void Modeller::initCollection()
{
#pragma warning(push)
#pragma warning(disable: 4996)
	/*
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, 80, "%F_%H-%M-%S", timeinfo);
*/

	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);
	std::stringstream filename, filenameLtf;
//	filename << "./";
	auto obj = std::put_time(std::localtime(&now_c), "%Y-%m-%d_%H-%M-%S");
	filename << "data/" << obj << "_stf.txt";
	filenameLtf << "data/" << obj << "_ltf.txt";
	m_filenameStf = filename.str();
	m_filenameLtf = filenameLtf.str();
#pragma warning(pop)
}

void Modeller::savePatterns()
{
	// STF
	{ 
		std::ofstream svmdata(m_filenameStf);

		if (svmdata.is_open())
		{
			for (const Human::Pattern &pattern : m_patternsStf)
			{
				svmdata << pattern.label;

				int idx = 1;
				for (double val : pattern.features)
				{
					if (std::isnan(val) || std::isinf(val))
						val = 0.0;
					svmdata << " " << idx << ":" << val;
					idx++;
				}

				svmdata << std::endl;
			}

			svmdata.close();
		}
		else
		{
			std::cerr << "failed to open file: " << SYSERROR() << std::endl;
		}
	}

	// LTF
	{ 	

		std::ofstream svmdata(m_filenameLtf);

		if (svmdata.is_open())
		{
			for (const Human::Pattern &pattern : m_patternsLtf)
			{
				svmdata << pattern.label;

				int idx = 1;
				for (double val : pattern.features)
				{
					if (std::isnan(val) || std::isinf(val))
						val = 0.0;
					svmdata << " " << idx << ":" << val;
					idx++;
				}

				svmdata << std::endl;
			}

			svmdata.close();
		}
		else
		{
			std::cerr << "failed to open file: " << SYSERROR() << std::endl;
		}
	}

}

bool Modeller::setLabelStf(uint64_t id, uint8_t labelStf)
{
	std::lock_guard<std::mutex> lock(m_humansMutex);

	if (m_humans.find(id) == m_humans.end())
	{
		return false;
	}

	m_humans[id]->setLabel(labelStf);
	return true;
}

Modeller::Predictions Modeller::updateWithFrame(const Human::HumanFrame &humanFrame)
{
	std::lock_guard<std::mutex> lock(m_humansMutex);

	if (m_humans.find(humanFrame.id) == m_humans.end())
	{
		m_humans[humanFrame.id] = std::make_shared<Human>(humanFrame.id);
	}

	auto human = m_humans[humanFrame.id];

	if (human->mostRecentFrame() < humanFrame.frame)
	{
		human->update(humanFrame.pos, humanFrame.rot, humanFrame.facerot, humanFrame.engaged);
		human->setMostRecentFrame(humanFrame.frame);
	}

	Predictions predictions;

	if (applicationMode() == ApplicationMode::PREDICT)
	{
		uint8_t predictionStf = predict(human->currentStfPattern(), AttentionType::STF);	
		human->setPredictionStf(predictionStf);
		predictions.stf = static_cast<StfClass>(predictionStf);

		uint8_t predictionLtf = predict(human->currentLtfPattern(), AttentionType::LTF);
		human->setPredictionLtf(predictionLtf);
		predictions.ltf = static_cast<LtfClass>(predictionLtf);

	}

	return predictions;
}

void Modeller::fixedLoop()
{
	m_fixedThread = std::shared_ptr<std::thread>(new std::thread([this](){
		while (this && m_doFixedUpdate)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			updateFixed(0.05f);
		}
	}));

	m_fixedThread->detach();
}

void Modeller::updateFixed(float dt)
{
	std::lock_guard<std::mutex> lock(m_humansMutex);

	std::list<uint64_t> toDelete;

	for (auto &kv : m_humans)
	{
		auto human = kv.second;
		human->setLifetime(human->lifetime() + dt);
		human->setTimeSinceLastUpdate(human->timeSinceLastUpdate() + dt);
//		std::cout << "human over: " << human->timeSinceLastUpdate()
//			<< " / " << DISAPPEAR_AFTER << std::endl;
		if (human->timeSinceLastUpdate() > DISAPPEAR_AFTER)
		{
			toDelete.push_back(human->id());
		}
	}

	for (auto humanToDelete : toDelete)
	{
		auto humanIt = m_humans.find(humanToDelete);

		if (applicationMode() == ApplicationMode::COLLECT)
		{
			std::cout << "SAVING HUMAN" << std::endl;
			auto human = humanIt->second;
			auto stfPatterns = human->labelledStfPatterns();
			m_patternsStf.insert(m_patternsStf.end(), stfPatterns.begin(), stfPatterns.end());

			m_patternsLtf.push_back(human->labelledLtfPattern(0));
		}

		m_humans.erase(humanIt);
	}
}