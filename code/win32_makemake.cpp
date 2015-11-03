/*
Create a header here for SVN to fill in later.
 */

#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <sstream>

// Used for now to convert to LPCSTR in order to support OutputDebugString();
#ifdef _UNICODE
std::wostringstream oss;
#else
std::ostringstream oss;
#endif

#define internal static
#define local_persist static 
#define global_variable static

struct Win32_Offscreen_Buffer
{
	BITMAPINFO info = {};
	void *memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel = 4;
};

struct Win32_Window_Dimension
{
	int width;
	int height;
};

/* How to import functions from XInput.lib without actually telling the linker 
   to include it in our build file (when compiling). This is due to the fact that 
   different lib's are used on different Windows platforms, and we don't want the
   game to not be playable without a gamepad (it won't load if the current platform doesn't
   support the required .lib and .dll 
   */

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32_LoadXInput()
{
	// TODO: Test this on Windows 8 (only has 1_4).
	HMODULE xInputLib = LoadLibrary("xinput1_3.dll");
	if (!xInputLib)
	{
		xInputLib = LoadLibrary("xinput1_4.dll");
	}

	if (xInputLib)
	{
		 XInputGetState = (x_input_get_state *)GetProcAddress(xInputLib, "XInputGetState");
		 if (!XInputGetState)
		 {
			 XInputGetState = XInputGetStateStub;
		 }
		 else
		 {
			 // Diagnostics
		 }

		 XInputSetState = (x_input_set_state *)GetProcAddress(xInputLib, "XInputSetState");
		 if (!XInputSetState)
		 {
			 XInputSetState = XInputSetStateStub;
		 }
		 else
		 {
			 // Diagnostics
		 }
	}
	else
	{
		// Some diagnostics 
	}
}

internal void Win32_InitDirectSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
	// Load the library
	HMODULE dSoundLib = LoadLibrary("dsound.dll");

	if (dSoundLib)
	{
		// Get a DirectSound object! - cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(dSoundLib, "DirectSoundCreate");

		// Doublecheck that this works on XP - DirectSound8 or 7?
		LPDIRECTSOUND directSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
		{
			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM; //WAVE_FORMAT_44S16;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = SamplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels*waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec*waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			if (SUCCEEDED(directSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				// Create a primary buffer (NOT REALLY A BUFFER, JUST TO SET THE SOUND CARD)
				DSBUFFERDESC bufferDescription = {};
				bufferDescription.dwSize = sizeof(bufferDescription);
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// BSDCAPS_GLOBALFOCUS?
				LPDIRECTSOUNDBUFFER primaryBuffer;
				HRESULT result;
				if (SUCCEEDED(result = directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
				{
					if (SUCCEEDED(result = primaryBuffer->SetFormat(&waveFormat)))
					{
						OutputDebugString("Primary buffer format set\n");
					}
					else
					{
						// Diagnostics
						oss << result;
						OutputDebugString("Error setting primary buffer format: ");
						OutputDebugString(oss.str().c_str());
						OutputDebugString("\n");
					}

				}
				else
				{
					// Diagnostics
					oss << result;
					OutputDebugString("Error setting primary buffer format: ");
					OutputDebugString(oss.str().c_str());
					OutputDebugString("\n");
				}
			}
			else
			{
				// Diagnostics
			}

			// TODO: DSBCAPS_GETCURRENTPOSITION2
			// Create a secondary buffer (which we will write to)
			DSBUFFERDESC bufferDescription = {};
			bufferDescription.dwSize = sizeof(bufferDescription);
			bufferDescription.dwFlags = 0;
			bufferDescription.dwBufferBytes = BufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;

			LPDIRECTSOUNDBUFFER secondaryBuffer;
			HRESULT result;
			if (SUCCEEDED(result = directSound->CreateSoundBuffer(&bufferDescription, &secondaryBuffer, 0)))
			{
				OutputDebugString("Secondary buffer created successfully.\n");
			}
			else
			{
				// Diagnostics
				oss << result;
				OutputDebugString("Error setting primary buffer format: ");
				OutputDebugString(oss.str().c_str());
				OutputDebugString("\n");
			}
		}
		else
		{
			// Diagnostics
		}
	}
	else
	{
		// Diagnostics
	}
}

internal Win32_Window_Dimension Win32_GetWindowDimension(HWND Window)
{
	RECT clientRect;
	Win32_Window_Dimension window = {};

	GetClientRect(Window, &clientRect);
	window.width = clientRect.right - clientRect.left;
	window.height = clientRect.bottom - clientRect.top;

	return window;
}

