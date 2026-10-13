#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

/*
Start the dashboard HTTP task on core 0 after the network interface is ready.
The task serves the embedded page and handles telemetry and manual requests;
it forwards commands to the control task instead of accessing actuators.
*/
esp_err_t web_server_start(void);

esp_err_t web_server_stop(void);

#endif

/*
 * How the web server works:
 * GET / serves the embedded dashboard, and GET /xml returns a copy of control
 * status. POST /controlMode and POST /controlPump validate and enqueue manual
 * requests. HTTP 202 reports queue acceptance; subsequent telemetry reports
 * the execution result. web_server_stop() releases the HTTP task and sockets.
 */
