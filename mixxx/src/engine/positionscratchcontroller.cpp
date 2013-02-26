#include <QtDebug>
#include <QCursor>

#include "engine/positionscratchcontroller.h"
#include "engine/enginebufferscale.h" // for MIN_SEEK_SPEED
#include "mathstuff.h"

#ifdef _MSC_VER
#include <float.h>  // for _finite() on VC++
#define isinf(x) (!_finite(x))
#endif

class VelocityController {
  public:
    VelocityController()
        : m_last_error(0.0),
          m_error_sum(0.0),
          m_p(0.0),
          m_i(0.0),
          m_d(0.0) {
    }

    void setPID(double p, double i, double d) {
        m_p = p;
        m_i = i;
        m_d = d;
    }

    void reset(double last_error) {
        m_last_error = last_error;
        m_error_sum = 0.0;
    }

    double observation(double position, double target_position, double dt) {
        Q_UNUSED(dt) // Since the controller runs with constant sample rate
                     // we don't have to deal with dt inside the controller

        const double error = target_position - position;

        double output = m_p * error;

        // In case of error too small then stop controller
        if (abs(error) > 0.001) {

            // Calculate integral component of PID
            m_error_sum += error;

            // Calculate differential component of PID. Positive if we're getting
            // worse, negative if we're getting closer.
            double error_change = (error - m_last_error);

            // qDebug() << "target:" << m_target_position << "position:" << position
            //          << "error:" << error << "change:" << error_change << "sum:" << m_error_sum;

            // Main PID calculation
            output += m_i * m_error_sum + m_d * error_change;

            // Try to stabilize us if we're close to the target. Otherwise we might
            // overshoot and oscillate.
            //if (fabs(error) < m_samples_per_buffer) {
                //double percent_remaining = error / m_samples_per_buffer;
                //// Apply exponential decay to try and stop the stuttering.
                //double decay = (1.0 - pow(2, -fabs(percent_remaining)));
                //output = percent_remaining * decay;
                //qDebug() << "clamp decay" << decay << "output" << output;
            //}
        }

        m_last_error = error;
        return output;
    }

  private:
    double m_last_error;
    double m_error_sum;
    double m_p, m_i, m_d;
};

class MouseRateIIFilter {
  public:
    MouseRateIIFilter()
        : m_factor(1.0),
          m_last_rate(0.0) {
    }

    void setFactor(double factor) {
        m_factor = factor;
    }

    void reset(double last_rate) {
        m_last_rate = last_rate;
    }

    double filter(double rate) {
        m_last_rate = m_last_rate * (1 - m_factor) + rate * m_factor;
        return m_last_rate;
    }

  private:
    double m_factor;
    double m_last_rate;
};


PositionScratchController::PositionScratchController(const char* pGroup)
    : m_group(pGroup),
      m_bScratching(false),
      m_bDetectStop(false),
      m_bEnableInertia(false),
      m_dLastPlaypos(0),
      m_dPositionDeltaSum(0),
      m_dTargetDelta(0),
      m_dStartScratchPosition(0),
      m_dRate(0),
      m_dMoveDelay(0),
      m_dMouseSampeTime(0) {
    m_pScratchEnable = new ControlObject(ConfigKey(pGroup, "scratch_position_enable"));
    m_pScratchPosition = new ControlObject(ConfigKey(pGroup, "scratch_position"));
    m_pMasterSampleRate = ControlObject::getControl(ConfigKey("[Master]", "samplerate"));
    m_pVelocityController = new VelocityController();
    m_pMouseRateIIFilter = new MouseRateIIFilter;


    //m_pVelocityController->setPID(0.2, 1.0, 5.0);
    //m_pVelocityController->setPID(0.1, 0.0, 5.0);
    //m_pVelocityController->setPID(0.0001, 0.0, 0.00);
}

PositionScratchController::~PositionScratchController() {
    delete m_pScratchEnable;
    delete m_pScratchPosition;
    delete m_pVelocityController;
    delete m_pMouseRateIIFilter;
}

//volatile double _p = 0.3;
//volatile double _i = 0;
//volatile double _d = -0.15;
//volatile double _f = 0.5;

