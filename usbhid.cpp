// usbhid.cpp: главный файл проекта.

#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <Oleacc.h>
#include <oleauto.h>
#include <iostream>
//#include <string.h>
#include <string>

//#include <Commctrl.h>
#include <iostream>
#include <fstream>
#include <math.h>

#include "well512.h"

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Oleacc.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace System;

const int KEYS_MAX=256;
const char KBD_MAX = 5;
DWORD keymap[KEYS_MAX];
double keydown_time[KBD_MAX][KEYS_MAX];
int keydown_index[KBD_MAX][KEYS_MAX];

HANDLE hStdin;
HANDLE hStdout;

HWINEVENTHOOK g_winevent_hook;
int g_con_win_height;

LARGE_INTEGER freq,timer1,timer2;
double conv;

HMIDIOUT midiHandle;

const int MIDI_NOTE_VEL_MIN_TIME = 10;
const int MIDI_NOTE_VEL_MAX_TIME = 350;
const int MIDI_NOTE_VEL_MIN_OUT = 15;

const int INFO_SUSTAIN_OFFSET_X = 74;

int octave_shift = 2;
HANDLE hKbd_handles[KBD_MAX]={NULL,};
char cKbd_count = 0;

void MoveCursor( short x, short y);

int get_kbd_index(HANDLE hDevice){
	for(int i=0;i<cKbd_count;i++) {
		if(hKbd_handles[i] == hDevice) {
			return i;
		}
	}

	MoveCursor(0,-1);
	printf("     New Keyboard: %u     \r", hDevice);
	MoveCursor(0,1);

	hKbd_handles[cKbd_count++] = hDevice;
	return cKbd_count-1;
}
DWORD get_kbd_keymap( int scan_code, int kbd_index ) {
	return keymap[scan_code] + 3 * 12 * kbd_index;
}
BOOL bMidi_pedal_sustain_down = false;

char const str_notes[] = "C \0C#\0D \0D#\0E \0F \0F#\0G \0G#\0A \0A#\0B \0";

BOOL can_suspend = false;

WORD conDefFontAttributes;

void con_set_font_attributes( WORD attribs ) {
	SetConsoleTextAttribute( hStdout, attribs == -1?conDefFontAttributes : attribs );
}
void con_set_font_attributes(  ) {
	SetConsoleTextAttribute( hStdout, conDefFontAttributes );
}

void midi_send_controller(int controller_id, int controller_velocity) {
	static int msg;
	if(controller_velocity > 127) {
		controller_velocity = 127;
	}else if(controller_velocity < 0) {
		controller_velocity = 0;
	}
	msg = 0xB0 | ( controller_velocity << 16) | (controller_id << 8);
	midiOutShortMsg(midiHandle, msg);
}
void midi_send_note(int velocity, int note_num, BOOL down) {
	static int msg;
	msg = ((velocity & 0x7f) << 16 ) | ((note_num + octave_shift*12) << 8) | ( down? 0x90 : 0x80 );

	midiOutShortMsg(midiHandle, msg);
}

int iPressed_keys = 0;
int iUpped_keys = 0;
int iDowned_keys = 0;

int iConBufferShiftedY = 0;

HANDLE hThreadUpdate;

void MoveCursor( short x, short y) {
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	GetConsoleScreenBufferInfo( hStdout, &conInfo );
	//printf("xy: %hu %hu\n", conInfo.dwCursorPosition.X, conInfo.dwCursorPosition.Y);
	//printf("XY: %hu %hu\n", conInfo.dwSize.X, conInfo.dwSize.Y);

	if( y < -g_con_win_height) {
		y = -g_con_win_height;
	}
	if( y >= g_con_win_height) {
		y = g_con_win_height;
	}
	conInfo.dwCursorPosition.X += x;
	conInfo.dwCursorPosition.Y += y;
	SetConsoleCursorPosition( hStdout, conInfo.dwCursorPosition);
}
int iGet_midi_vel( int scan_code, int kbd_index ) {
	int midi_vel = static_cast< int >(Math::Round(  (conv*(timer1.QuadPart - keydown_time[kbd_index][ scan_code ]) - MIDI_NOTE_VEL_MIN_TIME)*127/MIDI_NOTE_VEL_MAX_TIME ));

	if(midi_vel < 0) {
		midi_vel = 0;
	} else if(midi_vel > 127-MIDI_NOTE_VEL_MIN_OUT) {
		midi_vel = 127-MIDI_NOTE_VEL_MIN_OUT;
	}
	midi_vel = 127-midi_vel;

	return midi_vel;
}

