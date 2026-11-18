/*
 * config_site.h do pjproject para o MicroSIP | Excel (uso interno da Excel Suprimentos).
 * Build Win32 estatico, sem video, TLS pelo Schannel do Windows (sem OpenSSL).
 */
#pragma once

/* Desktop Windows: perfil padrao do pjproject para aplicativos de PC */
#define PJ_CONFIG_MINIMAL_SIZE              0
#define PJ_CONFIG_MAXIMUM_SPEED             1

/* Sem video: dispensa ffmpeg, libvpx, SDL e libyuv */
#define PJMEDIA_HAS_VIDEO                   0
#define PJMEDIA_HAS_LIBYUV                  0
#define PJMEDIA_HAS_FFMPEG                  0
#define PJMEDIA_HAS_VPX_CODEC               0
#define PJMEDIA_VIDEO_DEV_HAS_SDL           0
#define PJMEDIA_VIDEO_DEV_HAS_DSHOW         0

/* TLS pelo Schannel (nativo do Windows); DTLS-SRTP exige OpenSSL, fica desligado */
#define PJ_HAS_SSL_SOCK                     1
#define PJ_SSL_SOCK_IMP                     PJ_SSL_SOCK_IMP_SCHANNEL
#define PJMEDIA_HAS_SRTP                    1
#define PJMEDIA_SRTP_HAS_DTLS               0

/* Codecs: G.711, G.722, GSM, iLBC, Speex, G.722.1 (embutidos); sem Opus e G.729 */
#define PJMEDIA_HAS_OPUS_CODEC              0
#define PJMEDIA_HAS_BCG729                  0
#define PJMEDIA_HAS_G7221_CODEC             1
#define PJMEDIA_HAS_L16_CODEC               0

/* Cancelamento de eco WebRTC (o MicroSIP oficial usa) */
#define PJMEDIA_HAS_WEBRTC_AEC              1

/* Limites confortaveis para um softphone de mesa */
#define PJSUA_MAX_CALLS                     32
#define PJSUA_MAX_ACC                       32
#define PJSUA_MAX_PLAYERS                   32
#define PJSUA_MAX_RECORDERS                 32
#define PJSUA_MAX_CONF_PORTS                (PJSUA_MAX_CALLS + 2 * PJSUA_MAX_PLAYERS)
#define PJSUA_MAX_BUDDIES                   1024
#define PJ_IOQUEUE_MAX_HANDLES              1024

#define PJ_TODO(x)

/* O MicroSIP usa o nome antigo (o pjproject 2.17 renomeou para PJMEDIA_AUD_DEV_MAX_DEVS) */
#define PJMEDIA_AUD_MAX_DEVS                PJMEDIA_AUD_DEV_MAX_DEVS
