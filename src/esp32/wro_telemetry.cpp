#include "wro_build_target.h"
#if WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN

#include <Arduino.h>
#include "wro_config_v13.h"
#include "wro_imu.h"
#include "wro_odometry.h"
#include "wro_camera.h"
#include "wro_telemetry.h"
#include "wro_telemetry_wifi.h"
#include "wro_sensors.h"

// Live-tunable gains (extern declared in wro_telemetry.h; the behavior
// modules re-read them every update tick, so the serial commands below
// take effect immediately without a recompile).
float  g_pid_kp_obs   = PILLAR_KP;
float  g_pid_ki_obs   = PILLAR_KI;
float  g_pid_kd_obs   = PILLAR_KD;
float  g_gyro_kp      = HEADING_KP;
int    g_max_pwm_obs  = OBS_MAX_PWM;
int    g_max_pwm_open = OPEN_MAX_PWM;
float  g_steer_gain_left  = STEER_GAIN_LEFT_DEFAULT;
float  g_steer_gain_right = STEER_GAIN_RIGHT_DEFAULT;

static bool softwareEstopFlag = false;

static unsigned long lastDumpMs = 0;

#define CMD_BUF_SIZE 32
static char    cmdBuf[CMD_BUF_SIZE];
static uint8_t cmdPos = 0;

static const char* raceStateName(int s);
static const char* cornerStateName(int s);

void tlm_init() {
  cmdPos = 0;
  lastDumpMs = millis();
}

bool tlm_consume_software_estop() {
  bool v = softwareEstopFlag;
  softwareEstopFlag = false;
  return v;
}

static void handleCommand(const char *line) {
  if (line[0] == 0) return;
  char k = line[0];
  switch (k) {
    case 'P': g_pid_kp_obs = atof(line + 1); Serial.printf("KP=%.4f\n", g_pid_kp_obs); break;
    case 'I': g_pid_ki_obs = atof(line + 1); Serial.printf("KI=%.4f\n", g_pid_ki_obs); break;
    case 'D': g_pid_kd_obs = atof(line + 1); Serial.printf("KD=%.4f\n", g_pid_kd_obs); break;
    case 'G': g_gyro_kp    = atof(line + 1); Serial.printf("GKP=%.4f\n", g_gyro_kp);  break;
    // Steering asymmetry trim. Clamped to [0.5, 1.5]: a fat-fingered "L12"
    // would otherwise pin every left command against the end-stop.
    case 'L':
      g_steer_gain_left  = constrain((float)atof(line + 1), 0.5f, 1.5f);
      Serial.printf("STEER_GAIN L=%.3f\n", g_steer_gain_left);
      break;
    case 'R':
      g_steer_gain_right = constrain((float)atof(line + 1), 0.5f, 1.5f);
      Serial.printf("STEER_GAIN R=%.3f\n", g_steer_gain_right);
      break;
    case 'S':
      if (line[1] == '+') { g_max_pwm_obs += 5; g_max_pwm_open += 5; }
      if (line[1] == '-') { g_max_pwm_obs -= 5; g_max_pwm_open -= 5; }
      // Keep within [deadband, 8-bit PWM]: repeated 'S-' must not go
      // negative (negative = reverse on the straights!).
      if (g_max_pwm_obs  < MIN_DRIVE_PWM) g_max_pwm_obs  = MIN_DRIVE_PWM;
      if (g_max_pwm_open < MIN_DRIVE_PWM) g_max_pwm_open = MIN_DRIVE_PWM;
      if (g_max_pwm_obs  > 255) g_max_pwm_obs  = 255;
      if (g_max_pwm_open > 255) g_max_pwm_open = 255;
      Serial.printf("PWM open=%d obs=%d\n", g_max_pwm_open, g_max_pwm_obs);
      break;
    case '?':
      Serial.printf("yaw=%.1f total=%.1f distL=%ld distR=%ld v=%.1fcm/s tf=%dmm cam_ok=%d\n",
                    g_yaw, g_yaw_total, g_dist_left_ticks, g_dist_right_ticks,
                    g_speed_cm_s, sens_tf_front_mm(), (int)g_cam.online);
      Serial.printf("KP=%.4f KI=%.4f KD=%.4f GKP=%.4f SGL=%.3f SGR=%.3f\n",
                    g_pid_kp_obs, g_pid_ki_obs, g_pid_kd_obs, g_gyro_kp,
                    g_steer_gain_left, g_steer_gain_right);
      break;
    case '!':
      softwareEstopFlag = true;
      Serial.println("SOFTWARE ESTOP REQUESTED");
      break;
    default:
      Serial.printf("Unknown command: %c\n", k);
      break;
  }
}

static void readCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      cmdBuf[cmdPos] = 0;
      handleCommand(cmdBuf);
      cmdPos = 0;
      continue;
    }
    if (cmdPos < CMD_BUF_SIZE - 1) cmdBuf[cmdPos++] = c;
    else                            cmdPos = 0;
  }
}

void tlm_update_periodic(int race_state, int corner_state, int lap,
                         int signed_pwm, int steer_us) {
  readCommands();

  unsigned long now = millis();
  if (now - lastDumpMs < TELEMETRY_INTERVAL_MS) return;
  lastDumpMs = now;

  int rd = (g_cam.redDist   < 999) ? g_cam.redDist   : -1;
  int gd = (g_cam.greenDist < 999) ? g_cam.greenDist : -1;

  // Build once so USB serial and the Wi-Fi mirror emit the identical line.
  static char tlmLine[160];
  snprintf(tlmLine, sizeof(tlmLine),
    "T=%lu ST=%s CN=%s LAP=%d YAW=%.1f DST=L%+ld/R%+ld TF=%dmm CAM=R(%d,%d)/G(%d,%d) PWM=%+d ST=%dus\n",
    now,
    raceStateName(race_state),
    cornerStateName(corner_state),
    lap,
    g_yaw,
    g_dist_left_ticks, g_dist_right_ticks,
    sens_tf_front_mm(),
    g_cam.redX, rd,
    g_cam.greenX, gd,
    signed_pwm, steer_us
  );
  Serial.print(tlmLine);
  tlm_wifi_send(tlmLine);
}

static const char* raceStateName(int s) {
  switch (s) {
    case 0: return "INIT";
    case 1: return "WAIT";
    case 2: return "RUN_O";
    case 3: return "RUN_X";
    case 4: return "TURN";
    case 5: return "PARK";
    case 6: return "FIN";
    case 7: return "STOP";
    default: return "?";
  }
}
static const char* cornerStateName(int s) {
  switch (s) {
    case 0: return "ARM";
    case 1: return "SLOW";
    case 2: return "COMM";
    case 3: return "BRK";
    case 4: return "EXEC";
    case 5: return "EXIT";
    case 6: return "LOCK";
    case 7: return "FAIL";
    default: return "?";
  }
}

#endif  // WRO_ACTIVE_TARGET == WRO_TARGET_V13_MAIN
