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

#include "x-touch.h"
#include <stdio.h>
#include <string.h>

unsigned char probe[] =         { 0xf0, 0x00, 0x20, 0x32, 0x58, 0x54, 0x00, 0xf7 };
unsigned char proberesponse[] = { 0xf0, 0x00, 0x20, 0x32, 0x58, 0x54, 0x01, 0xf7 };
unsigned char probeb[] =        { 0xf0, 0x00, 0x00, 0x66, 0x58, 0x01, 0x30, 0x31, 0x35, 0x36, 0x34, 0x30, 0x36, 0x36, 0x37, 0x34, 0x30, 0xf7 };
unsigned char idlepacket[] =    { 0xf0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xf7 };

// Public interfaces
// You must pass the constructor a function for sending UDP packets back to the XTouch taking two parameters - the data buffer and the length
XTouch::XTouch(packet_sender PacketSendHandler,void *data) {
    int i;
    mPacketSendHandler=PacketSendHandler;
    mPPacketData=data;
    mLastIdle=0;
    // Set default LED states
    for(i=0;i<127;i++) {
        mButtonLEDStates[i]=OFF;
    }
    for(i=0;i<8;i++) {
        mDialLeds[i]=0;
        mMeterLevels[i]=0;
    }
    for(i=0;i<9;i++) {
        mFaderLevels[i]=0;
    }
    memset(mScribblePads,0,sizeof(mScribblePads));
    for(i=0;i<8;i++) {
        mScribblePads[i].Colour=WHITE;
    }
    mButtonCallbackHandler=NULL;
    mDialCallbackHandler=NULL;
    mLevelCallbackHandler=NULL;
    mFaderStateCallbackHandler=NULL;
    mFullRefreshNeeded=0;
}

XTouch::~XTouch() {
    
}

// The handler registered here will be called whenever a fader is moved
void XTouch::RegisterFaderCallback(callback Handler, void *data) {
    mLevelCallbackHandler=Handler;
    mLevelCallbackData=data;
}

// The handler registered here will be called whenever a fader is touched or released
void XTouch::RegisterFaderStateCallback(callback Handler, void *data) {
    mFaderStateCallbackHandler=Handler;
    mFaderStateCallbackData=data;
}

// The handler registered here will be called whenever a dial or jog wheel is turned
void XTouch::RegisterDialCallback(callback Handler, void *data) {
    mDialCallbackHandler=Handler;
    mDialCallbackData=data;
}

// The handler registered here will be called whenever a button is pressed or released
void XTouch::RegisterButtonCallback(callback Handler, void *data) {
    mButtonCallbackHandler=Handler;
    mButtonCallbackData=data;
}

// This moves a physical fader to the level provided (0 to 16384)
// 12800 is the 0db mark
// channel is in the range 0 to 8 (8=the 'main' fader)
void XTouch::SetFaderLevel(int channel, int level)
{
    if ((channel<0)||(channel>8)||(level<0)||(level>40960)) return;
    mFaderLevels[channel]=level;
    SendSingleFader(channel);
}

// Sets the level sent to the meters.
// channel = 0 to 7
// level = 0 to 9
void XTouch::SetMeterLevel(int channel, int level)
{
    if ((channel<0)||(channel>8)||(level<0)||(level>9)) return;
    mMeterLevels[channel]=level;
    SendAllMeters();
}

// If using the meters then you must frequently call SendAllMeters(), even if the levels don't change, as they naturally decay.
void XTouch::SendAllMeters()
{
    int i;
    unsigned char sendbuf[9];
    sendbuf[0]=0xd0;
    for(i=0;i<8;i++) {
        sendbuf[1+i]=(i<<4)+mMeterLevels[i];
    }
    SendPacket(sendbuf,9);
}

// Places a single mark around the dial to indicate pan position
// Channel = 0 to 7
// position = -6 for left pan, to +6 for right pan
void XTouch::SetDialPan(int channel, int position)
{
    if ((position<-6)||(position>6)) return;
    mDialLeds[channel]=1<<(position+6);
    SendSingleDial(channel);
}