int print_key_info( int scan_code, int kbd_index ) {
	std::string info;
	QueryPerformanceCounter(&timer1);
	
	int midi_vel = iGet_midi_vel( scan_code, kbd_index );
	int i;
	/*for(i=0;i<7;i++) {
		if( i == (keymap[scan_code] % 12) ) {
			info += 'N';
		} else {
			info += '-';
		}
	}*/
	switch( (keymap[scan_code] % 12) ){
		case  0:info += "C------";break;
		case  1:info += "C#-----";break;
		case  2:info += "-D-----";break;
		case  3:info += "-D#----";break;
		case  4:info += "--E----";break;
		case  5:info += "---F---";break;
		case  6:info += "---F#--";break;
		case  7:info += "----G--";break;
		case  8:info += "----G#-";break;
		case  9:info += "-----A-";break;
		case 10:info += "-----A#";break;
		case 11:info += "------B";break;
	}
	info += " [";
	for(i=0;i<32;i++) {
		if( midi_vel/4 > i) {
			info += '|';
		} else {
			info += ' ';
		}
	}
	double time = conv*(timer1.QuadPart - keydown_time[kbd_index][ scan_code ]);
	/*if( midi_vel <= MIDI_NOTE_VEL_MIN_OUT){
		time = 999.9;
	}*/
	div_t divresult = div(keymap[scan_code], 12);
	int octave = octave_shift + divresult.quot + kbd_index * 3;
	printf("%s%i %s] %6.1f ms. Vel.: %3i kbd:%i\r", &str_notes[(keymap[scan_code] % 12)*3], octave, info.c_str(), time, midi_vel, kbd_index);


	return midi_vel;
}

int get_random_int(int min_v, int max_v) {
	return int(min_v + (max_v - min_v + .5)*WELLRNG512a());
}


bool g_bDebug_keys = false;

