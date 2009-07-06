/*
 * nmea.c:  Simple NMEA parser designed for the Kaiser's
 *      Subset of NMEA 0183
 *
 * Copyright 2009 by Tyler Hall <tylerwhall@gmail.com>
 *
 * Licensed under GNU GPLv2
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include "nmea.h"

#define ADVANCE(x) (x)+strlen((x))+1

static void handle_vtg (nmea_state *state, char *args)
{
    float speed = -1;
    float bearing = -1;

    /* True */
    sscanf(args, "%f", &bearing);
    args = ADVANCE(args);
    args = ADVANCE(args);
    /* Magnetic */
    args = ADVANCE(args);
    args = ADVANCE(args);
    /* Knots */
    args = ADVANCE(args);
    args = ADVANCE(args);
    /* kph */
    sscanf(args, "%f", &speed);
    /*
    if (speed > 0)
        speed = speed / 3.6; //Convert to m/s
    */
    if (state->vtg_cb)
        state->vtg_cb(bearing, speed);
}

static void handle_gsv (nmea_state *state, char *args)
{
    int msgs;
    int msgn;
    int num_sats;
    struct sat_info sats[4];
    int i;

    sscanf(args, "%d", &msgs);
    args = ADVANCE(args);
    sscanf(args, "%d", &msgn);
    args = ADVANCE(args);
    sscanf(args, "%d", &num_sats);
    args = ADVANCE(args);

    for (i= 0; i < 4; i++)
    {
        if (!args[0])
            break;
        sscanf(args, "%d", &(sats[i].prn));
        args = ADVANCE(args);
        if (!args[0])
            break;
        sscanf(args, "%f", &sats[i].elevation);
        args = ADVANCE(args);
        if (!args[0])
            break;
        sscanf(args, "%f", &sats[i].azimuth);
        args = ADVANCE(args);
        if (!args[0])
            break;
        sscanf(args, "%d", &sats[i].snr);
        sats[i].snr *= 256; //Fixup for libgps
        args = ADVANCE(args);
    }
    if (state->gsv_cb)
        state->gsv_cb(msgs == msgn, 4 * (msgn - 1), i, sats);
}

static void handle_gsa (nmea_state *state, char *args)
{
    char *mode1 = args;
    char *mode2 = ADVANCE(mode1);
    int sats[12];
    int num_sats = 0;
    int i;

    args = ADVANCE(mode2);

    for (i=0; i<12; i++) {
        int sat_no = 0;

        sscanf(args, "%d", &sat_no);
        if (sat_no) {
            sats[num_sats] = sat_no;
            num_sats++;
        }
        args = ADVANCE(args);
    }

    if (state->sats_cb)
        state->sats_cb(num_sats, sats);
}

static void handle_gga (nmea_state *state, char *args)
{
    uint32_t time;
    uint32_t mtime;
    int deg, min, dmin;
    char c;
    char tmp[16];
    int32_t lat = 0, lon = 0;
    float altitude;
    float accuracy;
    int valid = 0;

    /* Time */
    sscanf(args, "%d", &time);
    mtime = time / 10000 * 3600; //hours
    mtime += time % 10000 / 100 * 60; //minutes
    mtime += time % 100; //seconds
    mtime *= 1000;
    args = ADVANCE(args);

    /* Multiple sscanf because the kernel does not support
     * field width (correctly)?? */
    /* Lat */
    strncpy(tmp, args, sizeof(tmp));
    if (strlen(tmp)) {
        char *ctmin = tmp;
        strsep(&ctmin, ".");
        sscanf(tmp, "%d", &deg); //Degrees and minutes
        min = deg % 100;
        deg = deg / 100;

        sscanf(ctmin, "%d", &dmin);
        lat = deg * (60 * 60 * 50) +
              min * (60 * 50) +
              dmin * 6 * 5 / 10000; //50ths of an arcsecond
    }
    args = ADVANCE(args);
    if (c == 'S')
        lat = -lat;
    args = ADVANCE(args);

    /* Lon */
    strncpy(tmp, args, sizeof(tmp));
    if (strlen(tmp)) {
        char *ctmin = tmp;
        strsep(&ctmin, ".");
        sscanf(tmp, "%d", &deg); //Degrees and minutes
        min = deg % 100;
        deg = deg / 100;

        sscanf(ctmin, "%d", &dmin);
        lon = deg * (60 * 60 * 50) +
              min * (60 * 50) +
              dmin * 6 * 5 / 10000; //50ths of an arcsecond
    }
    args = ADVANCE(args);
    sscanf(args, "%c", &c);
    if (c == 'W')
        lon = -lon;
    args = ADVANCE(args);

    /* Quality */
    if (args[0] != '0')
        valid = 1;
    args = ADVANCE(args);

    /* Num sats in use */
    args = ADVANCE(args);
    /* HDOP */
    sscanf(args, "%f", &accuracy);
    //XXX accuracy *= 6; //Assuming 6 meter ideal accuracy
    args = ADVANCE(args);
    /* Altitude */
    sscanf(args, "%f", &altitude);

    if (valid && state->pos_cb)
        state->pos_cb(lat, lon, mtime, altitude, accuracy);
}

static void process_sentence (nmea_state *state)
{
    char *sentence = state->line;
    char *tmp = sentence;
    char *args;

    /* Break string into comma seperated components */
    while (tmp) {
        strsep(&tmp, ",");
    }

    args = ADVANCE(sentence);

    if (!strcmp("GPGGA", sentence)) {
        handle_gga(state, args);
    } else if (!strcmp("GPGSA", sentence)) {
        handle_gsa(state, args);
    } else if (!strcmp("GPGSV", sentence)) {
        handle_gsv(state, args);
    } else if (!strcmp("GPVTG", sentence)) {
        handle_vtg(state, args);
    } else {
        //printf("Unknown command: %s\n", sentence);
    }
}

void nmea_parse(nmea_state *state, char buf[], char len)
{
    int i;

    for (i = 0; i < len; i++) {
        char ch = buf[i];
        switch (state->state) {
            case StateWaitStart:
                if (ch == '$') {
                    state->state = StateProcessing;
                }
                break;

            case StateProcessing:
                state->line[state->count++] = ch;
                if (ch == '*') {
                    state->line[state->count-1] = '\0';
                    process_sentence(state);
                    state->state = StateWaitStart;
                    state->count = 0;
                }
                break;
        }
    }
}


void nmea_init(nmea_state *state, sats_callback *s, pos_callback *p,
               gsv_callback *gsv, vtg_callback *vtg)
{
    state->sats_cb = s;
    state->pos_cb = p;
    state->gsv_cb = gsv;
    state->vtg_cb = vtg;
    state->state = StateWaitStart;
    state->count = 0;

}