// Places a growing bar graph around the dial to indicate level
// Channel = 0 to 7
// level = 0 to 13
void XTouch::SetDialLevel(int channel, int level)
{
    int i;
    int v=0;
    if ((level<0)||(level>13)) return;
    for(i=0;i<level;i++) {
        v+=1<<i;
    }
    mDialLeds[channel]=v;
    SendSingleDial(channel);
}

// Displays the integer provided in the 'assignment' display
// range = -9 to 99
void XTouch::SetAssignment(int v) {
    if ((v<-9)||(v>99)) return;
    DisplayNumber(0, 2, v);
    SendSegments();
}

// Displays values passed into Hours, Minutes, Seconds, Frames
void XTouch::SetHMSF(int h, int m, int s, int f) {
    DisplayNumber(2, 3, h);
    DisplayNumber(5, 2, m);
    DisplayNumber(7, 2, s);
    DisplayNumber(9, 3, f);
    SendSegments();
}

// Displays the integer provided in the 'frames' display
// range = -99 to 999
void XTouch::SetFrames(int v) {
    if ((v<-99)||(v>999)) return;
    DisplayNumber(9, 3, v);
    SendSegments();
}

// Displays a time provided in a tm structure into HMS
void XTouch::SetTime(struct tm* t) {
    if (!t) return;
    DisplayNumber(2, 3, t->tm_hour,0);
    DisplayNumber(5, 2, t->tm_min,1);
    DisplayNumber(7, 2, t->tm_sec,1);
    SendSegments();
}

// Sets the state of a button light (OFF, FLASHING, ON)
// 0 to 7 - Rec buttons
// 8 to 15 - Solo buttons
// 16 to 23 - Mute buttons
// 24 to 31 - Select buttons
// 40 to 45 - encoder assign buttons (track, send, pan,  plugin, eq, inst)
// 46 to 47 - Fader bank left / right
// 48 to 49 - Channel left / right
// 50 Flip
// 51 Global view
// 54 to 61 - Function buttons F1 to F8
// 62 to 69 - Buttons under 7-seg displays
// 70 to 73 - Modify buttons (shift, option, control, alt)
// 74 to 79 - Automation buttons (read, write, trim, touch, latch, group)
// 80 to 83 - Utility buttons (save, undo, cacel, enter)
// 84 to 90 - Transport buttons (marker, nudge, cycle, drop, replace, click, solo)
// 91 to 95 - Playback control (rewind, fast-forward, stop, play, record)
// 96 to 100 - Cursor keys (up, down, left, right, middle)
// 101 Scrub
// 113 Smpte
// 114 Beats
// 115 Solo - on 7-seg display
void XTouch::SetSingleButton(unsigned char n, xt_button_state_t v) {
    if ((n>115)||(v>2)) return;
    mButtonLEDStates[n]=v;
    SendSingleButton(n);
}

void XTouch::SetScribble(int channel, xt_ScribblePad_t info) {
    if ((channel<0)||(channel>7)) return;
    mScribblePads[channel]=info;
    SendScribble(channel);
}

// ----------------------------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------------------------


void XTouch::SendSingleFader(unsigned char n)
{
    unsigned char sendbuf[3];
    sendbuf[0]=0xe0+n;
    sendbuf[1]=mFaderLevels[n]&0x7f;
    sendbuf[2]=(mFaderLevels[n]>>7)&0x7f;
    SendPacket(sendbuf,3);
}

void XTouch::SendAllFaders()
{
    int i;
    unsigned char sendbuf[27];
    
    for(i=0;i<9;i++) {
        sendbuf[i*3]=0xe0+i;
        sendbuf[1+i*3]=mFaderLevels[i]&0x7f;
        sendbuf[2+i*3]=(mFaderLevels[i]>>7)&0x7f;
    }
    SendPacket(sendbuf,27);
}