//Handle USB_HID messages
void CheckKeyboardProc( HANDLE hDevice, int scan_code , bool bUp ) {
	while( !can_suspend ) {
		Sleep(1);
	}
	SuspendThread( hThreadUpdate );
	static int midi_vel, i;

	int kbd_index = get_kbd_index( hDevice );

	if( ( scan_code >= 0) && (scan_code < 256)) {
		if( keymap[scan_code] != 0) {
			if( bUp ) {
				//keydown_time[ scan_code ] = iPressed_keys;
				int c;
				c = keydown_index[kbd_index][ scan_code ];
				int iUp = ( iPressed_keys - c + iUpped_keys );
				MoveCursor( 0, -iUp);
				//printf("kUP: %i upped:%i pressed:%i downed:%i\r", c, iUpped_keys, iPressed_keys, iDowned_keys);
					
				keydown_index[kbd_index][ scan_code ] = -1;

				con_set_font_attributes( );
				print_key_info( scan_code, kbd_index );

				MoveCursor( 0, iUp );
				iPressed_keys--;
				if(iPressed_keys>0) {
					iUpped_keys++;
				}else {
					iUpped_keys = 0;
				}
			}
			//key pressed first time
			if( !bUp && keydown_time[kbd_index][ scan_code ] == 0) {
					
				QueryPerformanceCounter(&timer1);
				keydown_time[kbd_index][ scan_code ] = static_cast<double>(timer1.QuadPart);
					
				iPressed_keys++;
				//MoveCursor( 0, -1);
				//printf("Pressed:%i\n",iPressed_keys);
				if(iPressed_keys > 1) {
					iConBufferShiftedY++;
				} else {
					iConBufferShiftedY = 0;
				}
				keydown_index[kbd_index][ scan_code ] = iConBufferShiftedY;
				//printf("kDN: pressed:%i shifted:%i\n", iPressed_keys, iConBufferShiftedY);
				print_key_info( scan_code, kbd_index );
				printf("\n");
			} else if( bUp  ) {
				
				int midi_vel = iGet_midi_vel( scan_code, kbd_index );
					
				keydown_time[kbd_index][ scan_code ] = 0.0;
				midi_send_note( midi_vel, (int)get_kbd_keymap( scan_code, kbd_index ), TRUE);
				midi_send_note( 0,(int)get_kbd_keymap( scan_code, kbd_index ), FALSE);
			}
		}
	}
	if( scan_code == 57 ) {
		if( bUp) {
			midi_send_controller(64, 127);
			MoveCursor( INFO_SUSTAIN_OFFSET_X , -1);
			printf("Sustain pedal DOWN.\n");
			//MoveCursor( 0 , 0);
			bMidi_pedal_sustain_down = FALSE;
			//iPressed_keys++;
			//iConBufferShiftedY++;
		} else if( !bMidi_pedal_sustain_down ) {
			midi_send_controller(64, 0);
			MoveCursor( INFO_SUSTAIN_OFFSET_X , -1);
			printf("Sustain pedal UP.   \n");
			//MoveCursor( 0 , 0);
			bMidi_pedal_sustain_down = TRUE;
			//iConBufferShiftedY++;
			//iPressed_keys++;
		}

	}else if( scan_code == 74 && bUp) { /*numpad -*/
		octave_shift++;
	}else if ( scan_code == 78 && bUp ){ /*numpad +*/
		octave_shift--;
		/*
		CONSOLE_SCREEN_BUFFER_INFO conInfo;
		GetConsoleScreenBufferInfo( hStdout, &conInfo );
		printf("win max size: %i,%i %i,%i\n",conInfo.srWindow.Left, conInfo.srWindow.Top, conInfo.srWindow.Right, conInfo.srWindow.Bottom);
		*/
	}
		
	if( ( scan_code == 1 || scan_code == 66) && bUp ) { /*ESC or F8*/
		PostQuitMessage(0);
	}
		
	if( scan_code == 67 && !bUp ) { /*F9*/

		int random_note = get_random_int(24,24+24);

		//printf("%i\n", random_note );
		div_t divresult = div(random_note, 12);
		int octave = divresult.quot;

		printf("%s%i\n",&str_notes[(random_note % 12)*3], octave);

		midi_send_note( get_random_int(30,90), random_note, TRUE);
		midi_send_note( 0,random_note, FALSE);
		//print_key_info(random_note, 0);
		//MoveCursor( 0 , 1);
	}
	
	if(scan_code == 59 && bUp) {
		g_bDebug_keys = !g_bDebug_keys;
		printf("Debug key scan_codes %s",(g_bDebug_keys?"enabled.\n":"disabled.\n"));
	}
	FlushConsoleInputBuffer( hStdin );
	//printf("Flushed: %i\n",FlushConsoleInputBuffer( hStdin ) );
	
	if(g_bDebug_keys) {
		printf("DEBUG: scan_code:%i\n", scan_code);
	}
	ResumeThread( hThreadUpdate );
}

void LoadKeyMap(const char _fname[]) {
	int i,j;

	unsigned int r;
	InitWELLRNG512a(&r);
	for(i=0;i<10;i++){
		WELLRNG512a();
	}

	printf("Loading keymap from: %s\n",_fname);
	
	for(j=0;j<KBD_MAX;j++){
		keymap[j] = 0;
		for(i=0;i<KEYS_MAX;i++) {
			keydown_time[j][i] = 0.0;
			keydown_index[j][i] = -1;
		}
	}
	std::string str;
	std::ifstream myfile(_fname);
	
	int a = 0;
	int b = 0;
	if(!myfile) //Always test the file open.
	{
		printf("Error opening file \"%s\"\n",_fname);
		system("pause");
		return;
	}
	size_t pos_space;
	while(!myfile.eof())
	{
		getline(myfile, str,'\n');
		
		pos_space = str.find_first_of(' ',0);
		if(pos_space == -1) {
			continue;
		}
		keymap[atoi(str.substr(0,pos_space).c_str())] = atoi(str.substr(pos_space).c_str());
		//printf("line: %s %s\n", str.substr(0,pos_space).c_str(), str.substr(pos_space).c_str());
	}
	myfile.close();
	
	for(i=0;i<KEYS_MAX;i++) {
		if(keymap[i] == 0)
			continue;
		printf("[%i -> %i]\t", i, keymap[i]);
	}
	printf("\n");
}

