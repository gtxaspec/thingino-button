#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <ctype.h>

#define CONFIG_FILE "/etc/thingino-button.conf"
#define DEFAULT_DEVICE "/dev/input/event0"
#define MAX_CONFIGS 100
#define ACTION_PRESS "PRESS"
#define ACTION_RELEASE "RELEASE"
#define ACTION_TIMED "TIMED"
#define ACTION_TIMED_FIRE "TIMED_FIRE"
#define EV_KEY 0x01

typedef struct {
	int key_code;
	char action[20];
	char command[256];
	double time;
} Config;

typedef struct {
	struct timeval press_time;
	Config *config;
	int active;
} TimedFireEntry;

Config configs[MAX_CONFIGS];
int config_count = 0;
char input_device[256] = DEFAULT_DEVICE;

// Global variables
int silent_mode = 0;
int daemon_mode = 0;

struct input_event {
	struct timeval time;
	uint16_t type;
	uint16_t code;
	int32_t value;
};

typedef struct {
	TimedFireEntry entries[MAX_CONFIGS];
	int count;
} TimedFireList;

TimedFireList timed_fires[256] = {0};

void log_message(const char *format, ...) {
	va_list args;
	va_start(args, format);
	if (silent_mode || daemon_mode) {
		char buf[512];
		vsnprintf(buf, sizeof(buf), format, args);
		syslog(LOG_NOTICE, "%s", buf);
	} else {
		vprintf(format, args);
	}
	va_end(args);
}

// Support 0-9, MINUS, and ENTER
int event_code_from_name(const char *name) {
	switch (name[4]) { // name' is in the format "KEY_X" where X is a character
		case 'E':
			if (strcmp(name, "KEY_ENTER") == 0) return 28;
			break;
		case '1':
			if (strcmp(name, "KEY_1") == 0) return 2;
			break;
		case '2':
			if (strcmp(name, "KEY_2") == 0) return 3;
			break;
		case '3':
			if (strcmp(name, "KEY_3") == 0) return 4;
			break;
		case '4':
			if (strcmp(name, "KEY_4") == 0) return 5;
			break;
		case '5':
			if (strcmp(name, "KEY_5") == 0) return 6;
			break;
		case '6':
			if (strcmp(name, "KEY_6") == 0) return 7;
			break;
		case '7':
			if (strcmp(name, "KEY_7") == 0) return 8;
			break;
		case '8':
			if (strcmp(name, "KEY_8") == 0) return 9;
			break;
		case '9':
			if (strcmp(name, "KEY_9") == 0) return 10;
			break;
		case '0':
			if (strcmp(name, "KEY_0") == 0) return 11;
			break;
		case 'M':
			if (strcmp(name, "KEY_MINUS") == 0) return 12;
			break;
		default:
			break;
	}
	return -1;
}

void load_config() {
	FILE *file = fopen(CONFIG_FILE, "r");
	if (!file) {
		perror("Failed to open config file");
		exit(EXIT_FAILURE);
	}

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		// Remove trailing newline character from the line
		line[strcspn(line, "\n")] = '\0';

		if (strlen(line) == 0 || line[0] == '#') {
			continue;
		}

		if (strncmp(line, "DEVICE=", 7) == 0) {
			sscanf(line, "DEVICE=%255s", input_device);
		} else {
			Config config;
			char key[20], action[20], command[256];
			double time = 0.0;

			// Try to parse with time value
			int parsed = sscanf(line, "%19s %19s %lf %[^\n]", key, action, &time, command);

			// If time was not found, parse without it
			if (parsed < 4) {
				time = 0.0; // Default time if not provided
				parsed = sscanf(line, "%19s %19s %[^\n]", key, action, command);
			}

			if (parsed >= 3) {
				config.key_code = event_code_from_name(key);
				if (config.key_code == -1) {
					log_message("Invalid key code: %s\n", key);
					continue;
				}

				strncpy(config.action, action, sizeof(config.action) - 1);
				config.action[sizeof(config.action) - 1] = '\0';
				strncpy(config.command, command, sizeof(config.command) - 1);
				config.command[sizeof(config.command) - 1] = '\0';
				config.time = time;

				// Debug output for parsed config
				log_message("Loaded config: key_code=%d, action=%s, time=%f, command=%s\n",
							config.key_code, config.action, config.time, config.command);

				configs[config_count++] = config;
				if (config_count >= MAX_CONFIGS) {
					log_message("Maximum number of configurations reached.\n");
					break;
				}
			} else {
				log_message("Failed to parse line: %s\n", line);
			}
		}
	}
	fclose(file);
}


void execute_command(const char *command) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("Failed to fork");
		return;
	}
	if (pid == 0) {
		// Debug output for command execution
		log_message("Executing command: [%s]\n", command);

		// Execute command using shell
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		perror("execl failed");
		exit(EXIT_FAILURE);
	}
}

