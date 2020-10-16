//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2020 Fabian Herb
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Demonstration program for OPL library to play back DRO
//     format files.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "opl3.h"

const char* HEADER_STRING = "DBRAWOPL";

static opl3_chip chip;
static SDL_mutex* chipMutex = NULL;
static SDL_AudioDeviceID audioDevice;

void WriteReg(int bank, unsigned int reg, unsigned int val)
{
	SDL_LockMutex(chipMutex);
	OPL3_WriteRegBuffered(&chip, (bank << 8) | reg, val);
	SDL_UnlockMutex(chipMutex);
}

void AudioCallback(void* userdata, Uint8* stream, int len)
{
	SDL_LockMutex(chipMutex);
	OPL3_GenerateStream(&chip, (Bit16s*)stream, len / 4);
	SDL_UnlockMutex(chipMutex);
}

void Init(void)
{
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
    {
		fprintf(stderr, "Unable to initialise SDL\n");
		exit(1);
    }

	chipMutex = SDL_CreateMutex();

	OPL3_Reset(&chip, 49716);

	SDL_AudioSpec want, have;

	SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
	want.freq = 48000;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 256;
	want.callback = AudioCallback;
	audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
	if(audioDevice == 0)
	{
		fprintf(stderr, "Unable to open audio device\n");
		exit(1);
	}
	SDL_PauseAudioDevice(audioDevice, 0);
}

void Exit()
{
	SDL_CloseAudioDevice(audioDevice);
	SDL_DestroyMutex(chipMutex);
	SDL_Quit();
}

struct Dro2Header
{
	uint32_t lengthPairs;
	uint32_t lengthMs;
	uint8_t hardwareType; // Flag listing the hardware used in the song
	uint8_t format; // Data arrangement
	uint8_t	compression; // Compression type, zero means no compression (currently only zero is used)
	uint8_t	shortDelayCode; // Command code for short delay (1-256ms)
	uint8_t	longDelayCode; // Command code for short delay (> 256ms)
	uint8_t	codemapLength; // Number of entries in codemap table
	uint8_t codemap[128];
};

struct timer_data
{
    int running;
	FILE* fstream;
	struct Dro2Header dro2Header;
	int dro1Bank;
};

Uint32 Dro1TimerCallback(Uint32 interval, void *data)
{
    struct timer_data *timer_data = data;

    if (!timer_data->running)
    {
		return 0;
    }

    // Read data until we must make a delay.
    for (;;)
    {
        // End of file?
        if (feof(timer_data->fstream))
        {
			timer_data->running = 0;
			return 0;
        }

		int reg = fgetc(timer_data->fstream);
		int val = fgetc(timer_data->fstream);

        // Register value of 0 or 1 indicates a delay.
        if (reg == 0x00)
        {
			return val;
        }
        else if (reg == 0x01)
        {
            val |= (fgetc(timer_data->fstream) << 8);
			return val;
        }
		else if(reg == 0x02)
		{
			timer_data->dro1Bank = 0;
		}
		else if(reg == 0x03)
			timer_data->dro1Bank = 1;
		else if(reg == 0x04)
		{
			reg = val;
			val = fgetc(timer_data->fstream);
			WriteReg(timer_data->dro1Bank, reg, val);
		}
		else
        {
			WriteReg(timer_data->dro1Bank, reg, val);
        }
    }

	return 0;
}

Uint32 Dro2TimerCallback(Uint32 interval, void* data)
{
	struct timer_data* timerData = data;

	while(1)
	{
		// End of file?
		if (feof(timerData->fstream))
		{
			timerData->running = 0;
			return 0;
		}

		int code = fgetc(timerData->fstream);
		int val = fgetc(timerData->fstream);

		if(code == timerData->dro2Header.shortDelayCode)
			return val + 1;
		else if(code == timerData->dro2Header.longDelayCode)
			return (val + 1) * 256;
		else
		{
			int reg = timerData->dro2Header.codemap[code & 0x7f];
			WriteReg(code >> 7, reg, val);
		}
	}
}

void PlayFile(char *filename)
{
    struct timer_data timer_data;
    char buf[8];

    timer_data.fstream = fopen(filename, "rb");

    if (timer_data.fstream == NULL)
    {
        fprintf(stderr, "Failed to open %s\n", filename);
		exit(EXIT_FAILURE);
    }

    if (fread(buf, 1, 8, timer_data.fstream) < 8)
    {
		fprintf(stderr, "Failed to read file header\n");
		exit(EXIT_FAILURE);
    }

    if (strncmp(buf, HEADER_STRING, 8) != 0)
    {
		fprintf(stderr, "Invalid file format\n");
		exit(EXIT_FAILURE);
    }

	uint16_t version[2];
	if (fread(version, 1, 4, timer_data.fstream) < 4)
	{
		fprintf(stderr, "Failed to read file version\n");
		exit(EXIT_FAILURE);
	}

	if(version[0] == 0 && version[1] == 1)
	{
		fseek(timer_data.fstream, 28, SEEK_SET);
		timer_data.running = 1;
		timer_data.dro1Bank = 0;

		// Start callback loop sequence.
		SDL_AddTimer(0, Dro1TimerCallback, &timer_data);
	}
	else if(version[0] == 2 && version[1] == 0)
	{
		if(fread(&timer_data.dro2Header, 14, 1, timer_data.fstream) != 1)
		{
			fprintf(stderr, "Could not read DROv2 header\n");
			exit(EXIT_FAILURE);
		}
		if(timer_data.dro2Header.codemapLength > 128)
		{
			fprintf(stderr, "Too many codemap entries (%i)\n", timer_data.dro2Header.codemapLength);
			exit(EXIT_FAILURE);
		}
		if(fread(&timer_data.dro2Header.codemap, 1, timer_data.dro2Header.codemapLength, timer_data.fstream) != timer_data.dro2Header.codemapLength)
		{
			fprintf(stderr, "Could not read codemap\n");
			exit(EXIT_FAILURE);
		}
		timer_data.running = 1;
		SDL_AddTimer(0, Dro2TimerCallback, &timer_data);
	}
	else
	{
		fprintf(stderr, "Unrecognized DRO file version \"%i.%i\"\n", version[0], version[1]);
		exit(EXIT_FAILURE);
	}

    // Sleep until the playback finishes.
	while(timer_data.running)
		SDL_Delay(100);

    fclose(timer_data.fstream);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
		return EXIT_FAILURE;
    }

    Init();
    PlayFile(argv[1]);
	Exit();

	return EXIT_SUCCESS;
}