DWORD WINAPI procUpdateThread( LPVOID lpParam ) {
	int i,j;
	while( 1 ) {
		can_suspend = false;
		for(j=0;j<cKbd_count;j++) {
			for(i=0;i<KEYS_MAX;i++) {
				if( keydown_index[j][i] != -1) {
					int c;
					c = keydown_index[j][ i ];
					int iUp = ( iConBufferShiftedY - c + 1 );
				
					//QueryPerformanceCounter(&timer1);
					//int midi_vel = static_cast<int>(Math::Round(  (conv*(timer1.QuadPart - keydown_time[j][ i ]) - MIDI_NOTE_VEL_MIN_TIME)*127/MIDI_NOTE_VEL_MAX_TIME ));

					MoveCursor( 0, -iUp);
					con_set_font_attributes( FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY );
					//printf("put:%i %i\r", i, midi_vel);
					print_key_info( i , j);
					con_set_font_attributes( );
					MoveCursor( 0, iUp);
				}
			}
		}
		can_suspend = true;
		Sleep(10);
	}
	return 0;
}
void vUpdateConWinHeight(){
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	GetConsoleScreenBufferInfo( hStdout, &conInfo );
	g_con_win_height = conInfo.srWindow.Bottom - conInfo.srWindow.Top;
}
void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
	IAccessible* pAcc = NULL;
    VARIANT varChild;
    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &pAcc, &varChild);  
    if ((hr == S_OK) && (pAcc != NULL))
    {
		if( event == EVENT_CONSOLE_LAYOUT ) {
			vUpdateConWinHeight();
			//printf("g_con_win_height (%i)\n",  g_con_win_height);
		}
        //printf("WINEVENT (%x)\n",  event);
        pAcc->Release();
    }
}


int get_midi_yoke_num() {
	int midi_out_devs = midiOutGetNumDevs();
	int midi_yoke_1_devnum = -1;
	printf("Midi Out Devices: %i\n", midi_out_devs);
	if(midi_out_devs == 0) {
		printf("error: no output midi devices found.(install midi-YOKE!)");
		return -1;
	}

	MIDIOUTCAPS dev_caps;
	for(int i=0;i<midi_out_devs;i++){
		ZeroMemory(&dev_caps, sizeof(dev_caps));
		midiOutGetDevCaps(i, &dev_caps, sizeof(dev_caps));
		printf("%i: %ws\n", i, dev_caps.szPname);
		if( wcscmp(dev_caps.szPname, TEXT("Out To MIDI Yoke:  1")) == 0) {
			midi_yoke_1_devnum = i;
			printf("DEBUG: midi-YOKE found!\n");
		}
	}
	return midi_yoke_1_devnum;
}

void init_midi()
{
	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	GetConsoleScreenBufferInfo( hStdout, &conInfo );
	conDefFontAttributes = conInfo.wAttributes;
	
	printf("Window max size: %i,%i %i,%i\n",conInfo.srWindow.Left, conInfo.srWindow.Top, conInfo.srWindow.Right, conInfo.srWindow.Bottom);

	QueryPerformanceFrequency(&freq);
	conv = 1000.0 / freq.QuadPart;
	con_set_font_attributes(FOREGROUND_GREEN);
	printf("Performance counter freq.: %llu ticks per second\n",freq.QuadPart);
	con_set_font_attributes();
	
	printf("DEBUG: (err:%i) Input state:%i\n",  GetLastError(), GetInputState());
	
	hThreadUpdate =  CreateThread( NULL, 0, procUpdateThread, 0, CREATE_SUSPENDED, NULL);
	if( hThreadUpdate == NULL ) {
		printf("Can't create thread.\n");
		ExitProcess(0);
	}

	//find midi-YOKE 1 device by name
	int midi_yoke_num = get_midi_yoke_num();
	if(midi_yoke_num == -1) {
			exit(1);
	}
	LoadKeyMap("midi_pc101.txt");

	ResumeThread( hThreadUpdate );

	if( midiOutOpen(&midiHandle, (UINT)midi_yoke_num, 0, 0, CALLBACK_NULL) != 0 ) {
		printf("ERROR opening midi device.");
		exit(1);
	}
}
void free_midi() {
	printf("Midi: Exiting gracefully.\n");
	midiOutClose( midiHandle );
}


