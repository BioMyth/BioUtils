#ifndef __BIO_UTIL_H__
#define __BIO_UTIL_H__

#pragma once

#include <Arduino.h>
#include <mutex>

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
*/
namespace BioUtil{

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

    typedef enum : bool {
        Off = LOW,
        On = HIGH
    } LEDState;

    struct AnimationFrame {
        AnimationFrame(LEDState state = LEDState::Off, uint16_t msDuration = 0);
        LEDState state;
        // Duration of the state in ms
        uint16_t msDuration;
    };

    // Isolate local variables to this header
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
    };




    class Animation {
        protected:
            String name;
            AnimationFrame * animationFrames;
            u_int8_t frameCount;
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

            bool setup(const uint8_t &ledPin = defaultLEDPin, const Animation &animation = PresetAnimation::Off);

            void setAnimation(const Animation &newAnimation);


            Animation getAnimation();

    };
};

#endif