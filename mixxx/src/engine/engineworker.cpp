// engineworker.cpp
// Created 6/2/2010 by RJ Ryan (rryan@mit.edu)

#include "engine/engineworker.h"

EngineWorker::EngineWorker()
        : m_isActive(0) {
    setAutoDelete(false);
}

EngineWorker::~EngineWorker() {

}

void EngineWorker::run() {

}