void process_events(int fd) {
	struct input_event ev;
	struct timeval press_times[256] = {{0, 0}};

	while (1) {
		char buf[16];
		int n = read(fd, buf, sizeof(buf));
		if (n == 16) {
			ev.time.tv_sec = *(int32_t *)(buf);
			ev.time.tv_usec = *(int32_t *)(buf + 4);
			ev.type = *(uint16_t *)(buf + 8);
			ev.code = *(uint16_t *)(buf + 10);
			ev.value = *(int32_t *)(buf + 12);

			if (ev.type == EV_KEY) {
				// log_message("Key event: code %d, value %d\n", ev.code, ev.value);
				if (ev.value == 1) { // Key press
					log_message("Detected keycode %d\n", ev.code);
					gettimeofday(&press_times[ev.code], NULL);
					for (int i = 0; i < config_count; i++) {
						Config *config = &configs[i];
						if (ev.code == config->key_code && strcmp(config->action, ACTION_PRESS) == 0) {
							log_message("PRESS command for key %d: %s\n", ev.code, config->command);
							execute_command(config->command);
						} else if (ev.code == config->key_code && strcmp(config->action, ACTION_TIMED) == 0) {
							gettimeofday(&press_times[ev.code], NULL);
							// log_message("Press time recorded for key code %d\n", ev.code);
						} else if (ev.code == config->key_code && strcmp(config->action, ACTION_TIMED_FIRE) == 0) {
							TimedFireEntry entry = {0};
							gettimeofday(&entry.press_time, NULL);
							entry.config = config;
							entry.active = 1;
							timed_fires[ev.code].entries[timed_fires[ev.code].count++] = entry;
							// log_message("TIMED_FIRE started for key %d\n", ev.code);
						}
					}
				} else if (ev.value == 0) { // Key release
					struct timeval release_time;
					gettimeofday(&release_time, NULL);
					double hold_time = (release_time.tv_sec - press_times[ev.code].tv_sec) +
									(release_time.tv_usec - press_times[ev.code].tv_usec) / 1000000.0;
					// log_message("Key %d held for %f seconds\n", ev.code, hold_time);
					// Find and execute the longest TIMED command that fits within the hold time
					double max_time = 0;
					Config *selected_config = NULL;
					for (int i = 0; i < config_count; i++) {
						Config *config = &configs[i];
						if (ev.code == config->key_code && strcmp(config->action, ACTION_TIMED) == 0) {
							if (hold_time >= config->time && config->time > max_time) {
								max_time = config->time;
								selected_config = config;
							}
						}
					}
					if (selected_config) {
						log_message("TIMED command for key %d: %s (held for %f seconds)\n", ev.code, selected_config->command, hold_time);
						execute_command(selected_config->command);
					}

					// Execute RELEASE commands
					for (int i = 0; i < config_count; i++) {
						Config *config = &configs[i];
						if (ev.code == config->key_code && strcmp(config->action, ACTION_RELEASE) == 0) {
							log_message("RELEASE command for key %d: %s\n", ev.code, config->command);
							execute_command(config->command);
						}
					}

					// Deactivate all TIMED_FIRE entries for the key
					for (int i = 0; i < timed_fires[ev.code].count; i++) {
						timed_fires[ev.code].entries[i].active = 0;
					}
				}
			}
		} else if (n == -1 && errno != EAGAIN) {
			perror("Error reading event");
			break;
		}

		// Check for timed fire events
		struct timeval now;
		gettimeofday(&now, NULL);
		for (int i = 0; i < 256; i++) {
			for (int j = 0; j < timed_fires[i].count; j++) {
				TimedFireEntry *entry = &timed_fires[i].entries[j];
				if (!entry->active) {
					continue;
				}
				double elapsed = (now.tv_sec - entry->press_time.tv_sec) +
								(now.tv_usec - entry->press_time.tv_usec) / 1000000.0;
				if (elapsed >= entry->config->time) {
					log_message("TIMED_FIRE command for key %d: %s (elapsed %f seconds)\n", i, entry->config->command, elapsed);
					execute_command(entry->config->command);
					entry->active = 0; // Deactivate the entry after execution
				}
			}
		}

		usleep(10000); // Sleep for 10ms to reduce CPU usage
	}
}

void handle_signal(int signal) {
	switch (signal) {
		case SIGTERM:
			log_message("Received SIGTERM, exiting...\n");
			if (silent_mode || daemon_mode) {
				closelog();
			}
			exit(EXIT_SUCCESS);
			break;
		// Handle other signals if needed
	}
}

void daemonize() {
	pid_t pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	// On success: The child process becomes session leader
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	// Catch, ignore and handle signals
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	// Fork off for the second time
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	// Success? Let the parent terminate
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	// Why not
	umask(0);

	for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
		close(x);
	}

	openlog("thingino-button", LOG_PID, LOG_DAEMON);
}

int main(int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "sd")) != -1) {
		switch (opt) {
			case 's':
				silent_mode = 1;
				openlog("thingino-button", LOG_PID | LOG_CONS, LOG_DAEMON);
				setlogmask(LOG_UPTO(LOG_DEBUG)); // Ensure all messages are logged
				break;
			case 'd':
				daemon_mode = 1;
				daemonize();
				silent_mode = 1;
				openlog("thingino-button", LOG_PID | LOG_CONS, LOG_DAEMON);
				setlogmask(LOG_UPTO(LOG_DEBUG)); // Ensure all messages are logged
				break;
			default:
				fprintf(stderr, "Usage: %s [-s] [-d] [input_device]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	log_message("thingino-button started\n");

	if (silent_mode || daemon_mode) {
		log_message("Starting in silent mode, logging to syslog\n");
	} else {
		printf("Using input device: %s\n", input_device);
	}

	if (optind < argc) {
		strncpy(input_device, argv[optind], sizeof(input_device) - 1);
		input_device[sizeof(input_device) - 1] = '\0';
	} else {
		load_config();
	}

	// Register signal handlers
	signal(SIGTERM, handle_signal);

	int fd = open(input_device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("Failed to open event device");
		return 1;
	}

	if (!silent_mode && !daemon_mode) {
		printf("Input device opened successfully.\n");
	} else {
		log_message("Input device opened successfully.\n");
	}

	process_events(fd);

	close(fd);

	if (silent_mode || daemon_mode) {
		closelog();
	}

	return 0;
}