void PositionScratchController::process(double currentSample, double releaseRate,
        int iBufferSize, double baserate) {
    bool scratchEnable = m_pScratchEnable->get() != 0;

   	if (!m_bScratching && !scratchEnable) {
        // We were not previously in scratch mode are still not in scratch
        // mode. Do nothing
   	    return;
    }

    // The latency or time difference between process calls.
    const double dt = static_cast<double>(iBufferSize)
            / m_pMasterSampleRate->get() / 2;

    m_dMouseSampeTime += dt;
   	if (m_dMouseSampeTime > 0.016 || !m_bScratching) {
        // m_scratchPosition = m_pScratchPosition->get();
   	    m_scratchPosition = m_pScratchEnable->get() *
             QCursor::pos().x() * -2;
   	    m_dMouseSampeTime = 0;
   	}

    // Tweak PID controller for different latencies
    double p;
    double i;
    double d;
    double f;
    p = 17 * dt; // ~ 0.2 for 11,6 ms
    if (p > 0.3) {
        // avoid overshooting at high latency
        p = 0.3;
    }
    i = 0;
    d = p/-2;
    f = 20 * dt;
    if (f > 1) {
        f = 1;
    }
    m_pVelocityController->setPID(p, i, d);
    m_pMouseRateIIFilter->setFactor(f);
    //m_pVelocityController->setPID(_p, _i, _d);
    //m_pMouseRateIIFilter->setFactor(_f);

    if (m_bScratching) {
        if (m_bEnableInertia) {
            // If we got here then we're not scratching and we're in inertia
            // mode. Take the previous rate that was set and apply a
            // deceleration.

            // If we're playing, then do not decay rate below 1. If we're not playing,
            // then we want to decay all the way down to below 0.01
            const double kDecayThreshold = ((releaseRate < 0.01) ? 0.01 : releaseRate);

            // Max velocity we would like to stop in a given time period.
            const double kMaxVelocity = 100;
            // Seconds to stop a throw at the max velocity.
            const double kTimeToStop = 1.0;

            // We calculate the exponential decay constant based on the above
            // constants. Roughly we backsolve what the decay should be if we want to
            // stop a throw of max velocity kMaxVelocity in kTimeToStop seconds. Here is
            // the derivation:
            // kMaxVelocity * alpha ^ (# callbacks to stop in) = kDecayThreshold
            // # callbacks = kTimeToStop / dt
            // alpha = (kDecayThreshold / kMaxVelocity) ^ (dt / kTimeToStop)
            const double kExponentialDecay = pow(kDecayThreshold / kMaxVelocity, dt / kTimeToStop);

            m_dRate *= kExponentialDecay;

            // If the rate has decayed below the threshold, or scartching is 
            // reanabled then leave inertia mode.
            if (fabs(m_dRate) < kDecayThreshold || scratchEnable) {
                m_bEnableInertia = false;
                m_bScratching = false;
            }        
        } else if (scratchEnable) {
            // If we're scratching, clear the inertia flag. This case should
            // have been caught by the 'enable' case below, but just to make
            // sure.
            m_bEnableInertia = false;

            // Measure the total distance traveled since last frame and add
            // it to the running total. This is required to scratch within loop
            // boundaries. Normalize to one buffer
            m_dPositionDeltaSum += (currentSample - m_dLastPlaypos) /
                    (iBufferSize * baserate);

            // Set the scratch target to the current set position
            // and normalize to one buffer
            double targetDelta = (m_scratchPosition - m_dStartScratchPosition) /
                    (iBufferSize * baserate);

            bool mouse_update = true;

            if (m_dTargetDelta == targetDelta) {
                // we get here, if the next mouse position is delayed
                // the mouse is stopped or moves slow. Since we don't know the case
                // we assume delayed mouse updates for 40 ms
                m_dMoveDelay += dt;
                if (m_dMoveDelay < 0.04) {
                    // Assume a missing Mouse Update and continue with the
                    // Previously calculated rate.
                    targetDelta += m_dRate * (m_dMoveDelay/dt);
                    mouse_update = false;
                } else {
                    // Mouse has stopped
                    // Bypass Mouse IIF to avoid overshooting
                    m_pMouseRateIIFilter->setFactor(1);
                    m_pVelocityController->setPID(p, i, 0);
                    if (targetDelta == 0) {
                        // Mouse was not moved at all
                        // Stop immediately by restarting the controller
                        // in stopped mode
                        m_pVelocityController->reset(0);
                        m_pMouseRateIIFilter->reset(0);
                        m_dPositionDeltaSum = 0;
                    }
                }
            } else {
                m_dMoveDelay = 0;
                m_dTargetDelta = targetDelta;
            }

            double mouseRate = targetDelta - m_dPositionDeltaSum;

            if (mouse_update) {
                if (mouseRate < MIN_SEEK_SPEED && mouseRate > -MIN_SEEK_SPEED) {
                    // we cannot get closer
                    m_dRate = 0;
                } else {
                    mouseRate = m_pMouseRateIIFilter->filter(targetDelta - m_dPositionDeltaSum);

                    m_dRate = m_pVelocityController->observation(
                        m_dPositionDeltaSum, m_dPositionDeltaSum + mouseRate, dt);

                    // Note: The following SoundTouch changes the rate by a ramp
                    // This looks like average of the new and the old rate independent
                    // from dt. Ramping is disabled when direction changes or rate = 0;
                    // (determined experimentally)
                }
            }

            qDebug() << m_dRate << mouseRate << m_scratchPosition << (targetDelta - m_dPositionDeltaSum) << targetDelta << m_dPositionDeltaSum << dt;
        } else {
            // We were previously in scratch mode and are no longer in scratch
            // mode. Disable everything, or optionally enable inertia mode if
            // the previous rate was high enough to count as a 'throw'
            
            // The rate threshold above which disabling position scratching will enable
            // an 'inertia' mode.
            const double kThrowThreshold = 2.5;

            if (fabs(m_dRate) > kThrowThreshold) {
                m_bEnableInertia = true;
            } else {
                m_bScratching = false;
            }
            //qDebug() << "disable";
        }
    } else if (scratchEnable) {
            // We were not previously in scratch mode but now are in scratch
            // mode. Enable scratching.
            m_bScratching = true;
            m_bEnableInertia = false;
            m_dMoveDelay = 0;
            // Set up initial values, in a way that the system is settled
            m_dRate = releaseRate;
            m_dPositionDeltaSum = -(releaseRate / p); // Set to the remaining error of a p controller
            m_pVelocityController->reset(-m_dPositionDeltaSum);
            m_pMouseRateIIFilter->reset(-m_dPositionDeltaSum);
            m_dStartScratchPosition = m_scratchPosition;
            //qDebug() << "scratchEnable()" << currentSample;
    }
    m_dLastPlaypos = currentSample;
}

bool PositionScratchController::isEnabled() {
    // return true only if m_dRate is valid.
    return m_bScratching;
}

double PositionScratchController::getRate() {
    return m_dRate;
}

void PositionScratchController::notifySeek(double currentSample) {
    // scratching continues after seek due to calculating the relative distance traveled 
    // in m_dPositionDeltaSum   
    m_dLastPlaypos = currentSample;
}