// Global for now
global_variable bool Running;
global_variable Win32_Offscreen_Buffer GlobalBackbuffer;

internal void RenderWeirdGradient(Win32_Offscreen_Buffer *Buffer, int BlueOffset, int GreenOffset)
{
	// TODO: See what the optimizer does
	uint8_t *Row = (uint8_t *)Buffer->memory;
	for (int y = 0; y < Buffer->height; ++y)
	{
		uint32_t *Pixel = (uint32_t *)Row;
		for (int x = 0; x < Buffer->width; ++x)
		{
			uint8_t blue = (x + BlueOffset);
			uint8_t green = (y + GreenOffset);
			uint8_t red = 0;
			uint8_t padding = 0;

			/*
				Memory:    BB GG RR xx  (little endian)
				Register:  xx RR GG BB
			*/

			*Pixel++ = ((padding << 24 | red << 16 | green << 8) | blue);
		}
		Row +=  Buffer->width*Buffer->bytesPerPixel; // This is the pitch. Set this in the Buffer struct later.
	}
}

internal void RenderUserMovement(Win32_Offscreen_Buffer *Buffer, int X, int Y, int Width, int Height)
{
	uint8_t *Row = (uint8_t *)Buffer->memory;
	Row += (Buffer->width*Buffer->bytesPerPixel*Y);
	for (int y = 0; y < Height; ++y)
	{
		uint32_t *Pixel = (uint32_t *)Row + X;
		for (int x = 0; x < Width; ++x)
		{
			uint8_t blue = 0;
			uint8_t green = 0;
			uint8_t red = 0;
			uint8_t padding = 0;

			*Pixel++ = ((padding << 24 | red << 16 | green << 8) | blue);
		}
		Row += Buffer->width*Buffer->bytesPerPixel;
	}
}

internal void Win32_ResizeDIBSection(Win32_Offscreen_Buffer *Buffer, int Width, int Height)
{
	// TODO: Bulletproof this
	// Maybe don't free first, free after, then free first if that fails.
	
	// Freeing memory since we will later allocate (we don't want a memory leak do we?)
	// GOODTOKNOW: to find use after free errors (which are hard to find): set memory to PAGE_NOACCESS
	if (Buffer->memory)
	{
		VirtualFree(Buffer->memory, 0, MEM_RELEASE);
	}

	Buffer->width = Width;
	Buffer->height = Height;

	// Note: the negative biHeight field is a clue to Windows to treat this bitmap as top-down, not bottom-up.
	// Meaning that the first three bytes of the image are the color for the top left pixel in the bitmap, not the bottom left.
	Buffer->info.bmiHeader.biSize = sizeof(Buffer->info.bmiHeader);
	Buffer->info.bmiHeader.biWidth = Width;
	Buffer->info.bmiHeader.biHeight = -Height;
	Buffer->info.bmiHeader.biPlanes = 1;
	Buffer->info.bmiHeader.biBitCount = 32; // Only using 24 bits, but requesting 32 to align with 4 byte values. (1 byte padding)
	Buffer->info.bmiHeader.biCompression = BI_RGB;

	// NOTE: Thank you to Chris Hecker of Spy Party fame
	// for clarifying the deal with the StretchDIBits and BitBlt!
	// NO more DC for us.

	int BitmapMemorySize = (Buffer->width*Buffer->height)*Buffer->bytesPerPixel;

	// TODO: Do something more when allocation fails
	if ((Buffer->memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)) == 0)
	{
		// TODO: use GetLastError()
		// DWORD -> string....
		MessageBox(
			0,
			"There was an error allocating BitmapMemory"/* + GetLastError()*/,
			"MakeMake : Error",
			MB_OK | MB_ICONERROR);

		// TODO: Make a better way to handle.
		return;
	}

	//TODO: Clear to black when resize
}

