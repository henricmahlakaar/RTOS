#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <string.h>

// Tässä 1p suoritus 5vk tehtävästä, lisätään ominaisuuksia mahdollisuuksien mukaan.
// Tekijä: Henric M. ja Jere K.

// Defines
#define STACKSIZE 500
#define PRIORITY 5
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
//#define DEBUG
#define LINE_BUF_SIZE 32
#define ALARM_LED_ON_MS 500

#define TIME_LEN_ERROR -1
#define TIME_ARRAY_ERROR -2
#define TIME_VALUE_ERROR -3


// Ledit
static const struct gpio_dt_spec red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue  = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Taskit
void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);
static void alarm_off_handler(struct k_timer *timer);
static void alarm_handler(struct k_timer *timer);

// Threadit
K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task,    NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task,  NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task,       NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dispatcher_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_TIMER_DEFINE(alarm_off_timer, alarm_off_handler, NULL);
K_TIMER_DEFINE(alarm_timer, alarm_handler, NULL);

K_SEM_DEFINE(red_sem, 0,1);
K_SEM_DEFINE(yellow_sem, 0,1);
K_SEM_DEFINE(green_sem, 0,1);
K_SEM_DEFINE(done_sem, 0,1);

// FIFO-puskuri vastaanotetulle sekvenssille
K_FIFO_DEFINE(data_fifo);

static uint64_t sequence_total = 0;

// Aika sekvenssin käsittelyyn
int time_parse(char *time) {
 
	// how many seconds, default returns error
	int seconds = TIME_LEN_ERROR;
 
	// Check that string is not null
	if (time == NULL) {
		return TIME_ARRAY_ERROR;
	}
	// Tarkistaa että merkkijonon pituus on 6 merkkiä (hhmmss)
	if (strlen(time) != 6) {
		return TIME_LEN_ERROR;
	}
 
	// Parse values from time string
	// For example: 124033 -> 12hour 40min 33sec
	int values[3];
	values[2] = atoi(time+4); // seconds
	time[4] = 0;
	values[1] = atoi(time+2); // minutes
	time[2] = 0;
	values[0] = atoi(time); // hours
 
	// Boundary check: below zero or above limit not allowed
	if (values[0] < 0 || values[0] > 23 ||
		values[1] < 0 || values[1] > 59 ||
		values[2] < 0 || values[2] > 59) {
		return TIME_VALUE_ERROR;
	}
 
	seconds = values[0] * 3600 + values[1] * 60 + values[2];
 
	return seconds;
}

static void alarm_off_handler(struct k_timer *timer)
{
	gpio_pin_set_dt(&red, 0);
}

static void alarm_handler(struct k_timer *timer)
{
	printk("Timer interrupt: red on\n");
	gpio_pin_set_dt(&red, 1);
	k_timer_start(&alarm_off_timer, K_MSEC(ALARM_LED_ON_MS), K_NO_WAIT);
}


// FIFO:n datatyyppi
struct data_t {
	void *fifo_reserved;
	char color;
};

//Alustukset
 
int init_uart(void)
{
	if (!device_is_ready(uart_dev)) {
		return 1;
	}
	return 0;
}

int init_led(void)
{
	
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	ret |= gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
	ret |= gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);

	if (ret < 0) {
		printk("Error: Led configure failed\n");
		return ret;
	}

	
	gpio_pin_set_dt(&red, 0);
	gpio_pin_set_dt(&blue, 0);
	gpio_pin_set_dt(&green, 0);

	printk("Led initialized ok\n");

	return 0;
}

void add_sequence_time(uint64_t task_ns) {
	sequence_total += task_ns;
}

void print_sequence_time(void) {
	printk("Total sequence time: %lld.%03lld ms\n", sequence_total / 1000000, sequence_total % 1000000 / 1000);
	sequence_total = 0;
}
// Käsittelee yhden Enterillä päätetyn rivin
static void handle_line(char *line)
{
	if ((line[0] >= '0' && line[0] <= '9') || line[0] == '-') {
		// Aikamerkkijono -> parseri -> ajastin
		printk("Time string received: %s\n", line);
		int sec = time_parse(line);
		if (sec >= 0) {
			k_timer_start(&alarm_timer, K_SECONDS(sec), K_NO_WAIT);
			printk("Timer started: interrupt in %d s\n", sec);
		} else {
			printk("Time parse error: %d\n", sec);
		}
	} else {
		// Liikennevalosekvenssi
		for (int i = 0; line[i] != '\0'; i++) {
			char c = line[i];
			if (c == 'R' || c == 'Y' || c == 'G') {
				struct data_t *data = k_malloc(sizeof(struct data_t));
				if (data == NULL) {
					printk("Malloc failed\n");
					return;
				}
				data->color = c;
				k_fifo_put(&data_fifo, data);
				printk("Data sent to fifo: %c\n", c);
			} else {
				printk("Unknown char: %c\n", c);
			}
		}
	}
}

