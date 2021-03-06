#pragma once
#include <list>
#include <mutex>
#include <atomic>
#include <thread>

#include "Human.h"

struct svm_model;
struct svm_node;
struct svm_problem;
struct svm_parameter;

class Modeller
{
public:
	enum class AttentionType
	{
		STF, 
		LTF
	};

	enum class ApplicationMode
	{
		COLLECT,
		PREDICT
	};

	enum class StfClass
	{
		NO_CLASS = 0,
		OUTOFVIEW = 1,
		PERIPHERAL,
		APPROACHING,
		FOCUS,
		LEAVING
	};

	enum class LtfClass
	{
		NO_CLASS = 0,
		IGNORER = 1,
		GLIMPSER,
		WATCHER,
		INTERACTER
	};

	struct Predictions
	{
		Predictions() : stf(StfClass::NO_CLASS), ltf(LtfClass::NO_CLASS) {}
		StfClass stf;
		LtfClass ltf;
	};

	Modeller(ApplicationMode mode);
	~Modeller();

	void initCollection();
	void trainSvm();
	void savePatterns();
	void readProblem(const std::string &filenameStr, svm_problem &prob, svm_parameter &param,
		svm_node *x_space);
	uint8_t predict(const Human::Pattern &pattern, AttentionType type);

	Predictions updateWithFrame(const Human::HumanFrame &humanFrame);
	bool setLabelStf(uint64_t id, uint8_t labelStf);
	void updateFixed(float dt);

	ApplicationMode applicationMode() const { return m_applicationMode; }

	void setFilenameBase(const std::string &f) { m_filenameBase = f; }
	void fixedLoop();

private:
	std::list<Human::Pattern> m_patternsStf, m_patternsLtf;
	std::map<uint64_t, std::shared_ptr<Human>> m_humans;
	std::string m_filenameBase, m_filenameStf, m_filenameLtf;
	svm_model *m_modelStf;
	svm_model *m_modelLtf;
	std::mutex m_humansMutex;

	ApplicationMode m_applicationMode;
	std::shared_ptr<std::thread> m_fixedThread;
	std::atomic<bool> m_doFixedUpdate;
	double m_lastUpdate;
};