void Error() {
	printf("Something BAD happens;\n");
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SetActiveWindow(hwnd);
    switch(msg)
    {
		case WM_INPUT:{

			UINT dwSize;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, 
							sizeof(RAWINPUTHEADER));
			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == NULL) 
			{
				return 0;
			} 

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, 
				 sizeof(RAWINPUTHEADER)) != dwSize )
				 OutputDebugString (TEXT("GetRawInputData does not return correct size !\n")); 

			RAWINPUT* raw = (RAWINPUT*)lpb;
			//printf("Device: %u",raw->header.hDevice);
			if (false && raw->header.dwType == RIM_TYPEKEYBOARD) 
			{
				printf(" Kbd: make=%04hx Flags:%04hx Reserved:%04hx ExtraInformation:%08hx, msg=%04hx VK=%04hx \n", 
					raw->data.keyboard.MakeCode, 
					raw->data.keyboard.Flags, 
					raw->data.keyboard.Reserved, 
					raw->data.keyboard.ExtraInformation, 
					raw->data.keyboard.Message, 
					raw->data.keyboard.VKey);
			}
			CheckKeyboardProc( raw->header.hDevice, raw->data.keyboard.MakeCode, ((raw->data.keyboard.Flags & RI_KEY_BREAK) == RI_KEY_BREAK));
			break;}
        case WM_CLOSE:
            DestroyWindow(hwnd);
        break;
        case WM_DESTROY:
            PostQuitMessage(0);
        break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
	
}

int main(array<System::String ^> ^args)
{
    //Console::WriteLine(L"Здравствуй, мир!");
	init_midi();

	g_winevent_hook = SetWinEventHook(EVENT_CONSOLE_CARET, EVENT_CONSOLE_END_APPLICATION, NULL, HandleWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	vUpdateConWinHeight();

	UINT nDevices;
	PRAWINPUTDEVICELIST pRawInputDeviceList;
	
	if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0)
		{ Error();}
	printf("USB-HID Devices: %d\n",nDevices);
	if ((pRawInputDeviceList = (PRAWINPUTDEVICELIST)malloc(sizeof(RAWINPUTDEVICELIST) * nDevices)) == NULL)
		{Error();}
	if (GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST)) == -1)
		{Error();}
	
	//printf("Devices2: %d\n",nDevices);

	int *pData;
	UINT pcbSize;

	for(int i=0; i<(int)nDevices ;i++) {
		printf("%u Dev id:%u type:%hu\n",i,pRawInputDeviceList[i].hDevice, pRawInputDeviceList[i].dwType);
		GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, 0, &pcbSize);
		//printf("size:%u\n",pcbSize);
		pData = (int*)malloc((int)pcbSize*2);
		GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, (LPVOID)pData, &pcbSize);

		std::wcout << (wchar_t*)pData <<std::endl;

		free(pData);

	}
	
	free(pRawInputDeviceList);

	WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

	HBRUSH backColor = CreateSolidBrush(RGB(0x7f,0x00,0x00));

    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = NULL;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = backColor;//(HBRUSH)COLOR_WINDOW;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = L"myWindowClass";
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
    if(!RegisterClassEx(&wc))
    {
        printf("RegisterClassEx FAILED\n");
    }
	LPCWSTR mmm = L"myWindowClass";
    // Step 2: Creating the Window
    hwnd = CreateWindowEx(
        NULL,
        L"myWindowClass",
        L"Focus Me and press keys...",
        WS_OVERLAPPED | WS_SYSMENU | WS_SIZEBOX,
        0, 500, 300, 100,
         HWND_DESKTOP, NULL, NULL, NULL); //HWND_MESSAGE
	
    if(hwnd == NULL)
    {
        printf("CreateWindowEx FAILED\n");
		system("pause");
		return 1;
    }
	
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
	SetActiveWindow(hwnd);

	RAWINPUTDEVICE Rid[1];
        
	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = 0x06; //keyboard
	Rid[0].dwFlags = RIDEV_NOLEGACY;
	Rid[0].hwndTarget = 0;

	if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
		printf("RegisterRawInputDevices FAILED.\n");
		system("pause");
		return 1;
	}

    while(GetMessage(&Msg, NULL, 0, 0) > 0){
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
	ShowWindow( hwnd, SW_HIDE );
	UpdateWindow( hwnd );
	UnhookWinEvent( g_winevent_hook );
	TerminateThread(hThreadUpdate, 0);
	CloseHandle( hThreadUpdate );
	free_midi();
	//system("pause");
	Sleep(1000);

    return 0;
}
