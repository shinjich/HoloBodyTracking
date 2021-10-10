// Original Author: Shinji Chiba (MSKK)
#pragma once

#include "udp.h"
#include <k4a/k4a.h>
#include <k4abt.h>

#define RBT_LONGTHROW_DEPTH		1

// 共有名
#define DESTINATION_IP_ADDRESS	"111.111.111.111"
#define SENDBACK_IP_ADDRESS		"222.222.222.222"
#define DESTINATION_PORT		16812
#define SENDBACK_PORT			16813

// イメージの解像度
#if RBT_LONGTHROW_DEPTH
#define K4A_DEPTH_MODE			(K4A_DEPTH_MODE_NFOV_2X2BINNED)
#define RESOLUTION_WIDTH		(320)
#define RESOLUTION_HEIGHT		(288)
#define PACKET_DIVIDE			(16)
#define MIN_OFFSET_DISTANCE		(0)
#define SCOPE_DISTANCE_BITS_D	(13)
#define SCOPE_DISTANCE_BITS_I	(14)
#else
#define K4A_DEPTH_MODE			(K4A_DEPTH_MODE_WFOV_2X2BINNED)
#define RESOLUTION_WIDTH		(512)
#define RESOLUTION_HEIGHT		(512)
#define PACKET_DIVIDE			(32)
#define MIN_OFFSET_DISTANCE		(32)
#define SCOPE_DISTANCE_BITS_D	(10)
#define SCOPE_DISTANCE_BITS_I	(14)
#endif
#define RESOLUTION_SIZE			(RESOLUTION_WIDTH * RESOLUTION_HEIGHT)

#define MAX_BODIES			1

typedef struct {
	DWORD dwHeader;
	BYTE cDepth[RESOLUTION_SIZE * sizeof(UINT16) / PACKET_DIVIDE];
	BYTE cIr[RESOLUTION_SIZE * sizeof(UINT16) / PACKET_DIVIDE];
} RBT_STRUCT;

typedef struct {
	DWORD dwHeader;
	BYTE cDepth[(RESOLUTION_SIZE * SCOPE_DISTANCE_BITS_D / 8 / PACKET_DIVIDE + 3) & (size_t) -4];
	BYTE cIr[(RESOLUTION_SIZE * SCOPE_DISTANCE_BITS_I / 8 / PACKET_DIVIDE + 3) & (size_t) -4];
} RBT_STRUCT_SAVED;

typedef struct {
	DWORD dwHeader;
	k4abt_skeleton_t skeleton[MAX_BODIES];
	k4a_float2_t skeleton2D[MAX_BODIES][K4ABT_JOINT_COUNT];
	uint32_t bodyID[MAX_BODIES];
} SBT_STRUCT;
