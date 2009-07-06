#ifndef _NMEA_H_
#define _NMEA_H_

enum State {
    StateWaitStart,
    StateProcessing,
};

struct sat_info {
    int prn;
    float elevation;
    float azimuth;
    int snr;
};

typedef void (sats_callback)(int num_sats, int sats[]);
typedef void (pos_callback)(int32_t lat, int32_t lon, uint32_t time, float altitude, float accuracy);
typedef void (gsv_callback)(int last, int index, int num, struct sat_info sats[]);
typedef void (vtg_callback)(float bearing, float speed);

typedef struct nmea_state {
    sats_callback *sats_cb;
    pos_callback *pos_cb;
    gsv_callback *gsv_cb;
    vtg_callback *vtg_cb;

    enum State state;
    char line[256];
    int count;
} nmea_state;

void nmea_parse(nmea_state *state, char buf[], char len);
void nmea_init(nmea_state *state, sats_callback *s, pos_callback *p,
               gsv_callback *gsv, vtg_callback *vtg);

#endif
