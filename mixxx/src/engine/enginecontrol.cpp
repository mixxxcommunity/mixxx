// enginecontrol.cpp
// Created 7/5/2009 by RJ Ryan (rryan@mit.edu)

#include "engine/enginecontrol.h"
#include "engine/enginemaster.h"

EngineControl::EngineControl(const char * _group,
                             ConfigObject<ConfigValue> * _config) :
    m_pGroup(_group),
    m_pConfig(_config),
    m_dCurrentSample(0),
    m_dTotalSamples(0),
    m_pEngineMaster(NULL) {
}

EngineControl::~EngineControl() {

}


double EngineControl::process(const double,
                               const double,
                               const double,
                               const int) {
    return kNoTrigger;
}

double EngineControl::nextTrigger(const double,
                                  const double,
                                  const double,
                                  const int) {
    return kNoTrigger;
}

double EngineControl::getTrigger(const double,
                                 const double,
                                 const double,
                                 const int) {
    return kNoTrigger;
}

void EngineControl::trackLoaded(TrackPointer) {
}

void EngineControl::trackUnloaded(TrackPointer) {
}

void EngineControl::hintReader(QList<Hint>&) {
}

void EngineControl::setEngineMaster(EngineMaster* pEngineMaster) {
    m_pEngineMaster = pEngineMaster;
}

void EngineControl::setCurrentSample(const double dCurrentSample, const double dTotalSamples) {
    m_dCurrentSample = dCurrentSample;
    m_dTotalSamples = dTotalSamples;
}

double EngineControl::getCurrentSample() const {
    return m_dCurrentSample;
}

double EngineControl::getTotalSamples() const {
    return m_dTotalSamples;
}

const char* EngineControl::getGroup() {
    return m_pGroup;
}

ConfigObject<ConfigValue>* EngineControl::getConfig() {
    return m_pConfig;
}

EngineMaster* EngineControl::getEngineMaster() {
    return m_pEngineMaster;
}

void EngineControl::notifySeek(double dNewPlaypos) {
    Q_UNUSED(dNewPlaypos);
}
