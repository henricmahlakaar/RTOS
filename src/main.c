#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

// Tässä 1p suoritus 2vk tehtävästä, lisätään ominaisuuksia mahdollisuuksien mukaan.
// Tekijä: Henric M. ja Jere K.

// Defines
#define STACKSIZE 500
#define PRIORITY 5
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

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

// Threadit
K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task,    NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task,  NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task,       NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dispatcher_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);

K_SEM_DEFINE(red_sem, 0,1);
K_SEM_DEFINE(yellow_sem, 0,1);
K_SEM_DEFINE(green_sem, 0,1);
K_SEM_DEFINE(done_sem, 0,1);

// FIFO-puskuri vastaanotetulle sekvenssille
K_FIFO_DEFINE(data_fifo);

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
	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	ret |= gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
	ret |= gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);

	if (ret < 0) {
		printk("Error: Led configure failed\n");
		return ret;
	}

	// set leds off
	gpio_pin_set_dt(&red, 0);
	gpio_pin_set_dt(&blue, 0);
	gpio_pin_set_dt(&green, 0);

	printk("Led initialized ok\n");

	return 0;
}

// UART task - lukee sekvenssin sarjaportista FIFO-puskuriin
static void uart_task(void *, void *, void *)
{
	char c = 0;

	printk("Uart thread started\n");
	while (true) {
		if (uart_poll_in(uart_dev, &c) == 0) {
			if (c == 'R' || c == 'Y' || c == 'G') {
				struct data_t *data = k_malloc(sizeof(struct data_t));
				if (data == NULL) {
					printk("Malloc failed\n");
					continue;
				}
				data->color = c;
				k_fifo_put(&data_fifo, data);
				printk("Data sent to fifo: %c\n", c);
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
			k_sem_take(&done_sem, K_FOREVER); // Wait for the led task to signal completion
		}
	}
}

//Led taskit
void red_led_task(void *, void *, void *)
{
	printk("Red led thread started\n");
	while (true) {
		k_sem_take(&red_sem, K_FOREVER);
		gpio_pin_set_dt(&red, 1);
		printk("Red on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&red, 0);
		printk("Red off\n");
		k_sem_give(&done_sem); // Signal that red led task is done
	}
}

void yellow_led_task(void *, void *, void *)
{
	printk("Yellow led thread started\n");
	while (true) {
		k_sem_take(&yellow_sem, K_FOREVER);
		gpio_pin_set_dt(&red, 1);
		gpio_pin_set_dt(&green, 1);
		printk("Yellow on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&red, 0);
		gpio_pin_set_dt(&green, 0);
		printk("Yellow off\n");
		k_sem_give(&done_sem); // Signal that yellow led task is done
	}
}

void green_led_task(void *, void *, void *)
{
	printk("Green led thread started\n");
	while (true) {
		k_sem_take(&green_sem, K_FOREVER);
		gpio_pin_set_dt(&green, 1);
		printk("Green on\n");
		k_sleep(K_SECONDS(1));

		gpio_pin_set_dt(&green, 0);
		printk("Green off\n");
		k_sem_give(&done_sem); // Signal that green led task is done
	}
}

int main(void)
{
	init_led();

	if (init_uart() != 0) {
		printk("UART INIT FAILED\n");
		return 1;
	}

	return 0;
}