void XTouch::DisplayNumber(unsigned char start, int len, int v, int zeros)
{
    char display[13];
    int i;
    memset(display,0,13);
    if (zeros==0) {
        if (len==2) {
            snprintf(display,12,"%2d",v);
        } else {
            snprintf(display,12,"%3d",v);        
        }
    } else {
        if (len==2) {
            snprintf(display,12,"%02d",v);
        } else {
            snprintf(display,12,"%03d",v);        
        }        
    }
    for(i=0;i<len;i++) {
        SetSegments(start+i,SegmentBitmap(display[i]));
    }
}

unsigned char XTouch::SegmentBitmap(char v) {
    switch (v) {
        case '1': return 0x06;
        case '2': return 0x5b;
        case '3': return 0x4f;
        case '4': return 0x66;
        case '5': return 0x6d;
        case '6': return 0x7d;
        case '7': return 0x07;
        case '8': return 0x7f;
        case '9': return 0x6f;
        case '0': return 0x3f;
        case '-': return 0x40;
        case 'a': return 0x77;

        default: return 0;
    }
    return 0;
}

// 7-segment display numbers:
// 0x30 to 0x37 - Left hand sides of knobs
// 0x38 to 0x3F - Right hand sides of knobs
// 0x60 - Left hand assignment digit
// 0x61 - Right hand assignment digit
// 0x62-0x64 - Bars digits
// 0x65-0x66 - Beats digits
// 0x67-0x68 - Sub division digits
// 0x69-0x6B - Ticks digits
// 0x70-0x7B - same as above but with . also lit
// Value: 7-bit bitmap of segments to illuminate
void XTouch::SendSegments() {
    unsigned char sendbuf[25];
    unsigned char segment;

    sendbuf[0]=0xb0;
    for(segment=0;segment<12;segment++) {
        sendbuf[1+segment*2]=segment+0x60;
        sendbuf[2+segment*2]=mSegmentCache[segment];
    }
    SendPacket(sendbuf,25);    
}

void XTouch::SetSegments(unsigned char segment, unsigned char value) {
    if (segment>11) return;
    mSegmentCache[segment]=value&0x7F;
}

void XTouch::SendSingleDial(unsigned char n)
{
    unsigned char sendbuf[33];
    sendbuf[0]=0xb0;
    sendbuf[1]=0x30+n;
    sendbuf[2]=mDialLeds[n]&0x7F;
    sendbuf[3]=0x38+n;
    sendbuf[4]=(mDialLeds[n]>>7)&0x7F;
    SendPacket(sendbuf,5);

}

void XTouch::SendAllDials()
{
    int i;
    unsigned char sendbuf[33];
    sendbuf[0]=0xb0;
    for(i=0;i<8;i++) {
        sendbuf[1+i*4]=0x30+i;
        sendbuf[2+i*4]=mDialLeds[i]&0x7F;
        sendbuf[3+i*4]=0x38+i;
        sendbuf[4+i*4]=(mDialLeds[i]>>7)&0x7F;
    }
    SendPacket(sendbuf,33);
}

void XTouch::SendAllScribble()
{
    int n;
    for(n=0;n<8;n++) {
        SendScribble(n);
    }
}

void XTouch::SendScribble(unsigned char n)
{
    unsigned char sendbuf[233];
    int i;
    sendbuf[0]=0xf0;
    sendbuf[1]=0x00;
    sendbuf[2]=0x00;
    sendbuf[3]=0x66;
    sendbuf[4]=0x58; 
    sendbuf[5]=0x20+n;
    if (mScribblePads[n].Inverted) {
        sendbuf[6]=0x40+mScribblePads[n].Colour; 
    } else {
        sendbuf[6]=mScribblePads[n].Colour;
    }
    for(i=0;i<7;i++) {
        sendbuf[7+i]=mScribblePads[n].TopText[i];
        sendbuf[14+i]=mScribblePads[n].BotText[i];
    }
    sendbuf[21]=0xf7;
    SendPacket(sendbuf,22);
}

void XTouch::SendSingleButton(unsigned char n) {
    unsigned char sendbuf[3];
    sendbuf[0]=0x90;
    sendbuf[1]=n;
    sendbuf[2]=mButtonLEDStates[n];
    SendPacket(sendbuf,3);
}