// UART taski
static void uart_task(void *, void *, void *)
{
	char c = 0;
	char line[LINE_BUF_SIZE];
	int len = 0;

	printk("Uart thread started\n");
	while (true) {
		if (uart_poll_in(uart_dev, &c) == 0) {
			if (c == '\r' || c == '\n') {
				if (len > 0) {
					line[len] = '\0';
					printk("\n");
					handle_line(line);
					len = 0;
				}
			} else if (len < LINE_BUF_SIZE - 1) {
				line[len++] = c;
				uart_poll_out(uart_dev, c);
			}
		}
		k_msleep(10);
	}
}

static void dispatcher_task(void *, void *, void *)
{
	printk("Dispatcher thread started\n");
	while (true) {
		struct data_t *data = k_fifo_get(&data_fifo, K_FOREVER);
		if (data != NULL) {
			switch (data->color) {
				case 'R':
					k_sem_give(&red_sem);
					break;
				case 'Y':
					k_sem_give(&yellow_sem);
					break;
				case 'G':
					k_sem_give(&green_sem);
					break;
				default:
					printk("Unknown color: %c\n", data->color);
					break;
			}
			k_free(data);
			k_sem_take(&done_sem, K_FOREVER); 
			if (k_fifo_is_empty(&data_fifo)) {
				print_sequence_time();
			}
		}
	}
}

//Led taskit
void red_led_task(void *, void *, void *)
{
	printk("Red led thread started\n");
	while (true) {
		k_sem_take(&red_sem, K_FOREVER);
		timing_start();
		timing_t red_start_time = timing_counter_get();
		gpio_pin_set_dt(&red, 1);
		printk("Red on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&red, 0);
		printk("Red off\n");
		k_sem_give(&done_sem); 

		timing_t red_end_time = timing_counter_get();
		timing_stop();
		uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time));
		printk("Red led task time: %lld.%03lld ms\n", timing_ns / 1000000, timing_ns % 1000000 / 1000);
		add_sequence_time(timing_ns);

	}
}

void yellow_led_task(void *, void *, void *)
{
	printk("Yellow led thread started\n");
	while (true) {
		k_sem_take(&yellow_sem, K_FOREVER);
		timing_start();
		timing_t yellow_start_time = timing_counter_get();
		gpio_pin_set_dt(&red, 1);
		gpio_pin_set_dt(&green, 1);
		printk("Yellow on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&red, 0);
		gpio_pin_set_dt(&green, 0);
		printk("Yellow off\n");
		k_sem_give(&done_sem); 

		timing_t yellow_end_time = timing_counter_get();
		timing_stop();
		uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time));
		printk("Yellow led task time: %lld.%03lld ms\n", timing_ns / 1000000, timing_ns % 1000000 / 1000);
		add_sequence_time(timing_ns);
	}
}

void green_led_task(void *, void *, void *)
{
	printk("Green led thread started\n");
	while (true) {
		k_sem_take(&green_sem, K_FOREVER);
		timing_start();
		timing_t green_start_time = timing_counter_get();
		gpio_pin_set_dt(&green, 1);
		printk("Green on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&green, 0);
		printk("Green off\n");
		k_sem_give(&done_sem); 
		timing_t green_end_time = timing_counter_get();
		timing_stop();
		uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time));
		printk("Green led task time: %lld.%03lld ms\n", timing_ns / 1000000, timing_ns % 1000000 / 1000);
		add_sequence_time(timing_ns);
	}
}



int main(void)
{
	timing_init();
	timing_start();
	timing_t start_time = timing_counter_get();

	init_led();

	if (init_uart() != 0) {
		printk("UART INIT FAILED\n");
		return 1;
	}

	timing_t end_time = timing_counter_get();
	timing_stop();
	uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time));
	printk("Initialization time: %lld.%03lld ms\n", timing_ns / 1000000, timing_ns % 1000000 / 1000);
	return 0;
}