#include <BioUtils.h>

#include <Arduino.h>
#include <mutex>
//#include <freertos/task.h>
//#define LED_PIN 2     // Change this to match your board's LED pin, if different


namespace BioUtils{

    AnimationFrame::AnimationFrame(LEDState state, uint16_t msDuration): state(state), msDuration(msDuration){};

    Animation::Animation(): animationFrames(nullptr), frameCount(0), name(""){};

    Animation::Animation(const AnimationFrame * animationFrames, const u_int8_t & frameCount, const String & name): animationFrames(nullptr), frameCount(frameCount), name(name) {
        this->animationFrames = new AnimationFrame[this->frameCount];
        memcpy(this->animationFrames, animationFrames, this->frameCount * sizeof(AnimationFrame));
    };
    Animation::~Animation() {
        if(animationFrames != nullptr) delete animationFrames;
    };

    String Animation::text() const {
        String ret;
        ret += "Name: " + this->name + '\n';
        ret += "Frame Count: " + this->frameCount + '\n';
        for (unsigned int frame = 0; frame < this->frameCount; frame++) {
            ret += "Frame " + frame ;
            ret += ": " + this->animationFrames[frame].msDuration;
            ret += "ms ";
            if(this->animationFrames[frame].state == LEDState::On){
                ret += "On";
            } else {
                ret += "Off";
            }
            ret += "\n";
        }
        return ret;
    }

    String Animation::getName() const {
        return this->name;
    };

    u_int8_t Animation::getFrameCount() const { 
        return this->frameCount; 
    };

    AnimationFrame Animation::getFrame(const u_int8_t &frameNumber) const {
        return this->animationFrames[frameNumber % frameCount];
    };

    inline void LEDManager::setLED(const LEDState &newState) {
        digitalWrite(this->ledPin, newState);
        this->ledState = newState;
    };

    void LEDManager::nextFrame() {
        this->animFrameIndex = (this->animFrameIndex + 1) % this->animation.getFrameCount();
        this->currentFrame = this->animation.getFrame(this->animFrameIndex);

        this->setLED(this->currentFrame.state);

        this->animFrameStartTime = millis();
    };

    inline TickType_t LEDManager::msToTick(const unsigned long &ms) const {
        return ms / portTICK_PERIOD_MS;
    };

    void LEDManager::setTaskState(const eTaskState &newState) {
        std::scoped_lock lock(this->taskStateMutex);
        this->taskState = newState;
    };

    void LEDManager::threadedLoop(void *parameter) {
        LEDManager *ledManager = (LEDManager *) parameter;
        while(true) {
            ledManager->animationMutex.lock();

            // Only one frame or duration of 0, nothing to do so suspend thread until animation changes
            if (ledManager->currentFrame.msDuration == 0 || ledManager->animation.getFrameCount() <= 1) {
                ledManager->animationMutex.unlock();
                ledManager->setTaskState(eSuspended);
                vTaskSuspend(ledManager->task);
            }
            else {

                unsigned long currentTime = millis();

                if ((currentTime - ledManager->animFrameStartTime) > ledManager->currentFrame.msDuration && ledManager->currentFrame.msDuration != 0) {
                    ledManager->nextFrame();
                }

                long int calcTime = ledManager->currentFrame.msDuration - (millis() - ledManager->animFrameStartTime);

                ledManager->animationMutex.unlock();

                if (calcTime > 0) {
                    ledManager->setTaskState(eBlocked);
                    vTaskDelay(ledManager->msToTick(calcTime));
                }
            }
            // Once the logic is handled mark thread as running again
            ledManager->setTaskState(eRunning);
        };
    };

    void LEDManager::start() {
        // Make sure it is setup before running thread
        if (this->invalidPin || !this->isSetup) abort();

        xTaskCreate(
            this->threadedLoop,
            "LEDController",
            20000,
            (void *) this,
            2,
            &this->task
        );
        this->setTaskState(eRunning);
    };


    LEDManager::LEDManager(): animFrameIndex(0), animFrameStartTime(0), ledPin(0), ledState(false), invalidPin(false), isSetup(false), task(nullptr), taskState(eInvalid) {};

    LEDManager::~LEDManager() {
        this->setLED(BioUtils::LEDState::Off);
        usedPins[this->ledPin] = false;
    }

    bool LEDManager::setup(const uint8_t &ledPin, const Animation &animation){
        if (this->isSetup) {
            abort();
        }

        if (this->ledPin > 128 || usedPins[this->ledPin]) {
            // Some error case
            std::runtime_error("LED Pin is invalid");
            this->invalidPin = true;
            return false;
        }

        this->invalidPin = false;
        this->ledPin = ledPin;
        
        pinMode(this->ledPin, OUTPUT);
        this->isSetup = true;

        this->setAnimation(animation);
        this->start();
        return true;
    };



    void LEDManager::setAnimation(const Animation &newAnimation) {
        // Wait for the mutex lock
        std::scoped_lock lock(this->animationMutex);

        // If the animations are the same then skip
        //if (newAnimation.getName() == this->animation.getName()) return;
        if (newAnimation.uuid == this->animation.uuid) return;
        this->animation = newAnimation;

        this->animFrameIndex = 0;
        this->animFrameStartTime = millis();
        

        this->currentFrame = this->animation.getFrame(this->animFrameIndex);
        // If it isn't setup yet or the pin is invalid, then skip the pin related updates
        if (!this->isSetup || this->invalidPin) return;

        this->setLED(this->currentFrame.state);
        
        // If the task hasn't been started yet, no need to run task management
        if (this->task == nullptr) return;

        //TaskStatus_t taskDetails;
        //vTaskGetInfo(this->task, &taskDetails, pdFALSE, eInvalid);

        //switch (taskDetails.eCurrentState)
        // Check the current taskState & if it is paused then resume it. Probably want a task mutex tbh
        std::scoped_lock lockTaskState(this->taskStateMutex);
        switch (this->taskState)
        {
        case eSuspended:
            vTaskResume(this->task);
            this->taskState = eRunning;
            break;
        case eBlocked:
            xTaskAbortDelay(this->task);
            this->taskState = eRunning;
            break;
        default:
            break;
        }
    };


    Animation LEDManager::getAnimation(){
        // Get the animation object & return it
        std::scoped_lock lock(this->animationMutex);
        Animation ret = animation;
        return ret;
    };

}
