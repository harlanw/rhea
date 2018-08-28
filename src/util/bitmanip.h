#ifndef RHEA_BITMANIP_H
#define RHEA_BITMANIP_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOW(word) ((word) & 0xFF)
#define HIGH(word) (((word) & 0xFF00) >> 8)

#ifdef __cplusplus
};
#endif

#endif
