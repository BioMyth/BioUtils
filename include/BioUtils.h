#ifndef __BIO_UTILS_H__
#define __BIO_UTILS_H__

#pragma once

#include <Arduino.h>
#include <mutex>
#include <functional>
#include <random>
#include <limits>
#include <algorithms>

//#include <sstream>
//#include <ostream>
//#include <freertos/task.h>

#define DISABLE_SOUT
/*
TODO 
- Update the mutex to be more stable & simpler since the current granular mutex control is very excessive for the current implementation
- Simplify main LED Manager
- Improve naming scheme
- Add a deadzoning & scaling analog handler
- Add a rebound handling digital in button
- Move LED to the bluetooth/wifi thread
- Update main to suspend the system when off instead of just not firing the movement events
- Add some type of print wrapper for easier disabling

- Add a function to be called in a loop instead of threads if desired

- Add a tasker for threading effeciently
*/
namespace BioUtils{

    /*namespace {
        class SerialStream : public std::stringbuf {
            public:
                SerialStream(){
                    #ifndef DISABLE_SOUT
                        Serial.begin(115200);
                    #endif
                };
                virtual int sync() {
                    #ifdef DISABLE_SOUT
                        return 0;
                    #else
                        String convertedString = String(millis()) + ": " + this->str().c_str();
                        size_t printedChars = Serial.print(convertedString);
                        char emptyString = '\0';
                        // Clear buffer
                        this->pubsetbuf(&emptyString, std::streamsize(0));
                        return (printedChars == convertedString.length() ? 0 : -1);
                    #endif
                };
        };
        static SerialStream stream;
    };

    static std::ostream sout(&stream);*/
    namespace {
        static bool usedPins[128] = {false};
        
        static const AnimationFrame offAnimFrames = AnimationFrame(LEDState::Off, 0);

        static const AnimationFrame onAnimFrames = AnimationFrame(LEDState::On, 0);
        
        static const AnimationFrame blinkAnimFrames[] = {
            AnimationFrame(LEDState::Off, 500),
            AnimationFrame(LEDState::On, 500)
        }; 

        static const AnimationFrame doubleBlinkAnimFrames[] = {
            AnimationFrame(LEDState::Off, 700),
            AnimationFrame(LEDState::On, 100),
            AnimationFrame(LEDState::Off, 100),
            AnimationFrame(LEDState::On, 100)
        };

        static const u_int8_t defaultLEDPin = 2;
        std::mt19937_64 rand64Bit{ std::random_device{}() };
    };


    typedef enum : bool {
        Off = LOW,
        On = HIGH
    } LEDState;
    
    typedef unsigned char Byte;

    struct AnimationFrame {
        AnimationFrame(LEDState state = LEDState::Off, uint16_t msDuration = 0);
        LEDState state;
        // Duration of the state in ms
        uint16_t msDuration;
    };

    class UUID {
    protected:
        Byte uuid[16];
    public:
        UUID() {
            // Generate & fill with random
            memcpy(&this->uuid[0], &rand64Bit(), 8);
            memcpy(&this->uuid[8], &rand64Bit(), 8);
            // Bitset the values to mark it as a type 4 uuid 
            this->uuid[7] & 0x0F | 0x40;
            // UUID Version Field 3 bit identification used
            this->uuid[9] & 0x1F | 0xC0;
        };
        operator==(const &UUID b) const{
            return std::equal(this->uuid[0], this->uuid[15], b->uuid[0], b->uuid[15]);
        }
        operator!=(const &UUID b) const {
            return ! (*this == b);
        }
    };
    // Isolate local variables to this header



    class Animation {
        protected:
            String name;
            AnimationFrame * animationFrames;
            u_int8_t frameCount;
            UUID uuid;
        public:
            Animation();
            Animation(const AnimationFrame * animationFrames, const u_int8_t & frameCount, const String & name);
            ~Animation();

            String text() const;

            String getName() const;

            u_int8_t getFrameCount() const;

            AnimationFrame getFrame(const u_int8_t &frameNumber) const;
    };
    
    namespace PresetAnimation {
        
        static const Animation Off(&offAnimFrames, 1, "OffBuiltin");
        static const Animation On(&onAnimFrames, 1, "OnBuiltin");
        static const Animation Blink(blinkAnimFrames, 2, "BlinkBuiltin");
        static const Animation DoubleBlink(doubleBlinkAnimFrames, 4, "DoubleBlinkBuiltin");

    };

    class TaskManager {
        std::mutex taskMutex;
        eTaskState taskState;
        TaskHandle_t task;

        TaskFunction_t functionToCall;
        
        void *parameters;

        TickType_t callInterval;

        protected:
            void runningTask() {
                functionToCall(parameters);
            };
            void beginTask() {
                xTaskCreate(
                    this->functionToCall,
                "",
                20000,
                this->parameters,
                2,
                &this->task
                );
                };
        public:
            void setup(TaskFunction_t functionToCall, void* parameters, unsigned long callIntervalMS) {
                this->setup(functionToCall, parameters, (TickType_t) callIntervalMS / portTICK_PERIOD_MS);
            };
            void setup(TaskFunction_t functionToCall, void* parameters, TickType_t callIntervalTicks) {
                
            };



            eTaskState getState() {
                std::scoped_lock(this->taskMutex);
                return this->taskState;
            }
            bool suspendTask() {
                std::scoped_lock(this->taskMutex);
                if (this->taskState == eTaskState::eRunning) {
                    vTaskSuspend(this->task);
                    return true;
                }
                else return false;
            }
            bool delayTask(TickType_t ticks) {
                std::scoped_lock(this->taskMutex);
                if (this->taskState == eTaskState::eRunning) {
                    vTaskDelay(this->task, ticks);
                }
            }
            bool delayTask(unsigned long ms) {

            }
    };

    class LEDManager {
        protected:

            std::mutex animationMutex;

            std::mutex taskStateMutex;
            eTaskState taskState;

            Animation animation;

            int animFrameIndex;
            AnimationFrame currentFrame;

            unsigned long animFrameStartTime;
            uint8_t ledPin;
            bool ledState;

            bool invalidPin;
            bool isSetup;

            TaskHandle_t task;

            inline void setLED(const LEDState &newState);

            void nextFrame();

            inline TickType_t msToTick(const unsigned long &ms) const;

            static void threadedLoop(void * parameter);

            void start();

            void setTaskState(const eTaskState &newState);

        public:
            LEDManager();
            ~LEDManager();

            bool setup(const uint8_t &ledPin = defaultLEDPin, const Animation &animation = PresetAnimation::Off);

            void setAnimation(const Animation &newAnimation);


            Animation getAnimation();

    };
};

#endif
