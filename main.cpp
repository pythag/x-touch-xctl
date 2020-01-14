/* ------------------------------------------------------------------------------
   This is a sample application to demonstrate how to use the x-touch library.
   It makes the x-touch behave in a way similar to a simple 64 channel desk.
   Note: This is an interface demonstration only - no audio processing is done!
   -----------------------------------------------------------------------------*/

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

/* The X-Touch must have firmware version 1.15 in order to use Xctl mode
   To upgrade using Linux download the firmware from
   http://downloads.music-group.com/software/behringer/X-TOUCH/X-TOUCH_Firmware_V1.15.zip
   Connect the X-Touch via USB to the PC and run the following command:
   amidi -p hw:1,0,0 -s X-TOUCH_sysex_update_1-15_1-03.syx -i 100
*/

/* To configure the X-Touch for XCtl use the following procedure:
   - Hold select of CH1 down whilst the X-Touch is turned on
   - Set the mode to Xctl
   - Set the Ifc to Network
   - Set the Slv IP to the IP address of your PC
   - Set the DHCP on (or set a static IP on the X-Touch as desired)
   - Press Ch1 select to exit config mode
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include "x-touch.h"

#define BUFSIZE 1508

typedef struct {
    int sockfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
} socketinfo_t;

typedef struct {
    xt_ScribblePad_t pad;
    int mainlevel;
    int mute;
    int trimlevel;
    int pan;
    int solo;
    int rec;
    int mode;
} channelinfo_t;

channelinfo_t channels[64];

int selected=0;
int page=0;
int masterlevel=0;

void RenderDial(XTouch *board, int channel) {
    switch (channels[page*8+channel].mode) {
        case 0: // Pan mode
                sprintf(channels[page*8+channel].pad.TopText,"PAN");
                board->SetDialPan(channel, channels[page*8+channel].pan);
                break;
        case 1: // Trim mode
                sprintf(channels[page*8+channel].pad.TopText,"TRIM");
                board->SetDialLevel(channel, channels[page*8+channel].trimlevel);
                break;
        case 2: // Colour mode
                sprintf(channels[page*8+channel].pad.TopText,"Col");
                board->SetDialLevel(channel, 0);
                break;
        default: break;
    }
    board->SetScribble(channel,channels[page*8+channel].pad);
}

void RenderLEDS(XTouch *board, int channel) {
    if (channels[page*8+channel].rec>0) {
        board->SetSingleButton(0+channel,FLASHING);
    } else {
        board->SetSingleButton(0+channel,OFF);
    }
    if (channels[page*8+channel].solo>0) {
        board->SetSingleButton(8+channel,ON);
    } else {
        board->SetSingleButton(8+channel,OFF);
    }
    if (channels[page*8+channel].mute>0) {
        board->SetSingleButton(16+channel,ON);
    } else {
        board->SetSingleButton(16+channel,OFF);
    }
}

void RenderSelectedButton(XTouch *board) {
    int i;
    for(i=0;i<8;i++) {
        if (selected==page*8+i) {
            board->SetSingleButton(24+i,ON);
        } else {
            board->SetSingleButton(24+i,OFF);
        }
    }
}

void RenderPageAndSelected(XTouch *board) {
    board->SetAssignment(page+1);
    board->SetFrames(selected+1);
}

void RenderPage(XTouch *board) {
    int i;

    for(i=0;i<8;i++) {
        RenderDial(board, i);
        RenderLEDS(board,i);
        board->SetFaderLevel(i,channels[page*8+i].mainlevel);
    }
    board->SetFaderLevel(8,masterlevel);
    RenderPageAndSelected(board);
    RenderSelectedButton(board);
}

void sendpacket(void *socket, unsigned char *buffer, unsigned int len) {
    socketinfo_t *udpsocket=(socketinfo_t *)socket;
    sendto(udpsocket->sockfd, buffer, len, 0, (struct sockaddr *) &(udpsocket->clientaddr), (udpsocket->clientlen));
}

void buttonpressed(void *data, unsigned char button, int value)
{
    int channel;
    int t;
    if (value) {
        printf("Button %d pressed\n",button);
    } else {
        printf("Button %d released\n",button);        
    }
    if (button<40) {
        // For rec / solo / mute / select buttons handle these explicitly
        if (value) {
            channel=button%8;
            t=button/8;
            switch(t) {
                case 0: // Rec
                        channels[page*8+channel].rec=1-channels[page*8+channel].rec;
                        break;
                case 1: // Solo
                        channels[page*8+channel].solo=1-channels[page*8+channel].solo;
                        break;
                case 2: // Mute
                        channels[page*8+channel].mute=1-channels[page*8+channel].mute;
                        break;
                case 3: // Select
                        selected=page*8+channel;
                        RenderSelectedButton((XTouch*)data);
                        RenderPageAndSelected((XTouch*)data);
                        break;
                case 4: // Pressing the dials at the top
                        channels[page*8+channel].mode++;
                        if (channels[page*8+channel].mode>2) channels[page*8+channel].mode=0;
                        RenderDial((XTouch*)data,channel);            
                        break;
            }
            RenderLEDS((XTouch*)data,channel);
        }
    } else {
        // For other buttons light the button whilst pressed
        if (value) {
            ((XTouch*)data)->SetSingleButton(button,ON);
        } else {
            ((XTouch*)data)->SetSingleButton(button,OFF);            
        }
        // Handle the paging buttons
        if (value) {
            if (button==46) {
                // Previous page
                if (page>0) page--;
                RenderPage((XTouch*)data);
            }
            if (button==47) {
                // Next page
                if (page<7) page++;
                RenderPage((XTouch*)data);
            }
        }
    }
    
}

void fadertouch(void *data, unsigned char fader, int value)
{
    if (value) {
        printf("Fader %d pressed\n",fader);
    } else {
        printf("Fader %d released\n",fader);
        if (fader<8) {
            ((XTouch*)data)->SetFaderLevel(fader,channels[page*8+fader].mainlevel);
        } else {
            ((XTouch*)data)->SetFaderLevel(fader,masterlevel);            
        }
    }
}

void faderlevel(void *data, unsigned char fader, int value)
{
    printf("Fader %d level %d\n",fader, value);
    if (fader<8) {
        channels[page*8+fader].mainlevel=value;
    } else {
        masterlevel=value;
    }
}

void dial(void *data, unsigned char dial, int value)
{
    int channel;
    if (value>0) {
        printf("Dial %d clockwise by %d clicks\n", dial, value);
    } else {
        printf("Dial %d anti-clockwise by %d clicks\n", dial, 0-value);
    }
    if ((dial>15)&&(dial<24)) {
        channel=dial-16;
        if (value>0) {
            switch (channels[page*8+channel].mode) {
                case 0: // Pan
                        if (channels[page*8+channel].pan<6)  channels[page*8+channel].pan++;
                        break;
                case 1: // Trim
                        if (channels[page*8+channel].trimlevel<13)  channels[page*8+channel].trimlevel++;
                        break;
                case 2: // Colour
                        if (channels[page*8+channel].pad.Colour<WHITE) channels[page*8+channel].pad.Colour=(xt_colours_t)(((int)(channels[page*8+channel].pad.Colour))+1);
                        break;
            }
        } else {
            switch (channels[page*8+channel].mode) {
                case 0: // Pan
                        if (channels[page*8+channel].pan>-6)  channels[page*8+channel].pan--;
                        break;
                case 1: // Trim
                        if (channels[page*8+channel].trimlevel>0)  channels[page*8+channel].trimlevel--;
                        break;
                case 2: // Colour
                        if (channels[page*8+channel].pad.Colour>BLACK) channels[page*8+channel].pad.Colour=(xt_colours_t)(((int)(channels[page*8+channel].pad.Colour))-1);
                        break;
            }            
        }
        RenderDial((XTouch*)data, channel);
    }
    if (dial==60) {
        if (value>0) {
            if (selected<63) selected++;
        } else {
            if (selected>0) selected--;            
        }
        RenderPageAndSelected((XTouch*)data);
        RenderSelectedButton((XTouch*)data);
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in serveraddr;
    unsigned char recvbuf[BUFSIZE];
    int optval;
    int recvlen;
    int i;
    time_t now;
    struct tm* localtm;

    socketinfo_t udpsocket;

    // ------------------------------------------------------------------------------------
    // Perform socket related initilisations
    udpsocket.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpsocket.sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    optval = 1;
    setsockopt(udpsocket.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)10111);

    if (bind(udpsocket.sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    udpsocket.clientlen = sizeof(udpsocket.clientaddr);
    // ------------------------------------------------------------------------------------

    // Init stuff related to being a pretend desk (just to show button functionality)
    for(i=0;i<64;i++) {
        channels[i].mainlevel=0;
        channels[i].mute=1;
        channels[i].trimlevel=10;
        channels[i].pan=0;
        channels[i].solo=0;
        channels[i].rec=0;
        channels[i].pad.Colour=WHITE;
        channels[i].pad.Inverted=0;
        channels[i].mode=0;
        sprintf(channels[i].pad.TopText," ");
        sprintf(channels[i].pad.BotText,"Ch %d",i+1);        
    }

    XTouch FaderBoard(sendpacket,(void*)&udpsocket);

    FaderBoard.RegisterButtonCallback(buttonpressed, (void*)&FaderBoard);
    FaderBoard.RegisterFaderCallback(faderlevel,(void*)&FaderBoard);
    FaderBoard.RegisterFaderStateCallback(fadertouch,(void*)&FaderBoard);
    FaderBoard.RegisterDialCallback(dial,(void*)&FaderBoard);

    RenderPage(&FaderBoard);

    // The main packet processing loop
    while (1) {

        memset(recvbuf, 0, BUFSIZE);
        recvlen = recvfrom(udpsocket.sockfd, recvbuf, BUFSIZE, 0, (struct sockaddr *) &(udpsocket.clientaddr), &(udpsocket.clientlen));
        if (recvlen < 0) {
            perror("ERROR in recvfrom");
            exit(1);
        }

        now = time(0);
        localtm = localtime(&now);
        FaderBoard.SetTime(localtm);
        FaderBoard.HandlePacket(recvbuf,recvlen);
    }
}