void XTouch::SendAllButtons() {
    unsigned char sendbuf[233];
    int i;
    sendbuf[0]=0x90;
    for(i=0;i<116;i++) {
        sendbuf[1+i*2]=i;
        sendbuf[2+i*2]=mButtonLEDStates[i];
    }
    SendPacket(sendbuf,233);
}

void XTouch::SendAllBoard() {
    SendAllButtons();
    SendAllDials();
    SendAllFaders();
    SendAllScribble();
    SendSegments();
}

void XTouch::SendPacket(unsigned char *buffer, unsigned int len)
{
    mPacketSendHandler(mPPacketData, buffer,len);
}

int XTouch::HandleFaderTouch(unsigned char *buffer, unsigned int len) {
    int fader;
    if ((len==3)&&(buffer[0]==0x90)&&(buffer[1]>=0x68)&&(buffer[1]<=0x70)) {
        fader=buffer[1]-0x68;
        if (mFaderStateCallbackHandler) mFaderStateCallbackHandler(mFaderStateCallbackData,fader,(buffer[2]!=0));
        return 1;
    }
    return 0;
}

int XTouch::HandleLevel(unsigned char *buffer, unsigned int len) {
    int level;
    int channel;
    if ((len==3)&&((buffer[0]&0xf0)==0xe0)) {
        channel=buffer[0]&0x0f;
        level=buffer[1]+(buffer[2]<<7);
        if (mLevelCallbackHandler) mLevelCallbackHandler(mLevelCallbackData, channel, level);
        return 1;
    }
    return 0;
}

int XTouch::HandleRotation(unsigned char *buffer, unsigned int len) {
    if ((len==3)&&(buffer[0]==0xb0)) {
        if (mDialCallbackHandler) {
            if ((buffer[2]&0x40)==0x40) {
                mDialCallbackHandler(mDialCallbackData, buffer[1], 0-(buffer[2]&0x0f));
            } else {
                mDialCallbackHandler(mDialCallbackData, buffer[1], buffer[2]&0x0f);                
            }
        }
        return 1;
    }
    return 0;
}

int XTouch::HandleButton(unsigned char *buffer, unsigned int len) {
    if ((len==3)&&(buffer[0]==0x90)) {
        if (mButtonCallbackHandler) mButtonCallbackHandler(mButtonCallbackData, buffer[1], (buffer[2]!=0));
        return 1;
    }
    return 0;
}

int XTouch::HandleProbe(unsigned char *buffer, unsigned int len) {
    if ((len==sizeof(probe))&&(memcmp(buffer, probe, sizeof(probe))==0)) {
        SendPacket(proberesponse, sizeof(proberesponse));
        return 1;
    }
    if ((len==sizeof(probeb))&&(memcmp(buffer, probeb, sizeof(probeb))==0)) {
        // No response needed - just ignore
        return 1;
    }
    return 0;
}

int XTouch::HandleUnknown(unsigned char *buffer, unsigned int len) {
    // Packets we don't recognise
    int i;
    printf("Unhandled packet - length %d\n",len);
    for(i=0;i<(int)len;i++) {
        printf("%02x ",buffer[i]);
    }
    printf("\n");
    return 1;
}

int XTouch::HandlePacket(unsigned char *buffer, unsigned int len) {
    CheckIdle();
    if (HandleProbe(buffer,len)>0) return 1;
    if (HandleFaderTouch(buffer,len)>0) return 1;
    if (HandleButton(buffer,len)>0) return 1;
    if (HandleRotation(buffer,len)>0) return 1;
    if (HandleLevel(buffer,len)>0) return 1;
    HandleUnknown(buffer,len);
    return 0;    
}

void XTouch::CheckIdle() {
    if (mLastIdle!=time(NULL)) {
        SendPacket(idlepacket, sizeof(idlepacket));
        if (mFullRefreshNeeded) {
            SendAllBoard();
            mFullRefreshNeeded=0;
        }
        if (time(NULL)-mLastIdle>5) {
            mFullRefreshNeeded=1;
        }
    	mLastIdle=time(NULL);
    }
}
