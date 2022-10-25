#include <zephyr.h>
#include <sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

/* Code addons for UDP data sample */
#include <stdio.h>                  // sprintf - standard C IO calls
/* LTE Modem Header Files */
#include <modem/lte_lc.h>           // Link Control Functionality
#include <zephyr/net/socket.h>      // Zephyr Socket Functionality

/* UDP Sample Defines */
#define MY_NAME "Kevin-Nordic"
#define UDP_SERVER_ADDRESS "173.249.8.201"
#define UDP_SERVER_PORT 25000
#define UDP_TRANSMISSION_INTERVAL 30

/* Static, global definitions */
static int socket_client;
static struct sockaddr_storage host_addr;
static char data_buffer[64];

/* Modem init and connect function */
#if defined(CONFIG_NRF_MODEM_LIB)
static int modem_init_and_connect(void)
{
	int err;
    printk("Initializing modem and connecting to LTE network...\n");

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
        err = lte_lc_init_and_connect();
		if (err) {
			printk("Init/Connect to LTE network failed, error: %d\n", err);
			return err;
		}
	}
    printk("LTE connection successful.\n");
	return 0;
}
#endif

/* Init UDP Socket */
static int socket_init(void)
{
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&host_addr);

	server4->sin_family = AF_INET;
	server4->sin_port = htons(UDP_SERVER_PORT);

	inet_pton(AF_INET, UDP_SERVER_ADDRESS, &server4->sin_addr);

	return 0;
}

/* Close UDP Socket */
static void socket_close(void)
{
	(void)close(socket_client);
}

/* Connect UDP Socket */
static int socket_connect(void)
{
	int err;

	socket_client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket_client < 0) {
		printk("Failed to create UDP socket: %d\n", errno);
		err = -errno;
		goto error;
	}

	err = connect(socket_client, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("Connect failed : %d\n", errno);
		goto error;
	}

	return 0;

error:
	socket_close();

	return err;
}

/*
****************** MAIN ******************
*/

void main(void)
{
	int err;

	printk("MyProject is running!\n");

	const struct device *dev = DEVICE_DT_GET_ANY(bosch_bme280);
	if (dev==NULL){
		//No such node, or the node does not have status "okay"
		printk("\nError: Device \"%s\" is not ready.\n", dev->name);
		return;
	}

	// Modem init and connect
	err = modem_init_and_connect();
	if (err){
		return;
	}

	// Socket init and connect / open
	socket_init();
	socket_connect();

	while(1){
		
		struct sensor_value temp, press, humidity;
		
		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);
		sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
			
		printk("temp: %d.%06d; press: %d.%06d; humidity: %d.%06d\n",
					temp.val1, temp.val2, press.val1, press.val2,
					humidity.val1, humidity.val2);

		// Format sensor data, copy into transmit buffer
		temp.val2 = temp.val2/10000; //shrink fractional part to 2 digits
		sprintf(data_buffer, "[%s -- Temp: %d.%02d C]", MY_NAME, temp.val1, temp.val2);
		
		/* Transmit via UDP data socket */
		err = send(socket_client, data_buffer, strlen(data_buffer), 0);
		if (err < 0) {
			printk("Failed to transmit UDP packet, %d\n", errno);
		}else{
			printk("UDP data sent, content: %s\n", data_buffer);
		}

		k_sleep(K_SECONDS(UDP_TRANSMISSION_INTERVAL));
	}
}