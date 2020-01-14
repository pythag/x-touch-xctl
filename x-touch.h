/* ----------------------------------------------------------------
                   x-touch-xctl library.
   This library allows you to communicate with a Behringer X-Touch 
   over Ethernet using the Xctl protocol. It gives you full control
   over the motorised faders, LEDs, 7-segment displays, wheels
   and scribble pads (including RGB backlight control)
   ---------------------------------------------------------------- */
   
/* Note: This library doesn't contain the routines for actually sending
   and receiving the UDP packets over Ethernet (it only generates and
   interprets the packet contents). For an example of how to implement
   this see the main.cpp file supplied alongside */

/*
MIT License

Copyright (c) 2020 Martin Whitaker

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <time.h>

typedef void (*packet_sender)(void *,unsigned char*, unsigned int); // User pointer, Packet buffer pointer, Packet length
typedef void (*callback)(void *,unsigned char, int); // User pointer, Object ID, New value

enum xt_colours_t { BLACK, RED, GREEN, YELLOW, BLUE, PINK, CYAN, WHITE };
enum xt_button_state_t { OFF, FLASHING, ON };

typedef struct {
    char TopText[8];
    char BotText[8];
    xt_colours_t Colour;
    int Inverted;
} xt_ScribblePad_t;

class XTouch {
    public:
        XTouch(packet_sender PacketSendHandler, void *data);
        ~XTouch();

        int HandlePacket(unsigned char *buffer, unsigned int len);
        void SetAssignment(int v);
        void SetHMSF(int h, int m, int s, int f);
        void SetFrames(int v);
        void SetTime(struct tm* t);
        void SetDialPan(int channel, int position);
        void SetDialLevel(int channel, int level);
        void SetFaderLevel(int channel, int level);
        void SetMeterLevel(int channel, int level);
        void SendAllMeters();
        void SetSingleButton(unsigned char n, xt_button_state_t v);
        void SetScribble(int channel, xt_ScribblePad_t info);

        void RegisterFaderCallback(callback Handler, void *data);
        void RegisterFaderStateCallback(callback Handler, void *data);
        void RegisterDialCallback(callback Handler, void *data);
        void RegisterButtonCallback(callback Handler, void *data);      

    private:
        int HandleFaderTouch(unsigned char *buffer, unsigned int len);
        int HandleLevel(unsigned char *buffer, unsigned int len);
        int HandleRotation(unsigned char *buffer, unsigned int len);
        int HandleButton(unsigned char *buffer, unsigned int len);
        int HandleProbe(unsigned char *buffer, unsigned int len);
        int HandleUnknown(unsigned char *buffer, unsigned int len);
        void SendPacket(unsigned char *buffer, unsigned int len);
        void CheckIdle();
        void SendScribble(unsigned char n);
        void SendAllScribble();
        void SendSingleButton(unsigned char n);
        void SendAllButtons();
        void SendSingleDial(unsigned char n);
        void SendAllDials();
        void SendSingleFader(unsigned char n);
        void SendAllFaders();
        void SendAllBoard();
        void SetSegments(unsigned char segment, unsigned char value);
        void SendSegments();
        void DisplayNumber(unsigned char start, int len, int v,int zeros=0);
        unsigned char SegmentBitmap(char v);

        packet_sender mPacketSendHandler;
        void *mPPacketData;

        callback mButtonCallbackHandler;
        callback mDialCallbackHandler;
        callback mLevelCallbackHandler;
        callback mFaderStateCallbackHandler;
        void *mButtonCallbackData;
        void *mDialCallbackData;
        void *mLevelCallbackData;
        void *mFaderStateCallbackData;

        time_t mLastIdle;
        int mFullRefreshNeeded;
        xt_button_state_t mButtonLEDStates[127];
        unsigned int mDialLeds[8];
        unsigned char mMeterLevels[8];
        unsigned int mFaderLevels[9];
        unsigned char mSegmentCache[12];

        xt_ScribblePad_t mScribblePads[8];
};
