#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>

// Tässä 1p suoritus 3vk tehtävästä, lisätään ominaisuuksia mahdollisuuksien mukaan.
// Tekijä: Henric M. ja Jere K.

#define STACKSIZE 500
#define PRIORITY 5
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define DEBUG


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
K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task,
	NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task,
	NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task,
	NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task,
	NULL, NULL, NULL, PRIORITY, 0, 0);

K_THREAD_DEFINE(dispatcher_thread, STACKSIZE, dispatcher_task,
	NULL, NULL, NULL, PRIORITY, 0, 0);


// Semaphoret
K_SEM_DEFINE(red_sem, 0, 1);
K_SEM_DEFINE(yellow_sem, 0, 1);
K_SEM_DEFINE(green_sem, 0, 1);
K_SEM_DEFINE(done_sem, 0, 1);


// FIFO-puskuri vastaanotetulle sekvenssille
K_FIFO_DEFINE(data_fifo);


// Kokonaissekvenssin aika
static uint64_t sequence_total = 0;


// FIFO:n datatyyppi
struct data_t {
	void *fifo_reserved;
	char color;
};


// Alustukset

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

	// Set leds off
	gpio_pin_set_dt(&red, 0);
	gpio_pin_set_dt(&blue, 0);
	gpio_pin_set_dt(&green, 0);

	printk("Led initialized ok\n");

	return 0;
}


// Lisää taskin suoritusajan kokonaisaikaan
void add_sequence_time(uint64_t task_ns)
{
	sequence_total += task_ns;
}


// Tulostaa kokonaissekvenssin ajan
void print_sequence_time(void)
{
	printk(
		"Total sequence time: %lld.%03lld ms\n",
		sequence_total / 1000000,
		sequence_total % 1000000 / 1000
	);

	sequence_total = 0;
}


// UART task - lukee sekvenssin sarjaportista FIFO-puskuriin
static void uart_task(void *, void *, void *)
{
	char c = 0;

	printk("Uart thread started\n");

	while (true) {

		if (uart_poll_in(uart_dev, &c) == 0) {

			if (c == 'R' || c == 'Y' || c == 'G') {

				struct data_t *data =
					k_malloc(sizeof(struct data_t));

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


// Dispatcher task
static void dispatcher_task(void *, void *, void *)
{
	printk("Dispatcher thread started\n");

	while (true) {

		struct data_t *data =
			k_fifo_get(&data_fifo, K_FOREVER);

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
					printk(
						"Unknown color: %c\n",
						data->color
					);
					break;
			}

			k_free(data);

			// Odotetaan LED-taskin valmistumista
			k_sem_take(&done_sem, K_FOREVER);

			// Jos FIFO on tyhjä, tulostetaan kokonaisaika
			if (k_fifo_is_empty(&data_fifo)) {
				print_sequence_time();
			}
		}
	}
}

void red_led_task(void *, void *, void *)
{
	printk("Red led thread started\n");

	while (true) {

		k_sem_take(&red_sem, K_FOREVER);

		timing_start();

		timing_t red_start_time =
			timing_counter_get();


		gpio_pin_set_dt(&red, 1);

		printk("Red on\n");

		k_sleep(K_SECONDS(1));


		gpio_pin_set_dt(&red, 0);

		printk("Red off\n");


		// Lopetetaan ajan mittaus
		timing_t red_end_time =
			timing_counter_get();

		timing_stop();


		// Lasketaan taskin suoritusajan pituus
		uint64_t timing_ns =
			timing_cycles_to_ns(
				timing_cycles_get(
					&red_start_time,
					&red_end_time
				)
			);


		// Tulostetaan taskin aika
		printk(
			"Red led task time: %lld.%03lld ms\n",
			timing_ns / 1000000,
			timing_ns % 1000000 / 1000
		);


		// Lisätään aika kokonaissekvenssiin
		add_sequence_time(timing_ns);


		// Vasta nyt ilmoitetaan dispatcherille,
		// että task on kokonaan valmis
		k_sem_give(&done_sem);
	}
}

void yellow_led_task(void *, void *, void *)
{
	printk("Yellow led thread started\n");

	while (true) {

		k_sem_take(&yellow_sem, K_FOREVER);

		timing_start();

		timing_t yellow_start_time =
			timing_counter_get();


		// Yellow = red + green
		gpio_pin_set_dt(&red, 1);
		gpio_pin_set_dt(&green, 1);

		printk("Yellow on\n");

		k_sleep(K_SECONDS(1));


		gpio_pin_set_dt(&red, 0);
		gpio_pin_set_dt(&green, 0);

		printk("Yellow off\n");


		// Lopetetaan ajan mittaus
		timing_t yellow_end_time =
			timing_counter_get();

		timing_stop();


		// Lasketaan taskin suoritusajan pituus
		uint64_t timing_ns =
			timing_cycles_to_ns(
				timing_cycles_get(
					&yellow_start_time,
					&yellow_end_time
				)
			);


		// Tulostetaan taskin aika
		printk(
			"Yellow led task time: %lld.%03lld ms\n",
			timing_ns / 1000000,
			timing_ns % 1000000 / 1000
		);


		// Lisätään aika kokonaissekvenssiin
		add_sequence_time(timing_ns);


		// Ilmoitetaan dispatcherille vasta lopuksi
		k_sem_give(&done_sem);
	}
}

void green_led_task(void *, void *, void *)
{
	printk("Green led thread started\n");

	while (true) {

		k_sem_take(&green_sem, K_FOREVER);

		timing_start();

		timing_t green_start_time =
			timing_counter_get();


		gpio_pin_set_dt(&green, 1);

		printk("Green on\n");

		k_sleep(K_SECONDS(1));


		gpio_pin_set_dt(&green, 0);

		printk("Green off\n");


		// Lopetetaan ajan mittaus
		timing_t green_end_time =
			timing_counter_get();

		timing_stop();


		// Lasketaan taskin suoritusajan pituus
		uint64_t timing_ns =
			timing_cycles_to_ns(
				timing_cycles_get(
					&green_start_time,
					&green_end_time
				)
			);


		// Tulostetaan taskin aika
		printk(
			"Green led task time: %lld.%03lld ms\n",
			timing_ns / 1000000,
			timing_ns % 1000000 / 1000
		);


		// Lisätään aika kokonaissekvenssiin
		add_sequence_time(timing_ns);


		// Ilmoitetaan dispatcherille vasta lopuksi
		k_sem_give(&done_sem);
	}
}

int main(void)
{
	// Timingin alustus
	timing_init();

	timing_start();

	timing_t start_time =
		timing_counter_get();


	// LEDien alustus
	init_led();


	// UARTin alustus
	if (init_uart() != 0) {
		printk("UART INIT FAILED\n");
		return 1;
	}


	// Lopetetaan alustuksen ajan mittaus
	timing_t end_time =
		timing_counter_get();

	timing_stop();


	// Lasketaan alustuksen aika
	uint64_t timing_ns =
		timing_cycles_to_ns(
			timing_cycles_get(
				&start_time,
				&end_time
			)
		);


	printk(
		"Initialization time: %lld.%03lld ms\n",
		timing_ns / 1000000,
		timing_ns % 1000000 / 1000
	);


	return 0;
}
