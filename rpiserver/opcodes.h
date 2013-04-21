#ifndef COMM_OPCODE
#define COMM_OPCODE

#include <stdint.h>

#define SYNC_HEADER (0xAA)
// 0xAA <nrchars> int int
#define MAX_FRAME_SIZE (127)

typedef enum {
	STAGE_0	=	0,
	STAGE_1	=	1,
	STAGE_2	= 	2,
	STAGE_3 = 	3,
} comm_Stage;

typedef enum {
	MSG_PING = 0,
	MSG_PONG = 1,
} comm_Opcode;

typedef enum {
	PMSG_ATTITUDE = 2,
	MMSG_SET_SPEED_AND_DIRECTION = 3,
} comm_RosiaOpcode;

/* Opcodes sent by the computer software ( GCS Software ) and received by the GCS Hardware */
typedef enum {
	/* Opcodes destined to the GCS */
	CMSG_GCS_SYSTEM_STATUS_GET = 2,
} comm_COpcode;

/* Opcodes sent by the GCS Hardware and received by the robot */
typedef enum {
	/* Opcodes sent by the GCS itself */
	GMSG_GCS_SYSTEM_STATUS = 2,
} comm_GOpcode;

/* Opcodes sent by the robot */
typedef enum {
	RMSG_ROBOT_SYSTEM_STATUS = 2,
} comm_ROpcode;

/*
	Opcode: CMSG_GCS_SYSTEM_STATUS_GET OR CMSG_ROBOT_SYSTEM_STATUS_GET
	payload:
	payload size: 0
	reply to:
	expected reply: GMSG_GCS_SYSTEM_STATUS OR GMSG_ROBOT_SYSTEM_STATUS
*/

#if __GNUC__ && (GCC_MAJOR < 4 || GCC_MAJOR == 4 && GCC_MINOR < 1)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

typedef uint8_t ResetCause;
#define RESET_CAUSE_POR (uint8_t)(0)
#define RESET_CAUSE_BOR (uint8_t)(1)
#define RESET_CAUSE_MCLR (uint8_t)(2)
#define RESET_CAUSE_CMR (uint8_t)(3)
#define RESET_CAUSE_WDTR (uint8_t)(4)
#define RESET_CAUSE_SWR (uint8_t)(5)

#define TEMPERATURE_UNAVAILABLE 0xFFFF

typedef struct  {
	int32_t speed;
	int32_t directionAngle;
} tSetSpeedAndDirection;

typedef struct  {
	ResetCause resetCause;
	uint32_t uptime;
	uint32_t regulatedVoltage; /* in mV */
	uint32_t unregulatedVoltage; /* in mV */
	uint16_t temperature;
} tSystemStatus;

typedef uint8_t GpsStatus;
///< No GPS connected/detected
#define GPS_STATUS_NO_GPS (uint8_t)(0)
///< Receiving valid GPS messages but no lock
#define GPS_STATUS_NO_FIX (uint8_t)(1)
///< Receiving valid messages and locked
#define GPS_STATUS_OK (uint8_t)(2)

typedef struct {
	int32_t time;			///< GPS time (milliseconds from epoch)
	int32_t date;			///< GPS date (FORMAT TBD)
	int32_t latitude;		///< latitude in degrees * 10,000,000
	int32_t longitude;		///< longitude in degrees * 10,000,000
	int32_t altitude;		///< altitude in cm
	int32_t ground_speed;	///< ground speed in cm/sec
	int32_t ground_course;	///< ground course in 100ths of a degree
	int32_t speed_3d;		///< 3D speed in cm/sec (not always available)
	int32_t hdop;			///< horizontal dilution of precision in cm
	uint8_t num_sats;		///< Number of visible satelites
	GpsStatus status;
} tGps;

typedef struct  {
	int32_t latency;
	uint8_t LQI;
	uint8_t ED;
	uint32_t sentPackets;
	uint32_t ackedPackets;
	uint32_t receivedPackets;
} tRadioStatus;

/* speed in percentage -100% to 100% */
typedef struct {
	int8_t leftMotorsSpeed;
	int8_t rightMotorsSpeed;
} tSpeedSet;

typedef uint8_t NavMode;
#define NAVMODE_MANUAL (uint8_t)(0)
#define NAVMODE_AUTONOMOUS (uint8_t)(2)

typedef struct {
	NavMode mode;
} tNavMode;

typedef struct {
	int8_t angle;
} tCameraAngle;

typedef struct {
	int32_t q0; // float * 10^7
	int32_t q1; // float * 10^7
	int32_t q2; // float * 10^7
	int32_t q3; // float * 10^7
} tNavAttitude;

typedef struct {
	int32_t x; // in mm
	int32_t y; // in mm
	int32_t yaw; // in radians
} tNavPosition;

typedef struct {
	int32_t x;
	int32_t y;
} tWaypoint;

typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t gripper_angle;
} tArmPosition;

typedef struct {
	uint16_t base_servo;
	uint16_t shoulder_servo;
	uint16_t elbow_servo;
	uint16_t wrist_servo;
	uint16_t wrist_rotate_servo;
	uint16_t gripper_servo;
} tArmServoPos;

typedef struct {
	uint16_t left_sensor;
	uint16_t right_sensor;
} tDistanceSensors;

#if __GNUC__ && (GCC_MAJOR < 4 || GCC_MAJOR == 4 && GCC_MINOR < 1)
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif

