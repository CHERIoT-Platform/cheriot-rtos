// Modem.c

#include "modem.hh"
#include <debug.hh>
#include <platform/sunburst/platform-uart.hh>
#include <stdint.h>
#include <stdio.h> /* Standard input/output definitions */
#include <stdlib.h>
#include <string.h> /* String function definitions */
#include <ds/ring_buffer.h>	// So we can get a ring buffer
#include <thread_pool.h>	// We'll use this to read the receive buffer.
#include "uart.hh"

#define MAX_EVENTS (32)

#define BUFFER_SIZE (255)


/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Modem.cc">;

#define TASK_OFF (-1) // Unit is just switched on. We will need to configure it.
#define TASK_NONE (0) // No tasks waiting so pause and wait for something to happen
#define TASK_GET_IMSI (1) // Get the IMSI
#define TASK_GET_IMEI (2) // Get the IMEI
#define TASK_GET_CCID (3) // Get the CCID
#define TASK_SET_APN (4)  // Set the APN
#define TASK_PING (5)     // Ping an address
#define TASK_URL (6)      // Opens a URL
#define TASK_POST (7)     // Posts to the opened URL
#define TASK_CONTEXT_ID (8)	// Set the HTTP Context ID
#define TASK_QUERY_CONTEXT (9)	// Query the HTTP Context state
#define TASK_ACTIVATE_CONTEXT (10)	// Activate the HTTP Context

volatile OpenTitanUart *uartInterface = NULL;

struct TaskData
{
	char  *cmd;
	size_t len;
};

char *tasksCmds[] = {
  "",
  "at+cimi",
  "at+gsn",
  "at+qccid",
  "at+cgdcont",
  "at+qping",
  "at+qhttpurl",
  "at+qhttppost",
  "at+qhttpcfg=\"contextid\",1",
  "at+qiact?",
  "at+qiact=1"
};

uint8_t *tasks       = NULL;
uint16_t currentTask = 0;

uint8_t task_get_next()
{
	uint8_t ret = TASK_NONE;

	if (tasks != NULL)
	{
		ret = tasks[currentTask];
		currentTask++;
		if (ret == TASK_NONE)
		{
			free(tasks); // task chain is finished so remove
			currentTask = 0;
		}
	}

	return ret;
}

__cheri_callback void thread_rx(CHERI_SEALED(void *) data) {
	// This is our new thread.
	while(1) {
		if(uartInterface->receive_fifo_level() > 0) {
			uint8_t c = uartInterface->blocking_read();
			Debug::log("Rx: {}", c);
		}
	}
}

uint16_t temp = 0x01;
CHERI_SEALED(void *) temp2 = &temp;
void modem_init()
{
	uartInterface = MMIO_CAPABILITY(OpenTitanUart, uart1);
	uartInterface->init(115200);
	uartInterface->fifos_clear();
	// Start the rx thread.
	int ret = thread_pool_async(thread_rx, &temp2);
	Debug::log("thread_rx() returned {}", ret);
}

char post[] = "\r\n\r\n";
// This is a very old IP that we don't have anymore. Feel free to chaneg it to something useful to you.
// #define FORMAT_URL "http://35.178.111.83:3100/trk/%s/?%s"
#define FORMAT_URL "http://18.175.136.129:3100/trk/%s/?%s"
char *url       = NULL;
char *modemImsi = NULL;
char *modemImei = NULL;
char *modemCcid = NULL;
char *contextData = NULL;

void tasks_process() {
	char *tx;
	if (tasks == NULL) {
		return;
	}

	switch (tasks[currentTask]) {
		default:
		case TASK_NONE:
			currentTask = 0;
			free(tasks);
			tasks = NULL;
			break;
		case TASK_GET_IMSI:	// Get the IMSI
		case TASK_GET_IMEI:	// Get the IMEI
		case TASK_GET_CCID:	// Get the CCID
		case TASK_CONTEXT_ID:	// Set the HTTP Context ID
		case TASK_QUERY_CONTEXT:	// Query the HTTP Context state
			append_to_tx_buffer(tasksCmds[tasks[currentTask]],
				strlen(tasksCmds[tasks[currentTask]]));
			append_to_tx_buffer("\r\n", 2);
			break;
		case TASK_ACTIVATE_CONTEXT:	// Activate the HTTP Context
			if(contextData == NULL) {
				append_to_tx_buffer(tasksCmds[tasks[currentTask]],
					strlen(tasksCmds[tasks[currentTask]]));
				append_to_tx_buffer("\r\n", 2);
			} else {
				Debug::log("Context already active.");
				currentTask++;
				tasks_process();
			}
			break;
		case TASK_SET_APN: // Set the APN
			tx = static_cast<char *>(calloc(1, strlen(tasksCmds[tasks[currentTask]]) + 21 + 1));
			sprintf(tx,
			        "%s=1,\"IP\",\"gigsky-02\"\r\n",
			        tasksCmds[tasks[currentTask]]);
			append_to_tx_buffer(tx, strlen(tx));
			
			free(tx);
			break;
		case TASK_PING: // Ping an address
			tx = static_cast<char *>(
			  calloc(1, strlen(tasksCmds[tasks[currentTask]]) + 21 + 1));
			sprintf(
			  tx, "%s=1,\"www.google.com\"\r\n", tasksCmds[tasks[currentTask]]);
			append_to_tx_buffer(tx, strlen(tx));
			free(tx);
			break;
		case TASK_URL: // Open our post to an address
			tx = static_cast<char *>(
			  calloc(1, strlen(tasksCmds[tasks[currentTask]]) + 100 + 1));
			sprintf(
			  tx, "%s=%lu,80\r\n", tasksCmds[tasks[currentTask]], strlen(url));
			printf("Modem: OUT (serial): %s", tx);
			append_to_tx_buffer(tx, strlen(tx));
			free(tx);
			break;
		case TASK_POST: // Send data to an open connection
			tx = static_cast<char *>(
			  calloc(1, strlen(tasksCmds[tasks[currentTask]]) + 100 + 1));
			sprintf(tx,
			        "%s=%lu,80,80\r\n",
			        tasksCmds[tasks[currentTask]],
			        strlen(post));
			printf("Modem: OUT (serial): %s", tx);
			append_to_tx_buffer(tx, strlen(tx));
			free(tx);
			break;
	}
}

// void print_in_hex(char *buffer)
// {
// 	for (int i = 0; i < strlen(buffer); i++)
// 	{
// 		printf("0x%02x ", buffer[i]);
// 	}
// 	printf("\n");
// }

bool process_serial_replies(char *buffer, size_t len)
{
	bool ret = false;
	// printf("Serial In (%lu): %.*s", len, len, buffer);
	// printf("  Command (%lu): %.*s\r\n",
	// strlen(tasksCmds[tasks[currentTask]]),
	// strlen(tasksCmds[tasks[currentTask]]),
	// tasksCmds[tasks[currentTask]]); printf("Serial In (%lu): ", len);
	// printInHex(buffer);
	// printf("  Command (%lu): ", strlen(tasksCmds[tasks[currentTask]]));
	// printInHex(tasksCmds[tasks[currentTask]]);

	// if((tasks != NULL) && (tasks[currentTask] == TASK_URL)) {
	// 	printf("process_serial_replies(): %.*s", len, buffer);
	// }
	if (tasks == NULL)
	{
		// printf("Modem ");
		// print_time_now();
		// printf(" IN (serial A): buffer = %s\n", buffer);
		// printf("%s %u: There are no tasks active!\n", __FILE__, __LINE__);
		// printf("%s %u: buffer = %s\n", __FILE__, __LINE__, buffer);
	}
	else if (0 == strncmp("OK\r\n", buffer, len))
	{
		printf("Modem ");
		printf(" AT Command: Message successfully processed!\n");
		currentTask++;
		tasks_process();
	}
	else if (0 == strncmp("ERROR", buffer, 5))
	{
		printf("Modem ");
		printf(" ERROR! Retry the command.\n");
		tasks_process();
	}
	else if (0 == strncmp("\r\n", buffer, 2))
	{
		// printf("Ignore blank line!\n");
	}
	else if (0 == strncmp(tasksCmds[tasks[currentTask]],
	                      buffer,
	                      strlen(tasksCmds[tasks[currentTask]])))
	{
		// printf("Repeating the command back to us.\n");
	}
	else
	{
		// We may wish to save this information somewhere.
		int   eol  = len;
		char *eol1 = strchr(buffer, '\r');
		if ((eol1 != NULL) && ((eol1 - buffer) < eol))
		{
			eol = eol1 - buffer;
		}
		char *eol2 = strchr(buffer, '\n');
		if ((eol2 != NULL) && ((eol2 - buffer) < eol))
		{
			eol = eol2 - buffer;
		}
		char *eol3 = strchr(buffer, '\0');
		if ((eol3 != NULL) && ((eol3 - buffer) < eol))
		{
			eol = eol3 - buffer;
		}
		// printf("%s", buffer);
		// for(int i = 0; i < eol; i++)
		//   printf(" ");
		// printf("^\r\n");
		// printf("eol = %i\n", eol);
		// printf("%.*s***\n", eol, buffer);
		switch (tasks[currentTask])
		{
			case TASK_GET_IMSI:
				if(contextData != NULL) {
					free(modemImsi);
				}
				modemImsi = static_cast<char *>(calloc(1, eol + 1));
				strncpy(modemImsi, buffer, eol);
				printf("Modem ");
				printf(" IMSI: %s\n", modemImsi);
				break;
			case TASK_GET_IMEI:
				if(contextData != NULL) {
					free(modemImei);
				}
				modemImei = static_cast<char *>(calloc(1, eol + 1));
				strncpy(modemImei, buffer, eol);
				printf("Modem ");
				printf(" IMEI: %s\n", modemImei);
				break;
			case TASK_GET_CCID:
				if(contextData != NULL) {
					free(modemCcid);
				}
				modemCcid = static_cast<char *>(calloc(1, eol + 1));
				strncpy(modemCcid, buffer, eol);
				printf("Modem ");
				printf(" CCID: %s\n", modemCcid);
				break;
			case TASK_QUERY_CONTEXT:
				if(contextData != NULL) {
					free(contextData);
				}
				contextData = static_cast<char *>(calloc(1, eol + 1));
				strncpy(contextData, buffer, eol);
				printf("context: %s\n", contextData);
				break;
			case TASK_URL:
				if (0 == strncmp("CONNECT", buffer, 7))
				{
					printf("Modem ");
					printf(" OUT (serial): %s\n", url);
					append_to_tx_buffer(url, strlen(url));
					ret = true;
				}
				break;
			case TASK_POST:
				if (0 == strncmp("CONNECT", buffer, 7))
				{
					printf("Modem ");
					printf(" OUT (serial): Sending: body\n");
					append_to_tx_buffer(post, strlen(post));
					ret = true;
				}
				break;
		}
	}
	return ret;
}

void tasks_set_initialise_modem()
{
	if (tasks != NULL)
	{
		// Stop whatever we are doing an reinitialise everything!
		free(tasks);
		currentTask = 0;
	}

	tasks    = static_cast<uint8_t *>(calloc(1, 9));
	tasks[0] = TASK_GET_IMSI;
	tasks[1] = TASK_GET_IMEI;
	tasks[2] = TASK_GET_CCID;
	tasks[3] = TASK_SET_APN;
	tasks[4] = TASK_CONTEXT_ID;
	tasks[5] = TASK_QUERY_CONTEXT;
	tasks[6] = TASK_ACTIVATE_CONTEXT;
	tasks[7] = TASK_QUERY_CONTEXT;
	// tasks[8] = TASK_PING;
	// tasks[9] = TASK_NONE;
	tasks[8] = TASK_NONE;
}

void tasks_send_message(char *msg)
{
	if (tasks != NULL)
	{
		printf("%s %u: Still executing a task so skip.\n", __FILE__, __LINE__);
		return;
	}

	if (url != NULL)
	{
		free(url);
		url = NULL;
	}

	if (msg == NULL)
	{
		printf("%s %u: msg is null!\n", __FILE__, __LINE__);
	}
	// printf("%s %u: msg: %s\n", __FILE__, __LINE__, msg);
	int urllen = snprintf(NULL, 0, FORMAT_URL, modemImei, msg);
	// printf("%s %u: urllen: %i\n", __FILE__, __LINE__, urllen);
	url        = static_cast<char *>(calloc(1, urllen + 2));
	snprintf(url, urllen+1, FORMAT_URL, modemImei, msg);
	printf("%s %u: url[%i] = %s\n", __FILE__, __LINE__, urllen, url);
	// printf("Modem ");
	//   print_time_now();
	// printf(" OUT: url = %s\n", url);
	//   fflush(stdout);

	tasks    = static_cast<uint8_t *>(calloc(1, 3));
	tasks[0] = TASK_URL;
	tasks[1] = TASK_POST;
	tasks[2] = TASK_NONE;
}

// // Process the serial data here
// size_t modem_process_serial()
// {
// 	uint16_t toRead = uartInterface->receive_fifo_level();
// 	Debug::log("toRead = {}", toRead);

// 	// static char buffer[255]; // input buffer
// 	char *buffer = static_cast<char *>(calloc(1, toRead + 1)); // Always have a terminating null.
// 	if(buffer == NULL) {
// 		Debug::log("Error! calloc returned NULL!");
// 		return 0;
// 	}
// 	char  *pbuffer = buffer;  // current place in buffer
// 	int    nbytes;            // bytes read so far
// 	size_t size = 0;

// 	Debug::log("buffer: {}", buffer);
// 	Debug::log("pbuffer: {}", pbuffer);

// 	while ((nbytes = modem_serial_read(pbuffer, toRead - (pbuffer - buffer))) > 0)
// 	{
// 		Debug::log("We read {} bytes this time.", nbytes);
// 		pbuffer += nbytes;
// 		if ((pbuffer[-1] == '\n') || (pbuffer[-1] == '\r'))
// 		{
// 			*pbuffer = '\0';
// 			// printf("pbuffer[-2] = 0x%02x\n", pbuffer[-2]);
// 			// printf("pbuffer[-1] = 0x%02x\n", pbuffer[-1]);
// 			// printf("pbuffer[0] = 0x%02x\n", pbuffer[0]);

// 			printf("buffer: %.*s\n", strnlen(buffer, sizeof(buffer) - 1), buffer);
// 			// if(strnlen(buffer, sizeof(buffer) - 1) > 1)
// 			// printf("buffer: %.*s\n", (int)strnlen(buffer, toRead), buffer);
// 			if (strnlen(buffer, toRead) > 1)
// 			{
// 				// printf("buffer: %s", buffer);

// 				printf("Modem ");
// 				// print_time_now();
// 				printf(" IN (serial): buffer = %s\n", buffer);
// 				// fflush(stdout);

// 				size = strnlen(buffer, toRead);
// 				process_serial_replies(buffer, size);
// 			}
// 			pbuffer = buffer; // Reset line pointer
// 			                  // break;
// 		}
// 	}
// 	if (nbytes == 0)
// 	{
// 		if ((pbuffer - buffer) >= toRead - 1)
// 		{
// 			pbuffer = buffer; // Discard the excess bytes here.
// 		}
// 		else
// 		{
// 			printf("Waste! nbytes = %i\n", nbytes);
// 		}
// 	}
// 	free(buffer);
// 	return size;
// }

// // serial_read
// // Blocking function. Reads len bytes from the serial input. You should
// // check the fifo size to see how many bytes there are to read before
// // calling this function.
// // Param 1: out: buffer to write to
// // Param 2: in: number of bytes to write to
// // returns bytes read.
// int modem_serial_read(char *msg, size_t len)
// {
// 	size_t idx = 0;
// 	if (uartInterface == NULL)
// 	{
// 		return -1;
// 	}
// 	while (idx < len)
// 	{
// 		while (uartInterface->can_read() == false)
// 		{
// 			// Block until there is something to read.
// 			// You should check the fifo size before reading to avoid this!
// 		}
// 		msg[idx] = uartInterface->blocking_read();
// 		idx++;
// 	}
// 	return idx;
// }

// uint16_t modem_rx_buff_len()
// {
// 	return uartInterface->receive_fifo_level();
// }

// // serial_send
// // Blocking function. Sends len bytes from the buffer pointed to by msg.
// // Param 1: in: Buffer with data to send.
// // Param 2: in: length of data to send.
// // returns bytes written to the serial.
// int modem_serial_send(const char *msg, size_t len)
// {
// 	if (uartInterface == NULL)
// 	{
// 		return -1;
// 	}
// 	for (size_t i = 0; i < len; i++)
// 	{
// 		uartInterface->blocking_write(msg[i]);
// 	}
// 	return len;
// }