internal void Win32_DisplayBufferInWindow(HDC DeviceContext, Win32_Offscreen_Buffer *Buffer, int WindowWidth, int WindowHeight)
{
	// TODO: Aspect ratio correction (or just lock the window size?)
	StretchDIBits(DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->width, Buffer->height,
		Buffer->memory,
		&Buffer->info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32_MainWindowCallback(
	HWND   Window,
	UINT   Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT result = 0;

	switch (Message)
	{
		case WM_SIZE:
		{
			//Win32_Window_Dimension dim = Win32_GetWindowDimension(Window);
			//Win32_ResizeDIBSection(&GlobalBackbuffer, dim.width, dim.height);
			OutputDebugString("WM_SIZE\n");
		} break;

		case WM_CLOSE:
		{
			// TODO: Handle this with a message to the user?
			Running = false;
		} break;

		case WM_DESTROY:
		{
			// TODO: Handle this as an error - recreate window?
			Running = false;
		} break;

		case WM_SYSKEYDOWN:
		{
		}
		case WM_SYSKEYUP:
		{
		}
		case WM_KEYDOWN:
		{
		}
		case WM_KEYUP:
		{
			uint32_t vkCode = WParam;
			bool wasKeyDown = ((LParam & (1 << 30)) != 0);
			bool isKeyDown = ((LParam & (1 << 31)) == 0);
			if (wasKeyDown != isKeyDown)
			{
				switch (vkCode)
				{
					case 'W':
					{
						// testing for was down and is down here
						OutputDebugString("W: ");
						if (wasKeyDown)
						{
							OutputDebugString("was down\n");
						}
						if (isKeyDown)
						{
							OutputDebugString("is down\n");
						}
					} break;

					case 'A':
					{
						OutputDebugString("A\n");
					} break;

					case 'S':
					{
						OutputDebugString("S\n");
					} break;

					case 'D':
					{
						OutputDebugString("D\n");
					} break;

					case VK_UP:
					{
						OutputDebugString("UP\n");
					} break;

					case VK_DOWN:
					{
						OutputDebugString("DOWN\n");
					} break;

					case VK_LEFT:
					{
						OutputDebugString("LEFT\n");
					} break;

					case VK_RIGHT:
					{
						OutputDebugString("RIGHT\n");
					} break;

					case VK_ESCAPE:
					{
						OutputDebugString("ESC\n");
					} break;

					case VK_SPACE:
					{
						OutputDebugString("SPC\n");
					} break;

					case VK_F4:
					{
						// Checking if alt key is pressed down.
						// TODO: alt+gr also works, figure out how to filter that key out.
						//       Check this page for information: http://blogs.msdn.com/b/murrays/archive/2015/04/20/hot-keys-and-altgr.aspx
						//       probably need to handle alt, ctrl and so on in WM_SYSKEYDOWN and UP to set state on them.
						if (LParam & (1 << 29))
						{
							Running = false;
						}
					} break;
				}
			}

		}break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			Win32_Window_Dimension dim = Win32_GetWindowDimension(Window);
			HDC deviceContext = BeginPaint(Window, &paint);

			Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);

			EndPaint(Window, &paint);
		} break;

		default:
		{
			//OutputDebugString("Default\n");
			result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return result;
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR CmdLine,
	int ShowCode)
{
	// For fun! Stack overflow! Guess we have a 2MB stack :)
	// Optimizer removes it from the stack if we don't clear it (initialize to 0 : = {})
	// Change stack size with VS C++ compiler with -Fxxxxxxx where x is stack size.
	// uint8_t BigOlBlock[2 * 1024 * 1024] = {};

	Win32_LoadXInput();
	WNDCLASS windowClass = {};

	Win32_ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32_MainWindowCallback;
	windowClass.hInstance = Instance;
	//windowClass.hIcon;
	windowClass.lpszMenuName = "MakeMake_MenuName";
	windowClass.lpszClassName = "MakeMake_WindowClass";

	if (RegisterClass(&windowClass))
	{
		HWND window =
			CreateWindowEx(0,
				windowClass.lpszClassName,
				"MakeMake",
				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0);

		if (window)
		{
			int xOffset = 0;
			int yOffset = 0;

			int playerXPos = 0;
			int playerYPos = 0;
			int playerWidth = 10;
			int playerHeight = 50;

			/* .xm and .mod format information, taken from VLC mediainformation:
			   Codec: PCM S16 LE (araw)    --> WAVE_FORMAT_44S16 (trying out WAVE_FORMAT_PCM)
			   Channels: Stereo            --> 2
			   Samplingfrequency: 44100 Hz --> 44.1 kHz
			   Bits per second: 16         --> int16_t
			*/
			// TODO: expand this function to be able to initialize for more formats (expand parameters)
			Win32_InitDirectSound(window, 44100, 44100*sizeof(int16_t)*2);

			Running = true;
			while (Running)
			{
				MSG message;
				while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
					{
						Running = false;
					}
					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				HDC deviceContext = GetDC(window);
				Win32_Window_Dimension dim = Win32_GetWindowDimension(window);

				// Gamepad input/output
				// TODO: Should we poll this more frequently?
				//       What about listening for input instead of polling?
				//       Can also discover when a controller is plugged in
				//       XInput lib has a bad performance bug: if no controller is plugged in when polling with
				//       XInputGetState it will stall for several ms (causing a lot of lost CPU time)
				for (int cIndex = 0; cIndex < XUSER_MAX_COUNT; ++cIndex)
				{
					XINPUT_STATE controllerState;
					if (XInputGetState(cIndex, &controllerState) == ERROR_SUCCESS)
					{
						// Controller available
						XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

						bool dPadUp = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool dPadDown = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool dPadLeft = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool dPadRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
						bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
						bool leftThumb = (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
						bool rightThumb = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
						bool leftShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool a = (pad->wButtons & XINPUT_GAMEPAD_A);
						bool b = (pad->wButtons & XINPUT_GAMEPAD_B);
						bool x = (pad->wButtons & XINPUT_GAMEPAD_X);
						bool y = (pad->wButtons & XINPUT_GAMEPAD_Y);

						int16_t stickX = pad->sThumbLX;
						int16_t stickY = pad->sThumbLY;

						// dpad
						if (dPadUp)
						{
							//yOffset++;

							// Testing rumble, why controller 1 and not 0?
							//XINPUT_VIBRATION brrr;
							//brrr.wLeftMotorSpeed = 60000;
							//brrr.wRightMotorSpeed = 60000;
							//XInputSetState(1, &brrr);

							// Use another buffer !!
							if (playerYPos > 0)
							{
								playerYPos--;
								RenderUserMovement(&GlobalBackbuffer, playerXPos, playerYPos, playerWidth, playerHeight);
								Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);
							}
						}
						if (dPadDown)
						{
							//yOffset--;

							// Testing rumble
							//XINPUT_VIBRATION brrr;
							//brrr.wLeftMotorSpeed = 0;
							//brrr.wRightMotorSpeed = 0;
							//XInputSetState(1, &brrr);
							if (playerYPos < (dim.height - playerHeight))
							{

								/*
								TODO:
									Grunn til feil:
									tegner for langt i x retning, den looper litt rundt.
									spiser av plassen vi kan tegne i y retning, og klarer ikke å oppdage dette før vi
									skriver til minnelokasjon som vi ikke eier!
								*/
								playerYPos++;
								RenderUserMovement(&GlobalBackbuffer, playerXPos, playerYPos, playerWidth, playerHeight);
								Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);

								OutputDebugString("Moving Down\n");

								oss << playerYPos;
								OutputDebugString("playerYPos: ");
								OutputDebugString(oss.str().c_str());
								OutputDebugString("\n");
								oss.str(std::string());

								oss << dim.height;
								OutputDebugString("dim.height: ");
								OutputDebugString(oss.str().c_str());
								OutputDebugString("\n");
								oss.str(std::string());
							}
						}
						if (dPadLeft)
						{
							//xOffset++;

							if (playerXPos > 0)
							{
								playerXPos--;
								RenderUserMovement(&GlobalBackbuffer, playerXPos, playerYPos, playerWidth, playerHeight);
								Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);
							}
						}
						if (dPadRight)
						{
							//xOffset--;

							if (playerXPos < (dim.width - playerWidth))
							{
								playerXPos++;
								RenderUserMovement(&GlobalBackbuffer, playerXPos, playerYPos, playerWidth, playerHeight);
								Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);

								OutputDebugString("Moving Right\n");

								oss << playerXPos;
								OutputDebugString("playerXPos: ");
								OutputDebugString(oss.str().c_str());
								OutputDebugString("\n");
								oss.str(std::string());

								oss << dim.width;
								OutputDebugString("dim.width: ");
								OutputDebugString(oss.str().c_str());
								OutputDebugString("\n");
								oss.str(std::string());
							}
						}

						// stick right
						if (stickX > 10000)
						{
							xOffset -= (int16_t)(stickX / 5000);
						}
						// stick left
						else if (stickX < -10000)
						{
							xOffset -= (int16_t)(stickX / 5000);
						}

						// stick up
						if (stickY > 10000)
						{
							yOffset += (int16_t)(stickY / 5000);
						}
						// stick down
						else if (stickY < -10000)
						{
							yOffset += (int16_t)(stickY / 5000);
						}

						
					}
					else
					{
						// Controller not available
					}
				}

				RenderWeirdGradient(&GlobalBackbuffer, xOffset, yOffset);
				 
				// Moved HDC deviceContext = GetDC(window); before gamepad input.
				// Moved Win32_Window_Dimension dim = Win32_GetWindowDimension(window); before gamepad input.
				Win32_DisplayBufferInWindow(deviceContext, &GlobalBackbuffer, dim.width, dim.height);
				ReleaseDC(window, deviceContext);
				
				//++xOffset;
				//--yOffset;
			}
		}
		else
		{
			// TODO: Logging in case of fauil.
		}
	}
	else
	{
		// TODO: Loggin in case of fail
	}

	return(0);
}
