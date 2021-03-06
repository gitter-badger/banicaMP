#include "MPEG_decoder.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define BUFF_SIZE 8388608 // 8MB

/*
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
*/

static int scale(mad_fixed_t sample)
{
    /* round */
    sample += (1L << (MAD_F_FRACBITS - 16));

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    /* quantize */
    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

MPEG_decoder::MPEG_decoder(FILE *lol)
{
    file = lol;
}

MPEG_decoder::~MPEG_decoder(void)
{
    //fclose(file);
}

static enum mad_flow input(void *data,
        struct mad_stream *stream)
{
    uberbuff *buf = static_cast<uberbuff *>(data);
    int what;

    if ((what = fread(buf->buf->start, 1, buf->buf->length, buf->rd)) > 0) {
        mad_stream_buffer(stream, buf->buf->start, buf->buf->length);
        return MAD_FLOW_CONTINUE;
    } else {
        //fprintf(stderr, "Error %d\n", what);
        return MAD_FLOW_STOP;
    }
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow error(void *data, 
        struct mad_stream *stream,
        struct mad_frame *frame)
{
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mem_output(void *data, 
        const struct mad_header *header,
        struct mad_pcm *pcm)
{
    signed int sample;
    unsigned nchannels, nsamples, ss;
    const mad_fixed_t *left_ch, *right_ch;
    uberbuff *buf = static_cast<uberbuff *>(data);

    nchannels = pcm->channels;
    nsamples = pcm->length;
    ss = nsamples * (nchannels << 1);
    left_ch = pcm->samples[0];
    right_ch = pcm->samples[1];

    //if (buf->mem->cap() < (((size_t) buf->p - (size_t) buf->mem->begin()) + ss))
        //buf->mem->expand(buf->mem->cap());

    //signed char *to_write = (signed char *) malloc(nsamples * (nchannels << 1));
    //signed char *tmp = to_write;
    signed char *tmp = (signed char *) buf->p;
    while (nsamples--) {
        /* output sample(s) in 16-bit signed little-endian PCM */
        sample = scale(*left_ch++);
        tmp[0] = (sample >> 0) & 0xff;
        tmp[1] = (sample >> 8) & 0xff;
        sample = scale(*right_ch++);
        // two-channel?
        tmp[2] = (sample >> 0) & 0xff;
        tmp[3] = (sample >> 8) & 0xff;

        //memcpy(tmp, ostr, sizeof(ostr));
        //tmp += sizeof(ostr);
        tmp += 4;
    }

    //memcpy(buf->p, to_write, ss);
    buf->p = (void *) ((size_t) buf->p + ss);

    return MAD_FLOW_CONTINUE;
}

static enum mad_flow file_output(void *data, 
        const struct mad_header *header,
        struct mad_pcm *pcm)
{
    signed int sample;
    unsigned nchannels, nsamples, ss;
    const mad_fixed_t *left_ch, *right_ch;
    uberbuff *buf = static_cast<uberbuff *>(data);

    nchannels = pcm->channels;
    nsamples = pcm->length;
    ss = nsamples;
    left_ch = pcm->samples[0];
    right_ch = pcm->samples[1];

    signed char *to_write = (signed char *) malloc(nsamples * (nchannels << 1));
    signed char *tmp = to_write;
    while (nsamples--) {

        /* output sample(s) in 16-bit signed little-endian PCM */

        sample = scale(*left_ch++);
        tmp[0] = (sample >> 0) & 0xff;
        tmp[1] = (sample >> 8) & 0xff;
        sample = scale(*right_ch++);
        // We will likely use a two-channel audio >_>
        tmp[2] = (sample >> 0) & 0xff;
        tmp[3] = (sample >> 8) & 0xff;

        //if (nchannels == 2) {
            //sample = scale(*right_ch++);
            //ostr[2] = (sample >> 0) & 0xff;
            //ostr[3] = (sample >> 8) & 0xff;
            //putc((sample >> 0) & 0xff, buf->f);
            //putc((sample >> 8) & 0xff, buf->f);
        //}
        //fwrite((const void *) ostr, 1, 4, buf->f);
        //memcpy(tmp, ostr, sizeof(ostr));
        tmp += 4;
    }

    fwrite(to_write, sizeof(*to_write), ss * (nchannels << 1), buf->f);
    free(to_write);

    return MAD_FLOW_CONTINUE;
}

bool MPEG_decoder::decode(FILE *lol)
{
    buffer buf;
    struct mad_decoder dec;
    uberbuff pro_buff(&buf, lol, file, NULL);
    int result;

    //buf.start = new unsigned char [BUFF_SIZE];
    buf.start = (unsigned char *) malloc(BUFF_SIZE * sizeof(*buf.start)); 
    buf.length = BUFF_SIZE;

    mad_decoder_init(&dec, &pro_buff, input, 0, 0, 
            file_output, error, 0);

    result = mad_decoder_run(&dec, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&dec);
    free(buf.start);

    // FIXME
    return static_cast<bool>(!!result);
}

bool MPEG_decoder::decode(memory *m)
{
    buffer buf;
    struct mad_decoder dec;
    uberbuff pro_buff(&buf, NULL, file, m);
    int result;

    //buf.start = new unsigned char [BUFF_SIZE];
    buf.start = (unsigned char *) malloc(BUFF_SIZE * sizeof(*buf.start)); 
    buf.length = BUFF_SIZE;

    mad_decoder_init(&dec, &pro_buff, input, 0, 0, 
            mem_output, error, 0);

    result = mad_decoder_run(&dec, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&dec);
    free(buf.start);

    return static_cast<bool>(!!result);
}

