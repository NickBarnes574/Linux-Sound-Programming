#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "signal_handler.h"
#include "utilities.h"

#define FRAMES       128
#define SAMPLE_RATE  44100
#define NUM_CHANNELS 2
#define NUM_ARGS     3

static void print_help(char * program_name);
static int  initialize(char *       card_name,
                       char *       filename,
                       snd_pcm_t ** playback_handle);
static int  playback(snd_pcm_t *       playback_handle,
                     void *            buffer,
                     snd_pcm_uframes_t buffer_size,
                     FILE *            input_file);

int main(int argc, char ** argv)
{
    int         exit_code                     = E_FAILURE;
    short       buffer[FRAMES * NUM_CHANNELS] = { 0 };
    snd_pcm_t * playback_handle               = NULL;
    FILE *      input_file                    = NULL;
    char *      card_name                     = NULL;
    char *      filename                      = NULL;

    if (NULL == argv)
    {
        print_error("main(): NULL argument passed.");
        goto END;
    }

    if (NUM_ARGS != argc)
    {
        print_error("main(): Invalid number of arguments.");
        print_help(argv[0]);
        goto END;
    }

    // Initialize signal handler
    exit_code = signal_action_setup();
    if (E_SUCCESS != exit_code)
    {
        print_error("main(): Unable to setup signal handler.");
        goto END;
    }

    card_name = argv[1];
    filename  = argv[2];

    input_file = fopen(filename, "r");
    if (NULL == input_file)
    {
        print_error("main(): Unable to open file for reading.");
        goto END;
    }

    exit_code = initialize(card_name, filename, &playback_handle);
    if (E_SUCCESS != exit_code)
    {
        print_error("main(): Unable to initialize audio playback device.");
        goto END;
    }

    exit_code = playback(playback_handle, buffer, FRAMES, input_file);
    if (E_SUCCESS != exit_code)
    {
        print_error("main(): Fatal error occurred while playing audio.");
        goto END;
    }

END:
    if (NULL != input_file)
    {
        fclose(input_file);
        input_file = NULL;
    }
    if (NULL != playback_handle)
    {
        snd_pcm_drain(playback_handle);
        snd_pcm_close(playback_handle);
        playback_handle = NULL;
        snd_config_update_free_global();
    }

    return exit_code;
}

static int initialize(char *       card_name,
                      char *       filename,
                      snd_pcm_t ** playback_handle)
{
    int                   exit_code   = E_FAILURE;
    snd_pcm_hw_params_t * hw_params   = NULL;
    uint32_t              sample_rate = SAMPLE_RATE;

    if ((NULL == card_name) || (NULL == filename) || (NULL == playback_handle))
    {
        print_error("initialize(): NULL argument passed.");
        goto END;
    }

    exit_code =
        snd_pcm_open(playback_handle, card_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Cannot open audio device.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_malloc(&hw_params);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): CMR failure - hardware parameters.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_any(*playback_handle, hw_params);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to initialize hardware parameters.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_set_access(
        *playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to set access type.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_set_format(
        *playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to set sample format.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_set_rate_near(
        *playback_handle, hw_params, &sample_rate, 0);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to set sample rate.");
        goto END;
    }

    exit_code = snd_pcm_hw_params_set_channels(
        *playback_handle, hw_params, NUM_CHANNELS);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to set channel count.");
        goto END;
    }

    exit_code = snd_pcm_hw_params(*playback_handle, hw_params);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to set parameters.");
        goto END;
    }

    exit_code = snd_pcm_prepare(*playback_handle);
    if (E_SUCCESS != exit_code)
    {
        print_error("initialize(): Unable to prepare audio interface for use.");
        goto END;
    }

END:
    snd_pcm_hw_params_free(hw_params);
    return exit_code;
}

static int playback(snd_pcm_t *       playback_handle,
                    void *            buffer,
                    snd_pcm_uframes_t buffer_size,
                    FILE *            input_file)
{
    int               exit_code      = E_FAILURE;
    int               signal         = 0;
    snd_pcm_sframes_t frames_read    = 0;
    snd_pcm_sframes_t frames_written = 0;

    if ((NULL == playback_handle) || (NULL == buffer))
    {
        print_error("playback(): NULL argument passed.");
        goto END;
    }

    while (0 <
           (frames_read = fread(
                buffer, sizeof(short), buffer_size * NUM_CHANNELS, input_file)))
    {
        signal = check_for_signals();
        if (SHUTDOWN == signal)
        {
            exit_code = E_SUCCESS;
            goto END;
        }

        frames_written =
            snd_pcm_writei(playback_handle, buffer, frames_read / NUM_CHANNELS);
        if (0 > frames_written)
        {
            signal = check_for_signals();
            if (SHUTDOWN == signal)
            {
                exit_code = E_SUCCESS;
                goto END;
            }

            frames_written =
                snd_pcm_recover(playback_handle, frames_written, 0);
        }

        if (0 > frames_written)
        {
            signal = check_for_signals();
            if (SHUTDOWN == signal)
            {
                exit_code = E_SUCCESS;
                goto END;
            }

            print_error("playback(): Write to audio interface failed.");
            exit_code = snd_pcm_prepare(playback_handle);
            if (E_SUCCESS != exit_code)
            {
                print_error(
                    "playback(): Unable to prepare audio interface for use.");
                goto END;
            }
        }
    }

    exit_code = E_SUCCESS;
END:
    return exit_code;
}

static void print_help(char * program_name)
{
    if (NULL == program_name)
    {
        goto END;
    }

    fprintf(stderr, "Usage: %s cardname file\n", program_name);

END:
    return;
}
